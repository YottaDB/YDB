/***************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_reorg.c:
 *	Main program for mupip reorg (portable) .
 *
 *	This program creates a list of globals to be organized from /SELECT option and then calls mu_reorg() to
 *	reorganize each global seperately but excludes some variables' organization given in /EXCLUDE list.
 *
 *	This alternatively invokes mu_reorg_upgrd_dwngrd in case a MUPIP REORG -UPGRADE or -DOWNGRADE was specified
 */

#include "mdef.h"

#include "gtm_string.h"

#include "stp_parms.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "muextr.h"
#include "iosp.h"
#include "cli.h"
#include "mu_reorg.h"
#include "util.h"
#include "mu_truncate.h"
#include "op.h"
#include "tp_change_reg.h"
#include "is_proc_alive.h"
#include "filestruct.h"
#include "error.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#ifdef GTM_TRIGGER
#include "hashtab_mname.h"
#include "gv_trigger.h"
#include "gv_trigger_common.h"
#endif

/* Prototypes */
#include "mupip_reorg.h"
#include "mu_reorg_upgrd_dwngrd.h"
#include "targ_alloc.h"
#include "mupip_exit.h"
#include "gv_select.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "mu_getlst.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "mupip_reorg_encrypt.h"

error_def(ERR_CONCURTRUNCINPROG);
error_def(ERR_DBRDONLY);
error_def(ERR_EXCLUDEREORG);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUTRUNCFAIL);
error_def(ERR_MUTRUNCPERCENT);
error_def(ERR_MUTRUNC1ATIME);
error_def(ERR_NOSELECT);
error_def(ERR_NOEXCLUDE);
error_def(ERR_REORGCTRLY);
error_def(ERR_REORGINC);

GBLREF	bool			error_mupip;
GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	boolean_t		mu_reorg_process;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey_next_reorg, *gv_currkey, *gv_altkey;
GBLREF	gv_namehead		*reorg_gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	int			gv_keysize;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	tp_region		*grlist;
GBLREF	uint4			process_id;

static readonly mval literal_poollimit =
	DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, (SIZEOF("POOLLIMIT") - 1), "POOLLIMIT", 0, 0);

