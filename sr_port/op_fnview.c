/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef EARLY_VARARGS
#include <varargs.h>
#endif

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "view.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "tp_frame.h"
#include "collseq.h"
#include "error.h"
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif
#include "op.h"
#include "patcode.h"
#include "mvalconv.h"

GBLREF spdesc		stringpool;
GBLREF int4		cache_hits, cache_fails;
GBLREF unsigned char	*stackbase, *stacktop;
GBLREF gd_addr		*gd_header;
GBLREF gd_binding	*gd_map;
GBLREF gd_binding	*gd_map_top;
GBLREF bool		certify_all_blocks;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF jnl_fence_control jnl_fence_ctl;
GBLREF bool		undef_inhibit;
GBLREF int4		break_message_mask;
GBLREF command_qualifier cmd_qlf;
GBLREF bool		lv_null_subs;
GBLREF tp_frame		*tp_pointer;
GBLREF short		dollar_tlevel;
GBLREF collseq		*local_collseq;
GBLREF int4		zdir_form;

LITREF mval literal_zero;
LITREF mval literal_one;

void	op_fnview(va_alist)
va_dcl
{
	va_list		var;
	mval		*dst;
	mval		*keyword;
	mval		*arg;
	mstr		tmpstr;
	int		n, numarg;
	viewparm	parmblk;
	gd_binding	*map;
	gd_region	*reg;
	sgmnt_addrs	*sa;
	unsigned char	tmpchar[4];	/* Text for GVSTAT */
	short		tl, newlevel;
	tp_frame	*tf;
	viewtab_entry	*vtp;
	collseq		*csp;

	error_def(ERR_VIEWFN);


	VAR_START(var);
	numarg = va_arg(var, int4);
	if (numarg < 2)
		GTMASSERT;
	dst = va_arg(var, mval *);
	keyword = va_arg(var, mval *);
	MV_FORCE_STR(keyword);
	numarg -= 2;	/* remove destination and keyword from count */
	if (numarg > 0)
	{
		arg = va_arg(var, mval *);
		MV_FORCE_STR(arg);
	} else
		arg = (mval *)NULL;
	vtp = viewkeys(&keyword->str);
	view_arg_convert(vtp, arg, &parmblk);
	switch (vtp->keycode)
	{
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
		n = stackbase - stacktop;
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
		n = stringpool.top - stringpool.base;
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
			tmpstr.len = sizeof("MM")-1;
			break;
		case dba_bg:
			tmpstr.addr = "BG";
			tmpstr.len = sizeof("BG")-1;
			break;
		case dba_cm:
			tmpstr.addr = "CM";
			tmpstr.len = sizeof("CM")-1;
			break;
		case dba_usr:
			tmpstr.addr = "USR";
			tmpstr.len = sizeof("USR")-1;
			break;
		default:
			GTMASSERT;
		}
		s2pool(&tmpstr);
		dst->str = tmpstr;
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
			sa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != sa->hdr)
				n = sa->hdr->jnl_state;
			else
				n = -1;
		}
		else
			n = 0;
		break;
	case VTK_JNLFILE:
		assert(gd_header);
		if (parmblk.gv_ptr)
		{
			if (!parmblk.gv_ptr->open)
				gv_init_reg(parmblk.gv_ptr);
			sa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
			if (NULL != sa->hdr)
				view_jnlfile(dst, parmblk.gv_ptr);
			else
				dst->str.len = 0;
		}
		else
			dst->str.len = 0;
		break;
	case VTK_JNLTRANSACTION:
		n = jnl_fence_ctl.level;
		break;
	case VTK_LABELS:
		n = (cmd_qlf.qlf & CQ_LOWER_LABELS) ? 1 : 0;
		break;
	case VTK_LVNULLSUBS:
		n = lv_null_subs;
		break;
	case VTK_NOISOLATION:
		if (NOISOLATION_NULL != parmblk.ni_list.type || NULL == parmblk.ni_list.gvnh_list
				|| NULL != parmblk.ni_list.gvnh_list->next)
			rts_error(VARLSTCNT(1) ERR_VIEWFN);
		n = parmblk.ni_list.gvnh_list->gvnh->noisolation;
		break;
	case VTK_PATCODE:
		getpattabnam(&tmpstr);
		s2pool(&tmpstr);
		dst->str = tmpstr;
		break;
	case VTK_REGION:
		assert(gd_header);
		map = gd_map;
		map++;	/* get past local locks */
		for (; memcmp(&parmblk.ident, &(map->name[0]), sizeof(mident)) >= 0; map++)
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
		sa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
		if (NULL != sa->hdr)
			n = sa->hdr->trans_hist.free_blocks;
		else
			n = -1;
		break;
	case VTK_BLTOTAL:
		assert(gd_header);
		if (!parmblk.gv_ptr->open)
			gv_init_reg(parmblk.gv_ptr);
		sa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
		if (NULL != sa->hdr)
		{
			n = sa->hdr->trans_hist.total_blks;
			n -= (n + sa->hdr->bplmap - 1) / sa->hdr->bplmap;
		} else
			n = -1;
		break;
	case VTK_FREEZE:
		assert(gd_header);
		if (!parmblk.gv_ptr->open)
			gv_init_reg(parmblk.gv_ptr);
		sa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
		if (NULL != sa->hdr)
			n = sa->hdr->freeze;
		else
			n = -1;
		break;
	case VTK_GVSTATS:
		assert(gd_header);
		if (!parmblk.gv_ptr->open)
			gv_init_reg(parmblk.gv_ptr);
		sa = &FILE_INFO(parmblk.gv_ptr)->s_addrs;
		if (NULL != sa->hdr)
		{
#define GVSTAT_MAX_DIGITS 10
#define GVSTAT_KEYWORD_SIZE 5		/* THREE PLUS TWO DELIMITERS */
#define GVSTAT_KEYWORD_COUNT 9
#define GVSTAT_MAX_SIZE ((GVSTAT_KEYWORD_COUNT + CDB_STAGNATE + ARRAYSIZE(sa->hdr->n_tp_retries) \
				+ ARRAYSIZE(sa->hdr->n_tp_retries_conflicts)) * (GVSTAT_MAX_DIGITS + GVSTAT_KEYWORD_SIZE))
			if (stringpool.top - stringpool.free < GVSTAT_MAX_SIZE)
				stp_gcol(GVSTAT_MAX_SIZE);
			dst->str.addr = (char *)stringpool.free;
