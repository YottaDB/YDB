/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

GBLREF spdesc		stringpool;
GBLREF int4		cache_hits, cache_fails;
GBLREF unsigned char	*stackbase, *stacktop;
GBLREF gd_addr		*gd_header;
GBLREF gd_binding	*gd_map;
GBLREF gd_binding	*gd_map_top;
GBLREF boolean_t	certify_all_blocks;
GBLREF sgmnt_addrs	*cs_addrs;
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

error_def(ERR_VIEWFN);

LITREF gtmImageName	gtmImageNames[];
LITREF mval		literal_zero;
LITREF mval		literal_one;

void	op_fnview(UNIX_ONLY_COMMA(int numarg) mval *dst, ...)
{
	va_list		var;
	VMS_ONLY(int	numarg;)
	mval		*keyword;
	mval		*arg;
	mstr		tmpstr;
	int		n;
	mval	        v;
	viewparm	parmblk;
	gd_binding	*map;
	gd_region	*reg;
	sgmnt_addrs	*csa;
	short		tl, newlevel;
	tp_frame	*tf;
	viewtab_entry	*vtp;
	collseq		*csp;
	lv_val		*lv;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VMS_ONLY(va_count(numarg));
	if (numarg < 2)
		GTMASSERT;
	VAR_START(var, dst);
	keyword = va_arg(var, mval *);
	MV_FORCE_STR(keyword);
	numarg -= 2;	/* remove destination and keyword from count */
	if (numarg > 0)
	{
		arg = va_arg(var, mval *);
		MV_FORCE_STR(arg);
	} else
		arg = (mval *)NULL;
	va_end(var);
	vtp = viewkeys(&keyword->str);
	view_arg_convert(vtp, arg, &parmblk);
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
			switch (parmblk.gv_ptr->dyn.addr->acc_meth)
			{
				case dba_mm:
					tmpstr.addr = "MM";
					tmpstr.len = SIZEOF("MM")-1;
					break;
				case dba_bg:
					tmpstr.addr = "BG";
					tmpstr.len = SIZEOF("BG")-1;
					break;
				case dba_cm:
					tmpstr.addr = "CM";
					tmpstr.len = SIZEOF("CM")-1;
					break;
				case dba_usr:
					tmpstr.addr = "USR";
					tmpstr.len = SIZEOF("USR")-1;
					break;
				default:
					GTMASSERT;
			}
			dst->str = tmpstr;
			break;
		case VTK_FULLBOOL:
			switch (TREF(gtm_fullbool))
			{
				case GTM_BOOL:
					tmpstr.addr = "GT.M Boolean short-circuit";
					tmpstr.len = SIZEOF("GT.M Boolean short-circuit")-1;
					break;
				case FULL_BOOL:
					tmpstr.addr = "Standard Boolean evaluation side effects";
					tmpstr.len = SIZEOF("Standard Boolean evaluation side effects")-1;
					break;
				case FULL_BOOL_WARN:
					tmpstr.addr = "Boolean side-effect warning";
					tmpstr.len = SIZEOF("Boolean side-effect warning")-1;
					break;
				default:
					GTMASSERT;
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
			if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
				gvinit();
			tmpstr.addr = (char *)gd_header->regions->rname;
			tmpstr.len = gd_header->regions->rname_len;
			s2pool(&tmpstr);
			dst->str = tmpstr;
			break;
		case VTK_GVNEXT:
			assert(gd_header);
			if (arg->str.len)
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
#ifdef		DEBUG
		case VTK_PROBECRIT:
			if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
				gvinit();
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
			reg = parmblk.gv_ptr;
			grab_crit(reg);
			if (!WBTEST_ENABLED(WBTEST_HOLD_CRIT_ENABLED))
				rel_crit(reg);
			break;
#endif
		case VTK_REGION:
			/* if gd_header is null then get the current one, and update the gd_map */
			if (!gd_header)
			{
				SET_GD_HEADER(v);
				SET_GD_MAP;
			}
			DEBUG_ONLY(else GD_HEADER_ASSERT);
			map = gd_map;
			map++;	/* get past local locks */
			for (; memcmp(parmblk.ident.c, map->name, SIZEOF(mident_fixed)) >= 0; map++)
				assert(map < gd_map_top);
			reg = map->reg.addr;
			tmpstr.addr = (char *)reg->rname;
			tmpstr.len = reg->rname_len;
			s2pool(&tmpstr);
			dst->str = tmpstr;
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
#				define GVSTATS_MAX_DIGITS	MAX_DIGITS_IN_INT8
#				define GVSTATS_KEYWORD_SIZE	5		/* THREE PLUS TWO DELIMITERS */
#				define GVSTATS_KEYWORD_COUNT	n_gvstats_rec_types
#				define GVSTATS_MAX_SIZE (GVSTATS_KEYWORD_COUNT * (GVSTATS_MAX_DIGITS + GVSTATS_KEYWORD_SIZE))
				ENSURE_STP_FREE_SPACE(GVSTATS_MAX_SIZE);
				dst->str.addr = (char *)stringpool.free;
				/* initialize cnl->gvstats_rec.db_curr_tn field from file header */
				csa->nl->gvstats_rec.db_curr_tn = csa->hdr->trans_hist.curr_tn;
#				define GVSTATS_PUT_PARM(TXT, CNTR, cnl)						\
				{										\
					MEMCPY_LIT(stringpool.free, TXT);					\
					stringpool.free += STR_LIT_LEN(TXT);					\
					*stringpool.free++ = ':';						\
					stringpool.free = i2ascl(stringpool.free, cnl->gvstats_rec.CNTR);	\
					*stringpool.free++ = ',';						\
				}
#				define TAB_GVSTATS_REC(COUNTER,TEXT1,TEXT2)	GVSTATS_PUT_PARM(TEXT1, COUNTER, csa->nl)
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
			if (csp)
			{
				n = (*csp->version)();
				n &= 0x000000FF;	/* make an unsigned char, essentially */
			} else
				n = -1;
			break;
		case VTK_YDIRTREE:
			op_gvname(VARLSTCNT(1) parmblk.value);
			assert(INVALID_GV_TARGET == reset_gv_target);
			reset_gv_target = gv_target;
			gv_target = cs_addrs->dir_tree;	/* Trick the get program into using the directory tree */
			op_gvget(dst);
			RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
			break;
		case VTK_YLCT:
			n = -1;
			if (arg)
			{
				if (0 == MEMCMP_LIT(arg->str.addr, "nct"))
					n = TREF(local_coll_nums_as_strings) ? 1 : 0;
				else if (0 == MEMCMP_LIT(arg->str.addr, "ncol"))
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
			if (dba_cm == reg->dyn.addr->acc_meth)
				n = ((link_info *)reg->dyn.addr->cm_blk->usr)->buffer_size;
			break;
		case VTK_ZDEFCNT:
			n = 0;
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
			reg = parmblk.gv_ptr;
			if (dba_cm == reg->dyn.addr->acc_meth)
				n = ((link_info *)reg->dyn.addr->cm_blk->usr)->buffered_count;
			break;
		case VTK_ZDEFSIZE:
			n = 0;
			assert(gd_header);
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
			reg = parmblk.gv_ptr;
			if (dba_cm == reg->dyn.addr->acc_meth)
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
#		ifndef VMS
		case VTK_JNLERROR:
			n = TREF(error_on_jnl_file_lost);
			break;
#		endif
		default:
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VIEWFN);
	}
	dst->mvtype = vtp->restype;
	if (MV_NM == vtp->restype)
		MV_FORCE_MVAL(dst, n);
}
