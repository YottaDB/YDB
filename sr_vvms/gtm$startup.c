/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <efndef.h>
#include <descrip.h>
#include <jpidef.h>
#include <prvdef.h>
#include <ssdef.h>
#include <starlet.h>
#include <stsdef.h>

#include "gtm_limits.h"
#include "gtm_inet.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "zcall.h"
#include "cryptdef.h"
#include "mv_stent.h"
#include "startup.h"
#include "ladef.h"
#include "io.h"
#include "iottdef.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "cmd_qlf.h"
#include "gdskill.h"
#include "filestruct.h"
#include "error.h"		/* for EXIT_HANDLER macro used in SET_EXIT_HANDLER macro */
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "collseq.h"
#ifndef __vax
#  include "fnpc.h"
#endif
#include "desblk.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmimagename.h"
#include "cache.h"
#include "op.h"
#include "patcode.h"
#include "dpgbldir_sysops.h"	/* for dpzgbini prototype */
#include "dfntmpmbx.h"
#include "ast_init.h"
#include "comp_esc.h"
#include "repl_sp.h"
#include "mprof.h"
#include "init_secshr_addrs.h"
#include "getzmode.h"
#include "getzprocess.h"
#include "gtmmsg.h"
#include "tp_timeout.h"
#include "getjobname.h"
#include "error_trap.h"			/* for ecode_init() prototype */
#include "zyerror_init.h"
#include "generic_exit_handler.h"
#include "ztrap_form_init.h"
#include "zdate_form_init.h"
#include "dollar_system_init.h"
#include "gtm_env_xlate_init.h"
#include "zco_init.h"
#include "svnames.h"
#include "getzdir.h"
#include "jobinterrupt_init.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_logicals.h"	/* for DISABLE_ALIGN_STRINGS */
#include "logical_truth_value.h"
#include "zwrite.h"
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"

#define FREE_RTNTBL_SPACE 	17
#define MIN_INDIRECTION_NESTING 32
#define MAX_INDIRECTION_NESTING 256
#define MAX_IO_TIMER 		60000000
#define MIN_IO_TIMER 		1000000

GBLDEF zctabrtn		*zctab, *zctab_end;
GBLDEF zcpackage	*zcpack_start, *zcpack_end;
GBLDEF unsigned char	*gtm_main_address;	/* Save the address of gtm_main, for use by tir */
GBLDEF bool		init_done = FALSE;
GBLDEF uint4            trust = 0;
GBLDEF mval		original_cwd;
GBLDEF unsigned char	original_cwd_buff[PATH_MAX + PATH_MAX]; /* device + directory */

GBLREF desblk		exi_blk;
GBLREF void		(*tp_timeout_start_timer_ptr)(int4 tmout_sec);
GBLREF void		(*tp_timeout_clear_ptr)(void);
GBLREF void		(*tp_timeout_action_ptr)(void);
GBLREF int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF void		(*unw_prof_frame_ptr)(void);
GBLREF rtn_tabent	*rtn_fst_table, *rtn_names, *rtn_names_top, *rtn_names_end;
GBLREF int4		break_message_mask;
GBLREF int4		exi_condition;
GBLREF int4		write_filter;
GBLREF int4		spc_inp_prc;
GBLREF uint4		image_count;
GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*stackbase, *stacktop, *stackwarn, *msp;
GBLREF mv_stent		*mv_chain;
GBLREF bool		undef_inhibit;
GBLREF uint4	        iott_write_delay[2];
GBLREF int		(* volatile xfer_table[])();
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zstatus;
GBLREF mval		dollar_zdir;
GBLREF spdesc		stringpool;
GBLREF spdesc		rts_stringpool;
GBLREF command_qualifier glb_cmd_qlf, cmd_qlf;
GBLREF int4		zdir_form;
GBLREF int		mumps_status;
GBLREF boolean_t	is_replicator;
GBLREF int		dollar_truth;
GBLREF boolean_t	mstr_native_align;
GBLREF void             (*cache_table_relobjs)(void);   /* Function pointer to call cache_table_rebuild() */
GBLREF symval		*curr_symval;
GBLREF boolean_t	skip_dbtriggers;
OS_PAGE_SIZE_DECLARE

error_def(ERR_LINKVERSION);
error_def(ERR_WILLEXPIRE);
error_def(LP_NOCNFDB);			/* No license data base */
error_def(ERR_COLLATIONUNDEF);

static readonly mstr lnm$group   = {9,  "LNM$GROUP"};

/* Licensing     */
GBLREF bool     licensed;
GBLREF int4	lkid, lid;

