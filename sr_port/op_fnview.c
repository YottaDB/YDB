/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"
#include "filestruct.h"
#include "jnl.h"
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
#ifdef UNIX
# include "gtmlink.h"
#endif
#include "gtm_ctype.h"		/* for ISDIGIT_ASCII macro */

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
UNIX_ONLY(GBLREF	boolean_t		dmterm_default;)

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_GBLNOMAPTOREG);
error_def(ERR_GVSUBSERR);
error_def(ERR_VIEWFN);
error_def(ERR_VIEWGVN);

LITREF	gtmImageName	gtmImageNames[];
LITREF	mstr		relink_allowed_mstr[];
LITREF	mval		literal_zero;
LITREF	mval		literal_one;
LITREF	mval		literal_null;

#define		MM_RES			"MM"
#define		BG_RES			"BG"
#define		CM_RES			"CM"
#define		USR_RES			"USR"
#define		GTM_BOOL_RES		"GT.M Boolean short-circuit"
#define		STD_BOOL_RES		"Standard Boolean evaluation side effects"
#define		WRN_BOOL_RES		"Standard Boolean with side-effect warning"
#define		STATS_MAX_DIGITS	MAX_DIGITS_IN_INT8
#define		STATS_KEYWD_SIZE	(3 + 1 + 1)	/* 3 character mnemonic, colon and comma */

STATICFNDCL unsigned char *gvn2gds(mval *gvn, gv_key *gvkey, int act);

#define	COPY_ARG_TO_STRINGPOOL(DST, KEYEND, KEYSTART)			\
{									\
	int	keylen;							\
									\
	keylen = (unsigned char *)KEYEND - (unsigned char *)(KEYSTART);	\
	ENSURE_STP_FREE_SPACE(keylen);					\
	assert(stringpool.top - stringpool.free >= keylen);		\
	memcpy(stringpool.free, KEYSTART, keylen);			\
	DST->mvtype = MV_STR;						\
	DST->str.len = keylen;						\
	DST->str.addr = (char *)stringpool.free;			\
	stringpool.free += keylen;					\
}

#define STATS_PUT_PARM(TXT, CNTR, BASE)					\
{									\
	MEMCPY_LIT(stringpool.free, TXT);				\
	stringpool.free += STR_LIT_LEN(TXT);				\
	*stringpool.free++ = ':';					\
	stringpool.free = i2ascl(stringpool.free, BASE.CNTR);		\
	*stringpool.free++ = ',';					\
	assert(stringpool.free <= stringpool.top);			\
}

