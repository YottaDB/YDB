/****************************************************************
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

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs_ptr_t */
#include "view.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "hashtab_mname.h"    /* needed for cmmdef.h */
#include "cmidef.h"
#include "cmmdef.h"
#include "tp_frame.h"
#include "collseq.h"
#include "error.h"
#include "op.h"
#include "patcode.h"
#include "mvalconv.h"
#include "lv_val.h"
#include "alias.h"
#include "gtmimagename.h"
#include "fullbool.h"
#include "wbox_test_init.h"
#include "format_targ_key.h"
#include "str2gvkey.h"
#include "gvcst_protos.h"
#include "gvnh_spanreg.h"
#include "targ_alloc.h"
#include "change_reg.h"
# include "gtmlink.h"
#include "gtm_ctype.h"		/* for ISDIGIT_ASCII macro */
#include "gvn2gds.h"
#include "io.h"
#include "interlock.h"

GBLREF spdesc		stringpool;
GBLREF int4		cache_hits, cache_fails;
GBLREF unsigned char	*stackbase, *stacktop;
GBLREF gd_addr		*gd_header;
GBLREF boolean_t	certify_all_blocks;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF jnl_fence_control jnl_fence_ctl;
GBLREF jnlpool_addrs_ptr_t	jnlpool;
GBLREF bool		undef_inhibit;
GBLREF int4		break_message_mask;
GBLREF command_qualifier cmd_qlf;
GBLREF tp_frame		*tp_pointer;
GBLREF uint4		dollar_tlevel;
GBLREF int4		zdir_form;
GBLREF boolean_t	badchar_inhibit;
GBLREF boolean_t	gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database blocks */
GBLREF int		gv_fillfactor;
GBLREF int4		gtm_max_sockets;
GBLREF gv_key		*gv_currkey;
GBLREF boolean_t	is_gtm_chset_utf8;
GBLREF	uint4		process_id;
GBLREF mstr		extnam_str;
GBLREF	boolean_t	dmterm_default;

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_GBLNOMAPTOREG);
error_def(ERR_VIEWFN);
error_def(ERR_VIEWGVN);

LITREF	gtmImageName	gtmImageNames[];
LITREF	mstr		relink_allowed_mstr[];
LITREF	mval		literal_zero;
LITREF	mval		literal_one;

#define		MM_RES			"MM"
#define		BG_RES			"BG"
#define		CM_RES			"CM"
#define		USR_RES			"USR"
#define		GTM_BOOL_RES		"GT.M Boolean short-circuit"
#define		STD_BOOL_RES		"Standard Boolean evaluation side effects"
#define		WRN_BOOL_RES		"Standard Boolean with side-effect warning"
#define		STATS_MAX_DIGITS	MAX_DIGITS_IN_INT8
#define		STATS_KEYWD_SIZE	(3 + 1 + 1)	/* 3 character mnemonic, colon and comma */

#define STATS_PUT_PARM(TXT, CNTR, BASE)					\
{									\
	MEMCPY_LIT(stringpool.free, TXT);				\
	stringpool.free += STR_LIT_LEN(TXT);				\
	*stringpool.free++ = ':';					\
	stringpool.free = i2ascl(stringpool.free, BASE.CNTR);		\
	*stringpool.free++ = ',';					\
	assert(stringpool.free <= stringpool.top);			\
}