#define GVSTAT_PUT_PARM(B, A) (memcpy(stringpool.free, B, sizeof(B) - 1), stringpool.free += sizeof(B) - 1,	\
	*stringpool.free++ = ':', stringpool.free = i2asc(stringpool.free, sa->hdr->A),	\
	*stringpool.free++ = ',')
			GVSTAT_PUT_PARM("SET", n_puts);
			GVSTAT_PUT_PARM("KIL", n_kills);
			GVSTAT_PUT_PARM("GET", n_gets);
			GVSTAT_PUT_PARM("ORD", n_order);
			GVSTAT_PUT_PARM("QRY", n_queries);
			GVSTAT_PUT_PARM("ZPR", n_zprevs);
			GVSTAT_PUT_PARM("DTA", n_data);
			tmpchar[0] = 'R', tmpchar[1] = 'T', tmpchar[3] = 0;
			for (n  = 0; n < CDB_STAGNATE; n++)
			{
				tmpchar[2] = n + '0';
				GVSTAT_PUT_PARM(tmpchar, n_retries[n]);
			}
			tmpchar[0] = 'T', tmpchar[1] = 'P', tmpchar[3] = 0;
			for (n  = 0; n < ARRAYSIZE(sa->hdr->n_tp_retries); n++)
			{
				assert(n < 10);
				tmpchar[2] = n + '0';
				GVSTAT_PUT_PARM(tmpchar, n_tp_retries[n]);
			}
			tmpchar[0] = 'T', tmpchar[1] = 'C', tmpchar[3] = 0;
			for (n  = 0; n < ARRAYSIZE(sa->hdr->n_tp_retries_conflicts); n++)
			{
				assert(n < 10);
				tmpchar[2] = n + '0';
				GVSTAT_PUT_PARM(tmpchar, n_tp_retries_conflicts[n]);
			}
			assert(stringpool.free < stringpool.top);
			/* subtract one to remove extra trailing delimiter */
			dst->str.len = (char *)stringpool.free - dst->str.addr - 1;
		} else
			dst->str.len = 0;
		break;
	case VTK_TID:
		newlevel = MV_FORCE_INT(parmblk.value);
		if (!tp_pointer || newlevel <= 0)
			dst->str.len = 0;
		else
		{	tl = dollar_tlevel;
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
		RESET_GV_TARGET;
		break;
	case VTK_YLCT:
		n = local_collseq ? local_collseq->act : 0;
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
	default:
		rts_error(VARLSTCNT(1) ERR_VIEWFN);
	}
	dst->mvtype = vtp->restype;
	if (MV_NM == vtp->restype)
		MV_FORCE_MVAL(dst, n);
}