LITREF char	gtm_product[PROD];
LITREF int4	gtm_product_len;
LITREF char	gtm_version[VERS];
LITREF int4	gtm_version_len;
/* End of Licensing */

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_LINKVERSION);
error_def(ERR_WILLEXPIRE);
error_def(LP_NOCNFDB);			/* No license data base */

void gtm$startup(struct startup_vector *svec, boolean_t is_dal)
/* Note: various references to data copied from *svec could profitably be referenced directly */
{
	char		*mstack_ptr;
	unsigned char	*base_address, *transfer_address;
	void		dfnlnm$tmpmbx();
	static readonly unsigned char init_break[1] = {'B'};
	int4		lct;
	int		i;
	mstr		log_name;
	mstr		val;
	boolean_t	ret, is_defined;
	char		buff[MAX_FN_LEN];
	DCL_THREADGBL_ACCESS;

	uint4           imagpriv, trust_status;
	typedef struct {
		short buflen;
		short itmcode;
		void  *buffer;
		void  *retlen;
	} ITMLST;
	ITMLST item_list[2] = {
		{ 4, JPI$_IMAGPRIV, &imagpriv, 0},
		{ 0, 0,             0,         0} };

	/* Licensing */
	int4	status;
	int4	inid;
	int4	nid;				/* node number		*/
	int4	days;				/* days to expiration	  */
	int4	lic_x;				/* license value	  */
	char	*h;				/* license data base	  */
	char	*pak;				/* pak record		  */
	int4	mdl;				/* hardw. model type	  */
	struct dsc$descriptor_s	dprd, dver;
	/* End of Licensing */

	/* While normally gtm$startup is only called once per process, when the invocation method is through
	 * call-ins, it is invoked on every call-in so we need to be able to handle the case that we are already
	 * initialized. The main "flag" for this is the init_done global variable but since that flag may one day
	 * move into the gtm_threadgbl framework (even though VMS will never be threaded), we instead test the
	 * threadgbl base variable to see if it is initialized or not and bypass threadgbl initialization if so.
	 */
	if (NULL == gtm_threadgbl)
	{
		GTM_THREADGBL_INIT;			/* This is the first C routine in VMS so do init here */
	}
	cache_table_relobjs = &cache_table_rebuild;
	/* If we get any errors signalled during initialization, don't let the process
	 * continue under any circumstances.  This condition handler enforces that.
	 */
	lib$establish(lib$sig_to_stop);
	assert(BITS_PER_UCHAR == 8);
	/* When items are added to svec, but recompilation of a new release is not
	 * required, appropriate "if (svec->argcnt < ?)" statements should be
	 * added here to establish defaults.  The computed defaults may all be removed
	 * for each release where recompilation is required.
	 */
	tp_timeout_start_timer_ptr = tp_start_timer;
	tp_timeout_clear_ptr = tp_clear_timeout;
	tp_timeout_action_ptr = tp_timeout_action;
	op_open_ptr = op_open;
	unw_prof_frame_ptr = unw_prof_frame;
	if (SIZEOF(*svec) != svec->argcnt)
		rts_error(VARLSTCNT(1) ERR_LINKVERSION);
	if (!init_done)
	{
		gtm_imagetype_init(GTM_IMAGE);
		gtm_env_init();	/* read in all environment variables */
		if ((!is_dal) && NULL != (TREF(mprof_env_gbl_name)).str.addr)
			turn_tracing_on(TADR(mprof_env_gbl_name), TRUE, (TREF(mprof_env_gbl_name)).str.len > 0);
		licensed = TRUE;
		pak = NULL;
#		ifdef	NOLICENSE
		status = SS$_NORMAL;
		h = NULL;
#		else
		if (NULL == (h = la_getdb(LMDB)))		/* license db in mem	  */
			status = LP_NOCNFDB;
		else
			status = SS$_NORMAL;
#		endif
		get_page_size();
		/* note: for upward compatibility, missing values in the startup vector can be set to defaults */
		get_proc_info(0, TADR(login_time), &image_count);
		gtm_main_address = svec->gtm_main_inaddr;
		ast_init();
		rtn_fst_table = rtn_names = (rtn_tabent *)svec->rtn_start;
		rtn_names_end = (rtn_tabent *)svec->rtn_end - 1;
		rtn_names_top = (rtn_tabent *)svec->rtn_end + FREE_RTNTBL_SPACE - 1;
		rtn_tbl_sort(rtn_names, rtn_names_end);
		fgncal_zlinit();
		zctab = (zctabrtn *)svec->zctable_start;
		zctab_end = (zctabrtn *)svec->zctable_end;
		zcpack_start = (zcpackage *)svec->zcpackage_begin;
		zcpack_end = (zcpackage *)svec->zcpackage_end;
		svec->xf_tab = &xfer_table[0];
		svec->dlr_truth = &dollar_truth;
		if (0 != svec->user_spawn_flag)
		{
		        if (SS$_NORMAL != (trust_status = sys$getjpiw(EFN$C_ENF, 0, 0, item_list, 0, 0, 0)))
			        rts_error(VARLSTCNT(1) trust_status);
			else if (imagpriv & PRV$M_CMEXEC)
			       trust = svec->user_spawn_flag;
		}
		if (svec->user_stack_size < 4096)
			svec->user_stack_size = 4096;
		if (svec->user_stack_size > 8388608)
			svec->user_stack_size = 8388608;
		mstack_ptr = malloc(svec->user_stack_size);
		msp = stackbase = mstack_ptr + svec->user_stack_size - mvs_size[MVST_STORIG];
		mv_chain = (mv_stent *)msp;
		mv_chain->mv_st_type = MVST_STORIG;      /* Initialize first (anchor) mv_stent so doesn't do anything */
		mv_chain->mv_st_next = 0;
		mv_chain->mv_st_cont.mvs_storig = 0;
		stacktop = mstack_ptr + 2 * mvs_size[MVST_NTAB];
		stackwarn = stacktop + 1024;
		if (!svec->labels)
			glb_cmd_qlf.qlf &= ~CQ_LOWER_LABELS;
		cmd_qlf.qlf = glb_cmd_qlf.qlf;
		break_message_mask = svec->break_message_mask;
		undef_inhibit = svec->undef_inhib;
		TREF(lv_null_subs) = svec->lvnullsubs;
		zdir_form = svec->zdir_form;
		if (!IS_VALID_ZDIR_FORM(zdir_form))
			zdir_form = ZDIR_FORM_FULLPATH;
		write_filter = 0;
		if (svec->user_write_filter & CHAR_FILTER)
			write_filter |= CHAR_FILTER;
		if (svec->user_write_filter & ESC1)
			write_filter |= ESC1;
		spc_inp_prc = svec->special_input;
		if (svec->user_strpl_size < STP_INITSIZE)
			svec->user_strpl_size = STP_INITSIZE;
		else if (svec->user_strpl_size > STP_MAXINITSIZE)
			svec->user_strpl_size = STP_MAXINITSIZE;
		stp_init(svec->user_strpl_size);
		if ((svec->user_io_timer < MAX_IO_TIMER) && (svec->user_io_timer > MIN_IO_TIMER))
			iott_write_delay[0] = -svec->user_io_timer;
		rts_stringpool = stringpool;
		TREF(compile_time) = FALSE;
		assert(run_time); /* Should have been set by gtm_imagetype_init */
		/* Initialize alignment requirement for the runtime stringpool */
		log_name.addr = DISABLE_ALIGN_STRINGS;
		log_name.len = STR_LIT_LEN(DISABLE_ALIGN_STRINGS);
		/* mstr_native_align = logical_truth_value(&log_name, FALSE, NULL) ? FALSE : TRUE; */
		mstr_native_align = FALSE; /* TODO: remove this line and uncomment the above line */
		getjobname();
		DEBUG_ONLY(util_out_open(0));
		INVOKE_INIT_SECSHR_ADDRS;
#		ifdef	NOLICENSE
		status = SS$_NORMAL;
		mdl = nid = inid = 0;
#		else
		if (1 == (status & 1))				/* licensing: node+ system  */
		{
			mdl = nid = inid = 0;
			status = lm_mdl_nid(&mdl, &nid, &inid);
		}
#		endif
		dfntmpmbx(lnm$group.len, lnm$group.addr);
		if (svec->base_addr)
			base_address = svec->base_addr - SIZEOF(frame_pointer->rvector->jsb);
#		ifdef	NOLICENSE
		status = SS$_NORMAL;
		lid = 1;
		lic_x = 32767;
		days = 128;
#		else
		if (1 == (status & 1))				/* licensing: license */
		{
			dprd.dsc$w_length = gtm_product_len;
			dprd.dsc$a_pointer = gtm_product;
			dver.dsc$w_length = gtm_version_len;
			dver.dsc$a_pointer = gtm_version;
			status = lp_licensed(h, &dprd, &dver, mdl, nid, &lid, &lic_x, &days, pak);
		}
#		endif
		if (1 == (status & 1))				/* licensing: license units  */
			status = LP_ACQUIRE(pak, lic_x, lid, &lkid);	/* def in cryptdef */
#		ifdef	NOLICENSE
		status = SS$_NORMAL;
#		else
		if (LP_NOCNFDB != status)
			la_freedb(h);
		if (1 == (status & 1))					/* licensing */
		{
			licensed = TRUE;
			if (days < 14)
				la_putmsgu(ERR_WILLEXPIRE, 0, 0);
		} else
		{
			licensed = FALSE;
			rts_error(VARLSTCNT(1) status);
		}
#		endif
		jobinterrupt_init();
		getzprocess();
		getzmode();
		/* Now that we let users change cwd (set $zdir), we want to save cwd so that we can restore cwd at exit. */
		/* We save cwd at startup to workaround bug (?) documented in setzdir() regarding chdir() */
		/* We don't want to place original_cwd in the stringpool 'cos it doesn't change over the life of a process. */
		/* By placing original_cwd in a pre-allocated buffer, we avoid making original_cwd a part of gcol passes. */
		getzdir(); /* get current working directory at startup */
		original_cwd = dollar_zdir;
		original_cwd.str.addr = original_cwd_buff;
		memcpy(original_cwd_buff, dollar_zdir.str.addr, dollar_zdir.str.len);
		cache_init();
		zco_init();
		frame_pointer = msp -= SIZEOF(stack_frame) + SIZEOF(rhdtyp);
		memset(frame_pointer, 0, SIZEOF(stack_frame) + SIZEOF(rhdtyp));
		frame_pointer->type = SFT_COUNT;
		frame_pointer->rvector = (rhdtyp *)((char *)frame_pointer + SIZEOF(stack_frame));
		symbinit();
		if (svec->base_addr)
		{
			base_frame(base_address);
			jobchild_init(base_address);
		}
		dpzgbini();
		svec->frm_ptr = frame_pointer;
		io_init(svec->ctrlc_enable);
		dollar_ztrap.mvtype = MV_STR;
		dollar_ztrap.str.len = SIZEOF(init_break);
		dollar_ztrap.str.addr = init_break;
		dollar_zstatus.mvtype = MV_STR;
		dollar_zstatus.str.len = 0;
		dollar_zstatus.str.addr = NULL;
		ecode_init();
		zyerror_init();
		ztrap_form_init();
		zdate_form_init(svec);
		dollar_system_init(svec);
		gtm_env_xlate_init();
		initialize_pattern_table();
		/* Initialize compiler escape feature */
		ce_init();
		/* Initialize local collating sequence */
		TREF(transform) = TRUE;
		lct = find_local_colltype();
		if (0 != lct)
		{
			TREF(local_collseq) = ready_collseq(lct);
			if (!TREF(local_collseq))
			{
				exi_condition = ERR_COLLATIONUNDEF;
				gtm_putmsg(VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
				op_halt();
			}
		} else
			TREF(local_collseq) = NULL;
		/* Initialize zwrite subsystem. Better to do it now when we have storage to allocate than
		 * if we fail and storage allocation may not be possible. To that end, pretend we have
		 * seen alias acitivity so those structures are initialized as well.
		 */
		assert(FALSE == curr_symval->alias_activity);
		curr_symval->alias_activity = TRUE;
		lvzwr_init(0, (mval *)NULL);
		curr_symval->alias_activity = FALSE;
		/* Initialize cache structure for $Piece function (except for Vax which does not use this) */
		for (i = 0; FNPC_MAX > i; i++)
		{
			(TREF(fnpca)).fnpcs[i].pcoffmax = &(TREF(fnpca)).fnpcs[i].pstart[FNPC_ELEM_MAX];
			(TREF(fnpca)).fnpcs[i].indx = i;
		}
		(TREF(fnpca)).fnpcsteal = &(TREF(fnpca)).fnpcs[0];		/* Starting place to look for cache reuse */
		(TREF(fnpca)).fnpcmax = &(TREF(fnpca)).fnpcs[FNPC_MAX - 1];	/* The last element */
		/* until this point all signals run down the process. That sounds reasonable since we're not executing any
		 * of the customers code and we wouldn't know how to cope anyway.  Now we can cope so establish
		 * the mdb_condition_handler.
		 */
		*svec->fp = mdb_condition_handler;
		SET_EXIT_HANDLER(exi_blk, generic_exit_handler, exi_condition);	/* Establish exit handler */
		init_done = TRUE;
	} else
	{
		if (svec->base_addr)
		{	base_address = svec->base_addr - SIZEOF(frame_pointer->rvector->jsb);
			transfer_address = base_address + SIZEOF(rhdtyp);
			svec->xf_tab = &xfer_table[0];
			base_frame(base_address);
			new_stack_frame(base_address, transfer_address, transfer_address);
			svec->frm_ptr = frame_pointer;
			*svec->fp = mdb_condition_handler;
			mumps_status = SS$_NORMAL;
		}
	}
	lib$revert();	/* from lib$sig_to_stop establish */
	return;
}