void	op_fnview(UNIX_ONLY_COMMA(int numarg) mval *dst, ...)
{
	VMS_ONLY(int	numarg;)
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
			n = (int)(stringpool.top - stringpool.base);
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
				gv_init_reg(parmblk.gv_ptr);
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
				gv_init_reg(parmblk.gv_ptr);
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
				parmblk.gv_ptr++;
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
					gv_init_reg(parmblk.gv_ptr);
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
					gv_init_reg(parmblk.gv_ptr);
				csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
				if (NULL != csa->hdr)
					view_jnlfile(dst, parmblk.gv_ptr);
				else
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
				gv_init_reg(reg);
			csa = &FILE_INFO(reg)->s_addrs;
			n = csa->gbuff_limit;
			break;
		case VTK_PROBECRIT:
			assert(NULL != gd_header);	/* view_arg_convert would have done this for VTK_POOLLIMIT */
			reg = parmblk.gv_ptr;
			if (!reg->open)
				gv_init_reg(reg);
			csa = &FILE_INFO(reg)->s_addrs;
			if (NULL != csa->hdr)
			{
				UNIX_ONLY(csa->crit_probe = TRUE);
				grab_crit(reg);
				UNIX_ONLY(csa->crit_probe = FALSE);
				if (!WBTEST_ENABLED(WBTEST_HOLD_CRIT_ENABLED))
					rel_crit(reg);
				dst->str.len = 0;
#				ifdef UNIX
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
#				endif
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
			gvkey = &save_currkey[0];
			key = gvn2gds(arg1, gvkey, n);
			assert(key > &gvkey->base[0]);
			assert(gvkey->end == key - &gvkey->base[0] - 1);
			start_map = gv_srch_map(gd_header, (char *)&gvkey->base[0], gvkey->end - 1); /* -1 to remove trailing 0 */
			GVKEY_INCREMENT_ORDER(gvkey);
			end_map = gv_srch_map(gd_header, (char *)&gvkey->base[0], gvkey->end - 1); /* -1 to remove trailing 0 */
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
				gv_init_reg(parmblk.gv_ptr);
			csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != csa->hdr)
				n = csa->hdr->trans_hist.free_blocks;
			else
				n = -1;
			break;
		case VTK_BLTOTAL:
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
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
				gv_init_reg(parmblk.gv_ptr);
			csa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != csa->hdr)
				n = csa->hdr->freeze;
			else
				n = -1;
			break;
		case VTK_GVSTATS:
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
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
			op_gvname(VARLSTCNT(1) parmblk.value);
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
			if (NULL != arg2)
			{
				n = mval2i(arg2);
				if (0 != n)
				{
					csp = ready_collseq(n);
					if (NULL == csp)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, n);
						break;
					}
				} else
					csp = NULL;	/* Do not issue COLLATIONUNDEF for 0 collation */
			}
			/* Temporarily repoint global variables "gv_target" and "transform".
			 * They are needed by format_targ_key/gvsub2str "transform" and "gv_target->collseq".
			 */
			save_transform = TREF(transform);
			assert(save_transform);
			TREF(transform) = TRUE;
			reset_gv_target = gv_target;
			gv_target = &temp_gv_target;
			memset(gv_target, 0, SIZEOF(gv_namehead));
			if (NULL != arg2)
				gv_target->collseq = csp;
			assert(MV_IS_STRING(arg1));
			gvkey = &save_currkey[0];
			gvkey->prev = 0;
			gvkey->top = DBKEYSIZE(MAX_KEY_SZ);
			if ((gvkey->top < arg1->str.len) || (2 > arg1->str.len)
					|| (KEY_DELIMITER != arg1->str.addr[arg1->str.len-1])
					|| (KEY_DELIMITER != arg1->str.addr[arg1->str.len-2]))
				*dst = literal_null;
			else
			{
				memcpy(&gvkey->base[0], arg1->str.addr, arg1->str.len);
				DEBUG_ONLY(gvkey->end = arg1->str.len - 1;)	/* for an assert in format_targ_key */
				if (0 == (c = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gvkey, FALSE)))
					c = &buff[MAX_ZWR_KEY_SZ - 1];
				COPY_ARG_TO_STRINGPOOL(dst, c, &buff[0]);
			}
			/* Restore global variables "gv_target" and "transform" back to their original state */
			RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
			TREF(transform) = save_transform;
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
				gv_init_reg(parmblk.gv_ptr);
			reg = parmblk.gv_ptr;
			if (dba_cm == REG_ACC_METH(reg))
				n = ((link_info *)reg->dyn.addr->cm_blk->usr)->buffer_size;
			break;
		case VTK_ZDEFCNT:
			n = 0;
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
			reg = parmblk.gv_ptr;
			if (dba_cm == REG_ACC_METH(reg))
				n = ((link_info *)reg->dyn.addr->cm_blk->usr)->buffered_count;
			break;
		case VTK_ZDEFSIZE:
			n = 0;
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
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
#		ifdef UNIX
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
#		endif
		default:
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VIEWFN);
	}
	dst->mvtype = vtp->restype;
	if (MV_NM == vtp->restype)
		MV_FORCE_MVAL(dst, n);
}

/* Converts a GVN in string representation into a key in subscript representation.
 * Note: This code is very similar to "is_canonic_name()". With some effort, they might even be merged into one.
 */