void	op_fnview(int numarg, mval *dst, ...)
{
	boolean_t	save_transform;
	gv_key		save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	unsigned char	*key;
	unsigned char	buff[MAX_ZWR_KEY_SZ];
	collseq		*csp;
	gd_binding	*map, *start_map, *end_map;
	gd_region	*reg, *reg_start;
	gv_key		*gvkey;
	gv_namehead	temp_gv_target;
	gvnh_reg_t	*gvnh_reg;
	gvnh_spanreg_t	*gvspan;
	int		n, tl, newlevel, res, reg_index, collver, nct, act, ver;
	lv_val		*lv;
	gd_gblname	*gname;
	mstr		tmpstr, commastr, *gblnamestr;
	mval		*arg1, *arg2, tmpmval;
	mval		*keyword;
	sgmnt_addrs	*csa;
	tp_frame	*tf;
	trans_num	gd_targ_tn, *tn_array;
	unsigned char	*c, *c_top;
	va_list		var;
	viewparm	parmblk, parmblk2;
	viewtab_entry	*vtp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VMS_ONLY(va_count(numarg));
	assertpro(2 <= numarg);
	VAR_START(var, dst);
	keyword = va_arg(var, mval *);
	MV_FORCE_STR(keyword);
	numarg -= 2;	/* remove destination and keyword from count */
	if (numarg > 0)
	{
		arg1 = va_arg(var, mval *);
		MV_FORCE_STR(arg1);
		if (--numarg > 0)
		{
			arg2 = va_arg(var, mval *);
			DEBUG_ONLY(--numarg;)
		} else
			arg2 = (mval *)NULL;
	} else
	{
		arg1 = (mval *)NULL;
		arg2 = (mval *)NULL;
	}
	assert(!numarg);
	va_end(var);
	vtp = viewkeys(&keyword->str);
	view_arg_convert(vtp, (int)vtp->parm, arg1, &parmblk, IS_DOLLAR_VIEW_TRUE);
	switch (vtp->keycode)
	{
#		ifdef UNICODE_SUPPORTED
		case VTK_BADCHAR:
			n = badchar_inhibit ? 0 : 1;
			break;
#		endif
		case VTK_RCHITS:
			n = 0;
			break;
		case VTK_RCMISSES:
			n = 0;
			break;
		case VTK_RCSIZE:
			n = 0;
			break;
		case VTK_STKSIZ:
			n = (int)(stackbase - stacktop);
			break;
		case VTK_ICSIZE:
			n = 0;	/* must change run-time structure */
			break;
		case VTK_ICHITS:
			n = cache_hits;
			break;
		case VTK_ICMISS:
			n = cache_fails;
			break;
		case VTK_SPSIZE:
			commastr.len = 1;
			commastr.addr = ",";
			ENSURE_STP_FREE_SPACE((STATS_MAX_DIGITS * 3) + 2);
			MV_FORCE_MVAL(dst, (int)(stringpool.top - stringpool.base));
			MV_FORCE_STR(dst);
			dst->mvtype = vtp->restype;
			s2pool_concat(dst, &commastr);
			arg2 = &tmpmval;
			MV_FORCE_MVAL(arg2, (int)(stringpool.free - stringpool.base));
			MV_FORCE_STR(arg2);
			s2pool_concat(dst, &arg2->str);
			s2pool_concat(dst, &commastr);
			MV_FORCE_MVAL(arg2, (int)(stringpool.top - stringpool.invokestpgcollevel));
			MV_FORCE_STR(arg2);
			s2pool_concat(dst, &arg2->str);
			break;
		case VTK_GDSCERT:
			if (certify_all_blocks)
				*dst = literal_one;
			else
				*dst = literal_zero;
			break;
		case VTK_GVACC_METH:
			assert(gd_header);
			assert(parmblk.gv_ptr);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			assert(parmblk.gv_ptr->open);
			switch (REG_ACC_METH(parmblk.gv_ptr))
			{
				case dba_mm:
					tmpstr.addr = MM_RES;
					tmpstr.len = SIZEOF(MM_RES)-1;
					break;
				case dba_bg:
					tmpstr.addr = BG_RES;
					tmpstr.len = SIZEOF(BG_RES)-1;
					break;
				case dba_cm:
					tmpstr.addr = CM_RES;
					tmpstr.len = SIZEOF(CM_RES)-1;
					break;
				case dba_usr:
					tmpstr.addr = USR_RES;
					tmpstr.len = SIZEOF(USR_RES)-1;
					break;
				default:
					assertpro(FALSE && REG_ACC_METH(parmblk.gv_ptr));
			}
			dst->str = tmpstr;
			break;
		case VTK_FULLBOOL:
			switch (TREF(gtm_fullbool))
			{
				case GTM_BOOL:
					tmpstr.addr = GTM_BOOL_RES;
					tmpstr.len = SIZEOF(GTM_BOOL_RES)-1;
					break;
				case FULL_BOOL:
					tmpstr.addr = STD_BOOL_RES;
					tmpstr.len = SIZEOF(STD_BOOL_RES)-1;
					break;
				case FULL_BOOL_WARN:
					tmpstr.addr = WRN_BOOL_RES;
					tmpstr.len = SIZEOF(WRN_BOOL_RES)-1;
					break;
				default:
					assertpro(FALSE && TREF(gtm_fullbool));
			}
			dst->str = tmpstr;
			break;
		case VTK_GVDUPSETNOOP:
			if (gvdupsetnoop)
				*dst = literal_one;
			else
				*dst = literal_zero;
			break;
		case VTK_GVFILE:
			assert(gd_header);
			assert(parmblk.gv_ptr);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			tmpstr.addr = (char *)parmblk.gv_ptr->dyn.addr->fname;
			tmpstr.len = parmblk.gv_ptr->dyn.addr->fname_len;
			s2pool(&tmpstr);
			dst->str = tmpstr;
			break;
		case VTK_GVFIRST:
			if (!gd_header)
				gvinit();
			tmpstr.addr = (char *)gd_header->regions->rname;
			tmpstr.len = gd_header->regions->rname_len;
			s2pool(&tmpstr);
			dst->str = tmpstr;
			break;
		case VTK_GVNEXT:
			assert(gd_header);
			if (arg1->str.len)
			{
				for (reg = parmblk.gv_ptr + 1;
				     ((reg - gd_header->regions) < gd_header->n_regions) && IS_STATSDB_REG(reg); reg++)
					;
				parmblk.gv_ptr = reg;
			}
			if (parmblk.gv_ptr - gd_header->regions >= gd_header->n_regions)
				dst->str.len = 0;
			else
			{
				tmpstr.addr = (char *)parmblk.gv_ptr->rname;
				tmpstr.len = parmblk.gv_ptr->rname_len;
				s2pool(&tmpstr);
				dst->str = tmpstr;
			}
			break;
		case VTK_JNLACTIVE:
			assert(gd_header);
			if (parmblk.gv_ptr)
			{
				if (!parmblk.gv_ptr->open)
					gv_init_reg(parmblk.gv_ptr, NULL);
				csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
				if (NULL != csa->hdr)
					n = csa->hdr->jnl_state;
				else
					n = -1;
			} else
				n = 0;
			break;
		case VTK_JNLFILE:
			assert(gd_header);
			if (parmblk.gv_ptr)
			{
				if (!parmblk.gv_ptr->open)
					gv_init_reg(parmblk.gv_ptr, NULL);
				csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
				if (NULL != csa->hdr)
					view_jnlfile(dst, parmblk.gv_ptr);
				else
					dst->str.len = 0;
			} else
				dst->str.len = 0;
			break;
		case VTK_JNLPOOL:
			if (jnlpool)
			{
				reg = jnlpool->jnlpool_dummy_reg;
				if (reg && reg->dyn.addr)
				{
					tmpstr.addr = (char *)reg->dyn.addr->fname;
					tmpstr.len = reg->dyn.addr->fname_len;
					s2pool(&tmpstr);
					dst->str = tmpstr;
				} else
					dst->str.len = 0;
			} else
				dst->str.len = 0;
			break;
		case VTK_JNLTRANSACTION:
			n = jnl_fence_ctl.level;
			break;
		case VTK_LABELS:
			n = (cmd_qlf.qlf & CQ_LOWER_LABELS) ? 1 : 0;
			break;
		case VTK_LVNULLSUBS:
			n = TREF(lv_null_subs);
			break;
		case VTK_NOISOLATION:
			if (NOISOLATION_NULL != parmblk.ni_list.type || NULL == parmblk.ni_list.gvnh_list
			    || NULL != parmblk.ni_list.gvnh_list->next)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VIEWFN);
			n = parmblk.ni_list.gvnh_list->gvnh->noisolation;
			break;
		case VTK_PATCODE:
			getpattabnam(&tmpstr);
			s2pool(&tmpstr);
			dst->str = tmpstr;
			break;
		case VTK_POOLLIMIT:
			assert(NULL != gd_header);	/* view_arg_convert would have done this for VTK_POOLLIMIT */
			reg = parmblk.gv_ptr;
			if (!reg->open)
				gv_init_reg(reg, NULL);
			csa = &FILE_INFO(reg)->s_addrs;
			n = csa->gbuff_limit;
			break;
#ifdef TESTPOLLCRIT
		case VTK_GRABCRIT:
		case VTK_RELCRIT:
		case VTK_GRABLOCK:
		case VTK_RELLOCK:
		case VTK_GRABJNLPH2:
		case VTK_RELJNLPH2:
		case VTK_RELJNLPOOLPH2:
		case VTK_GRABJNLPOOLPH2:
		case VTK_GRABJNLQIO:
		case VTK_RELJNLQIO:
			reg = parmblk.gv_ptr;
			if (!reg->open)
				gv_init_reg(reg, NULL);
			csa = &FILE_INFO(reg)->s_addrs;
			if (NULL != csa->hdr)
			{
				if (VTK_GRABCRIT == vtp->keycode)
					grab_crit(reg);
				else if (VTK_RELCRIT == vtp->keycode)
					rel_crit(reg);
				else if (VTK_GRABLOCK == vtp->keycode)
				{
					assert(NULL != jnlpool);
					if (jnlpool)
						grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
				} else if (VTK_RELLOCK == vtp->keycode)
				{
					assert(NULL != jnlpool);
					if (jnlpool)
						rel_lock(jnlpool->jnlpool_dummy_reg);
				} else if (VTK_GRABJNLPH2 == vtp->keycode)
					grab_latch(&csa->jnl->jnl_buff->phase2_commit_latch, GRAB_LATCH_INDEFINITE_WAIT);
				else if (VTK_RELJNLPH2 == vtp->keycode)
					rel_latch(&csa->jnl->jnl_buff->phase2_commit_latch);
				else if (VTK_GRABJNLPOOLPH2 == vtp->keycode)
				{
					assert((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl));
					if (jnlpool && jnlpool->jnlpool_ctl)
						grab_latch(&jnlpool->jnlpool_ctl->phase2_commit_latch, GRAB_LATCH_INDEFINITE_WAIT);
				} else if (VTK_RELJNLPOOLPH2 == vtp->keycode)
				{
					assert((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl));
					if (jnlpool && jnlpool->jnlpool_ctl)
						rel_latch(&jnlpool->jnlpool_ctl->phase2_commit_latch);
				} else if (VTK_GRABJNLQIO == vtp->keycode)
				{
					while (!GET_SWAPLOCK(&csa->jnl->jnl_buff->io_in_prog_latch))
						SHORT_SLEEP(1);
				} else if (VTK_RELJNLQIO == vtp->keycode)
					RELEASE_SWAPLOCK(&csa->jnl->jnl_buff->io_in_prog_latch);
			}
			break;
#endif
		case VTK_PROBECRIT:
			assert(NULL != gd_header);	/* view_arg_convert would have done this for VTK_POOLLIMIT */
			reg = parmblk.gv_ptr;
			if (!reg->open)
				gv_init_reg(reg, NULL);
			csa = &FILE_INFO(reg)->s_addrs;
			if (NULL != csa->hdr)
			{
				csa->crit_probe = TRUE;
				grab_crit(reg);
				csa->crit_probe = FALSE;
				if (!WBTEST_ENABLED(WBTEST_HOLD_CRIT_ENABLED))
					rel_crit(reg);
				dst->str.len = 0;
				ENSURE_STP_FREE_SPACE(n_probecrit_rec_types * (STATS_MAX_DIGITS + STATS_KEYWD_SIZE));
				dst->str.addr = (char *)stringpool.free;
				/* initialize csa->proberit_rec.p_crit_success field from cnl->gvstats_rec */
				csa->probecrit_rec.p_crit_success = csa->nl->gvstats_rec.n_crit_success;
#				define TAB_PROBECRIT_REC(CNTR,TEXT1,TEXT2)	STATS_PUT_PARM(TEXT1, CNTR, csa->probecrit_rec)
#				include "tab_probecrit_rec.h"
#				undef TAB_PROBECRIT_REC
				assert(stringpool.free < stringpool.top);
				/* subtract one to remove extra trailing comma delimiter */
				dst->str.len = INTCAST((char *)stringpool.free - dst->str.addr - 1);
			}
			break;
		case VTK_REGION:
			gblnamestr = &parmblk.str;
			assert(NULL != gd_header); /* "view_arg_convert" call done above would have set this (for VTP_DBKEY case) */
			if (gd_header->n_gblnames)
			{
				gname = gv_srch_gblname(gd_header, gblnamestr->addr, gblnamestr->len);
				n = (NULL != gname) ? gname->act : 0;
			} else
				n = 0;
			gvkey = (gv_key *)save_currkey;
			key = gvn2gds(arg1, gvkey, n);
			assert(key > &gvkey->base[0]);
			assert(gvkey->end == key - &gvkey->base[0] - 1);
			/* -1 usage in "gv_srch_map" calls below is to remove trailing 0 */
			start_map = gv_srch_map(gd_header, (char *)&gvkey->base[0], gvkey->end - 1, SKIP_BASEDB_OPEN_FALSE);
			GVKEY_INCREMENT_ORDER(gvkey);
			end_map = gv_srch_map(gd_header, (char *)&gvkey->base[0], gvkey->end - 1, SKIP_BASEDB_OPEN_FALSE);
			BACK_OFF_ONE_MAP_ENTRY_IF_EDGECASE(gvkey->base, gvkey->end - 1, end_map);
			INCREMENT_GD_TARG_TN(gd_targ_tn);	/* takes a copy of incremented "TREF(gd_targ_tn)"
								 * into local variable "gd_targ_tn" */
			tn_array = TREF(gd_targ_reg_array);	/* could be NULL if no spanning globals were seen as
								 * part of gld open till now.
								 */
			reg_start = &gd_header->regions[0];
			commastr.len = 1;
			commastr.addr = ",";
			for (map = start_map; map <= end_map; map++)
			{
				OPEN_BASEREG_IF_STATSREG(map);
				reg = map->reg.addr;
				GET_REG_INDEX(gd_header, reg_start, reg, reg_index);	/* sets "reg_index" */
				assert((NULL == tn_array) || (TREF(gd_targ_reg_array_size) > reg_index));
				assert((map == start_map) || (NULL != tn_array));
				if ((NULL == tn_array) || (tn_array[reg_index] != gd_targ_tn))
				{	/* this region first encountered now. note it down in region-list to be returned */
					tmpstr.addr = (char *)reg->rname;
					tmpstr.len = reg->rname_len;
					if (map == start_map)
					{
						s2pool(&tmpstr);
						dst->str = tmpstr;
						dst->mvtype = vtp->restype;
					} else
					{
						s2pool_concat(dst, &commastr);
						s2pool_concat(dst, &tmpstr);
					}
					if (NULL != tn_array)
						tn_array[reg_index] = gd_targ_tn;
				}
			}
			break;
		case VTK_RTNCHECKSUM:
			view_routines_checksum(dst, &parmblk.ident);
			break;
		case VTK_RTNEXT:
			view_routines(dst, &parmblk.ident);
			break;
		case VTK_UNDEF:
			n = (undef_inhibit ? 0 : 1);
			break;
		case VTK_BREAKMSG:
			n = break_message_mask;
			break;
		case VTK_BLFREE:
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != csa->hdr)
				n = csa->hdr->trans_hist.free_blocks;
			else
				n = -1;
			break;
		case VTK_BLTOTAL:
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != csa->hdr)
			{
				n = csa->hdr->trans_hist.total_blks;
				n -= (n + csa->hdr->bplmap - 1) / csa->hdr->bplmap;
			} else
				n = -1;
			break;
		case VTK_FREEZE:
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != csa->hdr)
				n = csa->hdr->freeze;
			else
				n = -1;
			break;
		case VTK_GVSTATS:
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != csa->hdr)
			{
				ENSURE_STP_FREE_SPACE(n_gvstats_rec_types * (STATS_MAX_DIGITS + STATS_KEYWD_SIZE));
				dst->str.addr = (char *)stringpool.free;
				/* initialize cnl->gvstats_rec.db_curr_tn field from file header */
				csa->nl->gvstats_rec.db_curr_tn = csa->hdr->trans_hist.curr_tn;
#				define TAB_GVSTATS_REC(CNTR,TEXT1,TEXT2)	STATS_PUT_PARM(TEXT1, CNTR, csa->nl->gvstats_rec)
#				include "tab_gvstats_rec.h"
#				undef TAB_GVSTATS_REC
				assert(stringpool.free < stringpool.top);
				/* subtract one to remove extra trailing delimiter */
				dst->str.len = INTCAST((char *)stringpool.free - dst->str.addr - 1);
			} else
				dst->str.len = 0;
			break;
		case VTK_TID:
			newlevel = MV_FORCE_INT(parmblk.value);
			if (!tp_pointer || newlevel <= 0)
				dst->str.len = 0;
			else
			{
				tl = dollar_tlevel;
				tf = tp_pointer;
				while (tl > newlevel)
				{
					tf = tf->old_tp_frame;
					tl--;
				}
				*dst = tf->trans_id;
			}
			break;
		case VTK_YCOLLATE:
			n = MV_FORCE_INT(parmblk.value);
			csp = ready_collseq(n);
			if (NULL == arg2)
			{	/* $VIEW("YCOLLATE",coll) : Determine version corresponding to collation sequence "coll" */
				if (0 == n)
					break;	/* collation sequence # is 0, version is 0 in this case */
				if (csp)
				{
					n = (*csp->version)();
					n &= 0x000000FF;	/* make an unsigned char, essentially */
				} else
					n = -1;
			} else
			{	/* $VIEW("YCOLLATE",coll,ver) : Check if collsequence "coll" version is compatible with "ver" */
				if (0 == n)
					break;	/* collation sequence # is 0, any version is compatible with it */
				collver = mval2i(arg2);
				n = do_verify(csp, n, collver);
			}
			break;
		case VTK_YGLDCOLL:
			assert(NULL != gd_header);
			if (gd_header->n_gblnames)
			{
				gblnamestr = &parmblk.str;
				gname = gv_srch_gblname(gd_header, gblnamestr->addr, gblnamestr->len);
			} else
				gname = NULL;
			if (NULL != gname)
			{
				nct = 0;	/* currently gname->nct cannot be non-zero */
				act = gname->act;
				ver = gname->ver;
				commastr.len = 1;
				commastr.addr = ",";
				MV_FORCE_MVAL(dst, nct);
				MV_FORCE_STR(dst);
				dst->mvtype = vtp->restype;
				s2pool_concat(dst, &commastr);
				arg2 = &tmpmval;
				MV_FORCE_MVAL(arg2, act);
				MV_FORCE_STR(arg2);
				s2pool_concat(dst, &arg2->str);
				s2pool_concat(dst, &commastr);
				MV_FORCE_MVAL(arg2, ver);
				MV_FORCE_STR(arg2);
				s2pool_concat(dst, &arg2->str);
			} else
			{	/* Return string "0" (no commas) to indicate nothing defined in gld for this global.
				 * Do not return "0,0,0" as this might not be distinguishable from the case where
				 * act of 0, ver of 0 was defined explicitly for this global in the gld.
				 */
				*dst = literal_zero;
			}
			break;
		case VTK_YDIRTREE:
			n = extnam_str.len;		/* internal use of op_gvname should not disturb extended reference */
			op_gvname(VARLSTCNT(1) parmblk.value);
			extnam_str.len = n;
			if (NULL != arg2)
			{
				view_arg_convert(vtp, VTP_DBREGION, arg2, &parmblk2, IS_DOLLAR_VIEW_TRUE);
				reg = parmblk2.gv_ptr;
				/* Determine if "reg" is mapped to by global name. If not issue error */
				gvnh_reg = TREF(gd_targ_gvnh_reg);	/* set up by op_gvname */
				gvspan = (NULL == gvnh_reg) ? NULL : gvnh_reg->gvspan;
				if (((NULL != gvspan) && !gvnh_spanreg_ismapped(gvnh_reg, gd_header, reg))
					|| ((NULL == gvspan) && (reg != gv_cur_region)))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GBLNOMAPTOREG, 4,
						parmblk.value->str.len, parmblk.value->str.addr, REG_LEN_STR(reg));
				}
				if (NULL != gvspan)
					GV_BIND_SUBSREG(gd_header, reg, gvnh_reg);
			}
			assert(INVALID_GV_TARGET == reset_gv_target);
			reset_gv_target = gv_target;
			gv_target = cs_addrs->dir_tree;	/* Trick the get program into using the directory tree */
			op_fngvget(dst);
			RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
			break;
		case VTK_YGDS2GVN:
			n = (NULL != arg2) ? mval2i(arg2) : 0;
			key = gds2gvn(arg1, &buff[0], n);
			COPY_ARG_TO_STRINGPOOL(dst, key, &buff[0]);
			break;
		case VTK_YGVN2GDS:
			n = (NULL != arg2) ? mval2i(arg2) : 0;
			gvkey = &save_currkey[0];
			key = gvn2gds(arg1, gvkey, n);
			/* If input has error at some point, copy whatever subscripts (+ gblname) have been successfully parsed */
			COPY_ARG_TO_STRINGPOOL(dst, key, &gvkey->base[0]);
			break;
		case VTK_YLCT:
			n = -1;
			if (arg1)
			{
				if (0 == MEMCMP_LIT(arg1->str.addr, "nct"))
					n = TREF(local_coll_nums_as_strings) ? 1 : 0;
				else if (0 == MEMCMP_LIT(arg1->str.addr, "ncol"))
					n = TREF(local_collseq_stdnull);
			} else
				n = TREF(local_collseq) ? (TREF(local_collseq))->act : 0;
			break;
		case VTK_ZDEFBUFF:
			n = 0;
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			reg = parmblk.gv_ptr;
			if (dba_cm == REG_ACC_METH(reg))
				n = ((link_info *)reg->dyn.addr->cm_blk->usr)->buffer_size;
			break;
		case VTK_ZDEFCNT:
			n = 0;
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			reg = parmblk.gv_ptr;
			if (dba_cm == REG_ACC_METH(reg))
				n = ((link_info *)reg->dyn.addr->cm_blk->usr)->buffered_count;
			break;
		case VTK_ZDEFSIZE:
			n = 0;
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr, NULL);
			reg = parmblk.gv_ptr;
			if (dba_cm == REG_ACC_METH(reg))
				n = ((link_info *)reg->dyn.addr->cm_blk->usr)->buffer_used;
			break;
		case VTK_ZDIR_FORM:
			n = zdir_form;
			break;
		case VTK_FILLFACTOR:
			n = gv_fillfactor;
			break;
		case VTK_MAXSOCKETS:
			n = gtm_max_sockets;
			break;
		case VTK_LVCREF:
			lv = (lv_val *)parmblk.value;
			assert(LV_IS_BASE_VAR(lv));
			n = lv->stats.crefcnt;
			break;
		case VTK_LVREF:
			lv = (lv_val *)parmblk.value;
			assert(LV_IS_BASE_VAR(lv));
			n = lv->stats.trefcnt;
			break;
		case VTK_LVGCOL:
			n = als_lvval_gc();
			break;
		case VTK_IMAGENAME:
			dst->str.len = gtmImageNames[image_type].imageNameLen;
			dst->str.addr = gtmImageNames[image_type].imageName;
			break;
		case VTK_LOGTPRESTART:
			n = TREF(tprestart_syslog_delta);
			break;
		case VTK_LOGNONTP:
			n = TREF(nontprestart_log_delta);
			break;
		case VTK_JNLERROR:
			n = TREF(error_on_jnl_file_lost);
			break;
		case VTK_LINK:
			assert((0 <= TREF(relink_allowed)) && (TREF(relink_allowed) < LINK_MAXTYPE));
			dst->str = relink_allowed_mstr[TREF(relink_allowed)];
			s2pool(&dst->str);
			break;
		case VTK_DMTERM:
			n = dmterm_default;
			break;
		case VTK_STATSHARE:
			n = TREF(statshare_opted_in) ? TRUE : FALSE;
			break;
		default:
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VIEWFN);
	}
	dst->mvtype = vtp->restype;
	if (MV_NM == vtp->restype)
		MV_FORCE_MVAL(dst, n);
}