void mupip_reorg(void)
{
	boolean_t		resume, reorg_success = TRUE;
	int			data_fill_factor, index_fill_factor;
	int			reorg_op, reg_max_rec, reg_max_key, reg_max_blk, status;
	char			cli_buff[MAX_LINE], *ptr;
	glist			gl_head, exclude_gl_head, *gl_ptr, hasht_gl;
	uint4			cli_status;
	mval			reg_nam_mval = DEFINE_MVAL_STRING(MV_STR, 0 , 0 , SIZEOF(MAX_RN_LEN), 0, 0, 0);
	unsigned short		n_len;
	boolean_t		truncate, cur_success, restrict_reg, arg_present;
	int			root_swap_statistic;
	int4			truncate_percent;
	boolean_t		gotlock;
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;
	trunc_region		*reg_list, *tmp_reg, *reg_iter, *prev_reg;
	uint4			fs;
	uint4			lcl_pid;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jnlpool_init_needed = TRUE;
	mu_outofband_setup();
	truncate = FALSE;
	reg_list = NULL;
	if (CLI_PRESENT == cli_present("TRUNCATE"))
		truncate = TRUE;
	if ((CLI_PRESENT == cli_present("UPGRADE")) || (CLI_PRESENT == cli_present("DOWNGRADE")))
	{	/* Note that "mu_reorg_process" is not set to TRUE in case of MUPIP REORG -UPGRADE/DOWNGRADE.
		 * This is intentional because we are not doing any REORG kind of processing.
		 */
		mu_reorg_upgrd_dwngrd();
		mupip_exit(SS_NORMAL);	/* does not return */
	} else if (CLI_PRESENT == cli_present("ENCRYPT"))
	{	/* Note that "mu_reorg_process" is not set to TRUE in case of MUPIP REORG -ENCRYPT.
		 * This is intentional because we are not doing any REORG kind of processing.
		 */
		mupip_reorg_encrypt();
		mupip_exit(SS_NORMAL);	/* does not return */
	}
	grlist = NULL;
	restrict_reg = FALSE;
	arg_present = (0 != TREF(parms_cnt));
	VMS_ONLY(arg_present = (CLI_PRESENT == cli_present("REG_NAME")));
	if (CLI_PRESENT == cli_present("REGION"))
	{ /* MUPIP REORG -REGION reg-list restricts mu_reorg to variables in specified regions */
		error_mupip = FALSE;
		restrict_reg = TRUE;
		gvinit();	/* initialize gd_header (needed by the following call to mu_getlst) */
		mu_getlst("REG_NAME", SIZEOF(tp_region)); /* get parm for REGION qualifier */
		if (error_mupip)
		{
			util_out_print("!/MUPIP REORG cannot proceed with above errors!/", FLUSH);
			mupip_exit(ERR_MUNOACTION);
		}
	} else if (arg_present)
	{
		util_out_print("MUPIP REORG only accepts a parameter when -REGION is specified.", FLUSH);
		mupip_exit(ERR_MUPCLIERR);
	}

	resume = (CLI_PRESENT == cli_present("RESUME"));
	reorg_op = DEFAULT;
	n_len = SIZEOF(cli_buff);
	memset(cli_buff, 0, n_len);
	if (CLI_PRESENT == cli_present("USER_DEFINED_REORG") && (CLI_GET_STR_ALL("USER_DEFINED_REORG", cli_buff, &n_len)))
	{
		for (ptr = cli_buff; ; )
		{
			if (0 == STRNCMP_LIT(ptr, "SWAPHIST"))
				reorg_op |= SWAPHIST;
			else if (0 == STRNCMP_LIT(ptr, "NOCOALESCE"))
				reorg_op |= NOCOALESCE;
			else if (0 == STRNCMP_LIT(ptr, "NOSPLIT"))
				reorg_op |= NOSPLIT;
			else if (0 == STRNCMP_LIT(ptr, "NOSWAP"))
				reorg_op |= NOSWAP;
			else if (0 == STRNCMP_LIT(ptr, "DETAIL"))
				reorg_op |= DETAIL;
			ptr  = (char *)strchr(ptr, ',');
			if (ptr)
				ptr++;
			else
				break;
		}
	}
	if ((cli_status = cli_present("FILL_FACTOR")) == CLI_PRESENT)
	{
		assert(SIZEOF(data_fill_factor) == SIZEOF(int4));
		if (!cli_get_int("FILL_FACTOR", (int4 *)&data_fill_factor) || MAX_FILLFACTOR < data_fill_factor)
			data_fill_factor = MAX_FILLFACTOR;
		else if (MIN_FILLFACTOR > data_fill_factor)
			data_fill_factor = MIN_FILLFACTOR;
	} else
		data_fill_factor = MAX_FILLFACTOR;
	if ((cli_status = cli_present("INDEX_FILL_FACTOR")) == CLI_PRESENT)
	{
		assert(SIZEOF(index_fill_factor) == SIZEOF(int4));
		if (!cli_get_int("INDEX_FILL_FACTOR", (int4 *)&index_fill_factor))
			index_fill_factor = data_fill_factor;
		else if (MIN_FILLFACTOR > index_fill_factor)
			index_fill_factor = MIN_FILLFACTOR;
		else if (MAX_FILLFACTOR < index_fill_factor)
			index_fill_factor = MAX_FILLFACTOR;
	} else
		index_fill_factor = data_fill_factor;
	util_out_print("Fill Factor:: Index blocks !UL%: Data blocks !UL%", FLUSH, index_fill_factor, data_fill_factor);

	n_len = SIZEOF(cli_buff);
	memset(cli_buff, 0, n_len);
	if (CLI_PRESENT != cli_present("EXCLUDE"))
		exclude_gl_head.next = NULL;
	else if (FALSE == CLI_GET_STR_ALL("EXCLUDE", cli_buff, &n_len))
		exclude_gl_head.next = NULL;
	else
	{
		/* gv_select will select globals for this clause */
		gv_select(cli_buff, n_len, FALSE, "EXCLUDE", &exclude_gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk, FALSE);
		if (!exclude_gl_head.next)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOEXCLUDE);
	}
	n_len = SIZEOF(cli_buff);
	memset(cli_buff, 0, n_len);
	if (CLI_PRESENT != cli_present("SELECT"))
	{
		n_len = 1;
                cli_buff[0] = '*';
	} else if (FALSE == CLI_GET_STR_ALL("SELECT", cli_buff, &n_len))
	{
		n_len = 1;
                cli_buff[0] = '*';
	}
	/* gv_select will select globals for this clause */
	TREF(want_empty_gvts) = TRUE; /* Allow killed globals to be selected and processed by mu_reorg */
	gv_select(cli_buff, n_len, FALSE, "SELECT", &gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk, restrict_reg);
	if (!gl_head.next)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSELECT);
		mupip_exit(ERR_NOSELECT);
	}
	TREF(want_empty_gvts) = FALSE;

	mu_reorg_process = TRUE;
	assert(NULL == gv_currkey_next_reorg);
	GVKEYSIZE_INIT_IF_NEEDED;	/* sets "gv_keysize", "gv_currkey" and "gv_altkey" (if not already done) */
	GVKEY_INIT(gv_currkey_next_reorg, gv_keysize);
	reorg_gv_target = targ_alloc(MAX_KEY_SZ, NULL, NULL);
	reorg_gv_target->hist.depth = 0;
	reorg_gv_target->alt_hist->depth = 0;
	for (gl_ptr = gl_head.next; gl_ptr; gl_ptr = gl_ptr->next)
	{
		/* mu_reorg_process can't be set before gv_select above as the assert(!mu_reorg_process) fails in gvcst_search.
		 * The option to set mu_reorg_process before calling gv_select and clearing it after being used in gvcst_init,
		 *  can't be taken up as gvcst_init is called multiple times, once for each region.
		 * This means gvcst_init() doesn't set csa->gbuff and we need to set csa->gbuff_limit explicitly now. */
		if (0 == (TREF(gbuff_limit)).str.len)
		{
			(TREF(gbuff_limit)).str.len = SIZEOF(REORG_GBUFF_LIMIT);
			(TREF(gbuff_limit)).str.addr = malloc(SIZEOF(REORG_GBUFF_LIMIT));
			memcpy((TREF(gbuff_limit)).str.addr, REORG_GBUFF_LIMIT, SIZEOF(REORG_GBUFF_LIMIT));
		}
		reg_nam_mval.str.len = gv_cur_region->rname_len;
		reg_nam_mval.str.addr = (char *)&gv_cur_region->rname;
		op_view(VARLSTCNT(3) &literal_poollimit, &reg_nam_mval, &(TREF(gbuff_limit)));
		if (WBTEST_ENABLED(WBTEST_REORG_DEBUG))
			util_out_print("GTMPOOLLIMIT used for mupip reorg : !UL", TRUE, cs_addrs->gbuff_limit);
		util_out_print("   ", FLUSH);
		util_out_print("Global: !AD (region !AD)", FLUSH, GNAME(gl_ptr).len, GNAME(gl_ptr).addr, REG_LEN_STR(gl_ptr->reg));
		if (in_exclude_list((unsigned char *)GNAME(gl_ptr).addr, GNAME(gl_ptr).len, &exclude_gl_head))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_EXCLUDEREORG, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
			reorg_success = FALSE;
			continue;
		}
		/* Save the global name in reorg_gv_target. Via gv_currkey_next_reorg, it's possible for gv_currkey to become
		 * out of sync with gv_target. We'll use reorg_gv_target->gvname to make sure the correct root block is found.
		 */
		reorg_gv_target->gvname.var_name = GNAME(gl_ptr);
		GTMTRIG_ONLY(assert(!IS_MNAME_HASHT_GBLNAME(reorg_gv_target->gvname.var_name));)
		cur_success = mu_reorg(gl_ptr, &exclude_gl_head, &resume, index_fill_factor, data_fill_factor, reorg_op);
		reorg_success &= cur_success;
		SET_GV_CURRKEY_FROM_GVT(reorg_gv_target);
		if (truncate)
		{	/* No need to move root blocks unless truncating */
			assert(gv_cur_region == gl_ptr->reg);	/* should have been set inside "mu_reorg" call done above */
			if (cur_success)
			{	/* add region corresponding to this global to the set (list) of regions to truncate */
				for (reg_iter = reg_list, prev_reg = reg_list; reg_iter; reg_iter = reg_iter->next)
					if (reg_iter->reg == gv_cur_region)
						break;
					else
						prev_reg = reg_iter;
				if (NULL == reg_iter)
				{
					tmp_reg = (trunc_region *)malloc(SIZEOF(trunc_region));
					tmp_reg->reg = gv_cur_region;
					tmp_reg->next = NULL;
					if (NULL == reg_list)
						reg_list = tmp_reg;
					else
						prev_reg->next = tmp_reg;
#					ifdef GTM_TRIGGER
					if (truncate)
					{	/* Reorg ^#t in this region to move it out of the way. */
						SET_GVTARGET_TO_HASHT_GBL(cs_addrs);	/* sets gv_target */
						inctn_opcode = inctn_invalid_op;	/* needed for INITIAL_HASHT_ROOT_SEARCH */
						INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;	/* sets gv_target->root */
						DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
						hasht_gl.next = NULL;
						hasht_gl.reg = gv_cur_region;
						hasht_gl.gvt = gv_target;
						if (0 != gv_target->root)
						{
							util_out_print("   ", FLUSH);
							util_out_print("Global: !AD (region !AD)", FLUSH,
								GNAME(&hasht_gl).len, GNAME(&hasht_gl).addr,
								REG_LEN_STR(gv_cur_region));
							reorg_gv_target->gvname.var_name = gv_target->gvname.var_name;
							cur_success = mu_reorg(&hasht_gl, &exclude_gl_head, &resume,
										index_fill_factor, data_fill_factor, reorg_op);
							reorg_success &= cur_success;
						}
					}
#					endif
				}
			}
		}
		if (mu_ctrlc_occurred || mu_ctrly_occurred)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REORGCTRLY);
			mupip_exit(ERR_MUNOFINISH);
		}
	}
	status = SS_NORMAL;
	if (!reorg_success)
	{
		inctn_opcode = inctn_invalid_op;	/* needed by assert inside "preemptive_db_clnup" called by rts_error */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REORGINC);
		status = ERR_REORGINC;
	} else if (truncate)
	{
		/* Move GVT ROOT blocks of all global names AFTER doing regular reorg on ALL global names.
		 * This way we ensure one pass of reorg -truncate is enough to produce an optimally truncated file.
		 */
		root_swap_statistic = 0;
		for (gl_ptr = gl_head.next; gl_ptr; gl_ptr = gl_ptr->next)
			mu_swap_root(gl_ptr, &root_swap_statistic);
		hasht_gl.next = NULL;
		for (reg_iter = reg_list; reg_iter; reg_iter = reg_iter->next)
		{
			gv_cur_region = reg_iter->reg;
			tp_change_reg();
			SET_GVTARGET_TO_HASHT_GBL(cs_addrs);	/* sets gv_target */
			SET_GV_CURRKEY_FROM_GVT(gv_target);
			gv_target->root = 0; /* Recompute gv_target->root in case mu_reorg changed things around */
			inctn_opcode = inctn_invalid_op;	/* needed for GVCST_ROOT_SEARCH */
			GVCST_ROOT_SEARCH;			/* set gv_target->root */
			if (0 == gv_target->root)
				continue;
			hasht_gl.reg = gv_cur_region;
			hasht_gl.gvt = gv_target;
			mu_swap_root(&hasht_gl, &root_swap_statistic);
		}
		util_out_print("Total root blocks moved: !UL", FLUSH, root_swap_statistic);
		mu_reorg_process = FALSE;
		/* Default threshold is 0 i.e. we attempt to truncate no matter what free_blocks is. */
		truncate_percent = 0;
		cli_get_int("TRUNCATE", (int4 *)&truncate_percent);
		if (99 < truncate_percent)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUTRUNCPERCENT);
			mupip_exit(ERR_MUTRUNCFAIL);
		}
		for (reg_iter = reg_list; reg_iter; reg_iter = reg_iter->next)
		{
			gv_cur_region = reg_iter->reg;
			tp_change_reg();
			csd = cs_data;
			csa = cs_addrs;
			cnl = csa->nl;
			/* Ensure only one truncate process at a time operates on given region */
			grab_crit(gv_cur_region);
			lcl_pid = cnl->trunc_pid;
			if (lcl_pid && is_proc_alive(lcl_pid, 0))
			{
				rel_crit(gv_cur_region);
				send_msg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(4) ERR_MUTRUNC1ATIME, 3, lcl_pid,
						REG_LEN_STR(gv_cur_region));
				continue;
			}
			cnl->trunc_pid = process_id;
			cnl->highest_lbm_with_busy_blk = 0;
			rel_crit(gv_cur_region);
			if (!mu_truncate(truncate_percent))
				mupip_exit(ERR_MUTRUNCFAIL);
			grab_crit(gv_cur_region);
			assert(cnl->trunc_pid == process_id);
			cnl->trunc_pid = 0;
			rel_crit(gv_cur_region);
			if (mu_ctrlc_occurred || mu_ctrly_occurred)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REORGCTRLY);
				mupip_exit(ERR_MUNOFINISH);
			}
		}
	}
	mupip_exit(status);
}