STATICFNDEF unsigned char *gvn2gds(mval *gvn, gv_key *gvkey, int act)
{
	boolean_t	save_transform, is_zchar;
	collseq		*csp;
	gd_region	tmpreg, *save_gv_cur_region;
	gv_namehead	temp_gv_target;
	int		quotestate, clen;
	int4		num;
	unsigned char	strbuff[MAX_KEY_SZ + 1], *key, *key_start, *key_top, *c, *c_top, ch, *str, *str_top, *c1, *c2, *numptr;
	char		fnname[6];	/* to hold the function name $CHAR or $ZCHAR */
	char		number[32];	/* to hold a codepoint numeric value for $char or $zchar; 32 digits is more than enough */
	mval		tmpmval, *mvptr, dollarcharmval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 != act)
	{
		csp = ready_collseq(act);
		if (NULL == csp)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, act);
	} else
		csp = NULL;	/* Do not issue COLLATIONUNDEF for 0 collation */
	assert(MV_IS_STRING(gvn));
	c = (unsigned char *)gvn->str.addr;
	c_top = c + gvn->str.len;
	key_start = &gvkey->base[0];
	key = key_start;
	if ((c >= c_top) || ('^' != *c++))
		return key;
	gvkey->prev = 0;
	gvkey->top = DBKEYSIZE(MAX_KEY_SZ);
	key_top = key_start + gvkey->top;
	/* Parse GBLNAME */
	for ( ; (c < c_top) && (key < key_top); )
	{
		if ('(' == *c)
			break;
		*key++ = *c++;
	}
	if (key >= key_top)
		return key_start;
	if (c == c_top)
	{
		*key++ = KEY_DELIMITER;
		gvkey->end = key - key_start;
		*key++ = KEY_DELIMITER;
		return key;
	}
	assert('(' == *c);
	c++; /* skip past "(" */
	*key++ = KEY_DELIMITER;
	gvkey->end = key - key_start;
	str = &strbuff[0];
	str_top = str + ARRAYSIZE(strbuff);
	/* Temporarily repoint global variables "gv_cur_region", "gv_target" and "transform".
	 * They are needed by mval2subsc for the following
	 *	"transform", "gv_target->nct", "gv_target->collseq" and "gv_cur_region->std_null_coll"
	 */
	save_transform = TREF(transform);
	assert(save_transform);
	TREF(transform) = TRUE;
	reset_gv_target = gv_target;
	gv_target = &temp_gv_target;
	memset(gv_target, 0, SIZEOF(gv_namehead));
	gv_target->collseq = csp;
	save_gv_cur_region = gv_cur_region;
	gv_cur_region = &tmpreg;
	memset(gv_cur_region, 0, SIZEOF(gd_region));
	/* Note that the primary caller of the YGVN2GDS functionality is going to be GDE for globals that
	 * span regions. And since such globals need to reside in regions that have standard null collation
	 * defined, we set the std_null_coll field to TRUE above.
	 */
	gv_cur_region->std_null_coll = TRUE;
	/* Parse subscripts */
	quotestate = 0;
	for ( ; (c < c_top) && (str < str_top); c++)
	{
		ch = *c;
		switch (quotestate)
		{
			case 0:
			case 6:
				if (('"' == ch) || ('$' == ch))
				{	/* start of a string subscript */
					if (0 == quotestate)
					{
						tmpmval.mvtype = (MV_STR | MV_NUM_APPROX);
							/* MV_NUM_APPROX needed by mval2subsc to skip val_iscan call */
						str = &strbuff[0];
						tmpmval.str.addr = (char *)str;
					}
					quotestate = ('"' == ch) ? 1 : 3;
				} else if (6 == quotestate)
				{	/* Defer rts_error until after global variables "gv_cur_region" etc. are restored. */
					quotestate = -1;/* error in input */
					c = c_top;	/* do not parse remaining input as subscripted gvn is complete now */
				} else
				{	/* quotestate is 0, in this case this is the start of a number */
					quotestate = 4;	/* start of a number */
					tmpmval.mvtype = MV_STR;
					tmpmval.str.addr = (char *)c;
				}
				break;
			case 1:
				if ('"' == ch)
					quotestate = 2;
				else
					*str++ = ch;	/* and quotestate stays at 1 */
				break;
			case 2:
			case 9:
				if ((2 == quotestate) && ('"' == ch))
				{
					*str++ = '"';
					quotestate = 1;
					break;
				} else if (')' == ch)
					quotestate = 5;
				else if (',' == ch)
					quotestate = 0;
				else if ('_' == ch)
				{
					quotestate = 6;
					break;
				} else
				{	/* Defer rts_error until after global variables "gv_cur_region" etc. are restored. */
					quotestate = -1;	/* error in input */
					c = c_top;		/* force break from for loop */
					break;
				}
				assert((')' == ch) || (',' == ch));
				tmpmval.str.len = str - (unsigned char *)tmpmval.str.addr;
				DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = TRUE;)
				mval2subsc(&tmpmval, gvkey, gv_cur_region->std_null_coll);
				DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = FALSE;)
				assert(gvkey->end < gvkey->top); /* else GVSUBOFLOW error would have been issued */
				key = &gvkey->base[gvkey->end];
				break;
			case 3:
				/* Allow only one of $C( or $CHAR( or $ZCH( or $ZCHAR( */
				c1 = c;
				for ( ; (c < c_top) && ('(' != *c); c++)
					;
				if (c == c_top)
					break;
				clen = c - c1;
				if (clen >= ARRAYSIZE(fnname))
				{
					c = c_top;	/* force break from for loop */
					break;		/* bad function name. issue error after breaking from for loop */
				}
				for (c2 = (unsigned char *)&fnname[0]; c1 < c; c2++, c1++)
					*c2 = TOUPPER(*c1);
				if (!MEMCMP_LIT(fnname, "ZCHAR") || !MEMCMP_LIT(fnname, "ZCH"))
					is_zchar = 1;
				else if (!MEMCMP_LIT(fnname, "CHAR") || !MEMCMP_LIT(fnname, "C"))
					is_zchar = 0;
				else
				{
					c = c_top;	/* force break from for loop */
					break;		/* bad function name. issue error after breaking from for loop */
				}
				assert('(' == *c);
				quotestate = 7;
				break;
			case 4:
				if (',' == ch)
					quotestate = 0;
				else if (')' == ch)
					quotestate = 5;
				else
					break;
				tmpmval.str.len = c - (unsigned char *)tmpmval.str.addr;
				mvptr = &tmpmval;
				MV_FORCE_NUM(mvptr);
				if (MVTYPE_IS_NUM_APPROX(tmpmval.mvtype))
				{	/* User specified either a non-numeric or an imprecise numeric.
					 * Defer rts_error until after global variables "gv_cur_region" etc. are restored.
					 */
					quotestate = -1;/* error in input */
					c = c_top;	/* do not parse remaining input as subscripted gvn is complete now */
					break;
				}
				mval2subsc(&tmpmval, gvkey, gv_cur_region->std_null_coll);
				assert(gvkey->end < gvkey->top); /* else GVSUBOFLOW error would have been issued */
				key = &gvkey->base[gvkey->end];
				break;
			case 5:
				/* Defer rts_error until after global variables "gv_cur_region" etc. are restored. */
				quotestate = -1;/* error in input */
				c = c_top;	/* do not parse remaining input as subscripted gvn is complete now */
				break;
			case 7:
				if (!ISDIGIT_ASCII(ch))
				{	/* Not an ascii numeric digit.
					 * Defer rts_error until after global variables "gv_cur_region" etc. are restored.
					 */
					quotestate = -1;/* error in input */
					c = c_top;	/* do not parse remaining input as subscripted gvn is complete now */
					break;
				}
				numptr = c;	/* record start of number */
				quotestate = 8;
				break;
			case 8:
				if (')' == ch)
					quotestate = 9;
				else if (',' == ch)
					quotestate = 7;
				else if (!ISDIGIT_ASCII(ch))
				{	/* Not an ascii numeric digit
					 * Defer rts_error until after global variables "gv_cur_region" etc. are restored.
					 */
					quotestate = -1;/* error in input */
					c = c_top;	/* do not parse remaining input as subscripted gvn is complete now */
					break;
				} else
					break;	/* continue processing numeric argument to $c or $zch */
				/* end of the $c() number. find its zchar value */
				if ((c - numptr) >= ARRAYSIZE(number))
				{	/* number specified to $c or $zch is more than 32 digits long. error out */
					c = c_top;	/* force break from for loop */
					break;		/* bad function name. issue error after breaking from for loop */
				}
				memcpy(number, numptr, c - numptr);
				number[c - numptr] = '\0';
				num = (int4)STRTOUL(number, NULL, 10);
				if (0 > num)
				{	/* number is negative. issue error */
					c = c_top;	/* force break from for loop */
					break;		/* bad function name. issue error after breaking from for loop */
				}
#				ifdef UNICODE_SUPPORTED
				if (!is_zchar && is_gtm_chset_utf8)
					op_fnchar(2, &dollarcharmval, num);
				else
#				endif
					op_fnzchar(2, &dollarcharmval, num);
				assert(MV_IS_STRING(&dollarcharmval));
				if (dollarcharmval.str.len)
				{
					if (str + dollarcharmval.str.len > str_top)
					{	/* String overflows capacity.
						 * Defer rts_error until after global variables "gv_cur_region" etc. are restored.
						 */
						quotestate = -1;/* error in input */
						c = c_top; /* do not parse remaining input as subscripted gvn is complete now */
						break;
					}
					memcpy(str, dollarcharmval.str.addr, dollarcharmval.str.len);
					str += dollarcharmval.str.len;
				}
				break;
			default:
				assertpro(FALSE && quotestate);
				break;
		}
	}
	/* Restore global variables "gv_cur_region", "gv_target" and "transform" back to their original state */
	gv_cur_region = save_gv_cur_region;
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
	TREF(transform) = save_transform;
	if ((str >= str_top) || !CAN_APPEND_HIDDEN_SUBS(gvkey))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, gvn->str.len, gvn->str.addr);
	if (5 == quotestate)
		*key++ = KEY_DELIMITER;	/* add double terminating null byte */
	else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVSUBSERR);
	assert(key <= key_top);
	return key;
}
