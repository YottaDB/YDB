/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* gcc/LinuxIA32 needs stdio before varargs until stdio.h removed from error.h */
/* gcc/Linux390 needs varargs before stdio */
#ifdef EARLY_VARARGS
#include <varargs.h>
#endif
#ifdef __GNUC__
#include "gtm_stdio.h"
#endif
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmd_qlf.h"
#include "collseq.h"
#include "error.h"
#include "hashdef.h"		/* needed for mname */
#include "jnl.h"
#include "lv_val.h"
#include "view.h"
#include "send_msg.h"
#include "op.h"
#include "change_reg.h"
#include "patcode.h"
#include "mprof.h"
#include "cmidef.h"
#include "gvcmz.h"
#include "testpt.h"
#include "mvalconv.h"
#include "dpgbldir.h"	/* for get_next_gdr() prototype */

#define WRITE_LITERAL(x) (outval.str.len = sizeof(x) - 1, outval.str.addr = (x), op_write(&outval))

/* if changing noisolation status within TP and already referenced the global, then error */
#define SET_GVNH_NOISOLATION_STATUS(gvnh, status)							\
{													\
	if (!dollar_tlevel || gvnh->read_local_tn != local_tn || status == gvnh->noisolation)		\
		gvnh->noisolation = status;								\
	else												\
		rts_error(VARLSTCNT(6) ERR_ISOLATIONSTSCHNG, 4, mid_len(&gvnh->gvname), &gvnh->gvname,	\
				gvnh->noisolation, status);						\
}

GBLREF bool		certify_all_blocks, lv_null_subs, undef_inhibit, jobpid;
GBLREF bool		view_debug1, view_debug2, view_debug3, view_debug4;
GBLREF bool		zdefactive;
GBLREF unsigned short	zdefbufsiz;
GBLREF int4		break_message_mask;
GBLREF collseq		*local_collseq;
GBLREF char		*lcl_coll_xform_buff;
GBLREF command_qualifier cmd_qlf, glb_cmd_qlf;
GBLREF gd_addr		*gd_header;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target, *gv_target_list;
GBLREF gv_namehead	*reset_gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF symval		*curr_symval;
GBLREF trans_num	local_tn;	/* transaction number for THIS PROCESS */
GBLREF short		dollar_tlevel;
GBLREF boolean_t	lv_dupcheck;
GBLREF int4		zdate_form;
GBLREF int4		zdir_form;

#define MAX_YDIRTSTR 32
#define ZDEFMIN 1024
#define ZDEFDEF 32767
#define ZDEFMAX 65535

void	op_view(va_alist)
va_dcl
{
	int4		testvalue;
	uint4		jnl_status;
	int		status, numarg;
	gd_region	*reg, *r_top, *save_reg;
	gv_namehead	*gvnh;
	mval 		*arg, *keyword, outval;
	mstr		tmpstr;
	va_list		var;
	viewparm	parmblk;
	viewtab_entry	*vtp;
	gd_addr		*addr_ptr;
	noisolation_element	*gvnh_entry;

	static int ydirt_str_len = 0;
	static char ydirt_str[MAX_YDIRTSTR + 1];
	static readonly char msg1[] = "Caution: Database Block Certification Has Been ";
	static readonly char msg2[] = "Disabled";
	static readonly char msg3[] = "Enabled";
	static readonly char lv_msg1[] =
			"Caution: Sanchez reserved local variable string pointer duplicate check diagnostic has been";
	static readonly char upper[] = "UPPER";
	static readonly char lower[] = "LOWER";

	error_def(ERR_VIEWCMD);
	error_def(ERR_ZDEFACTIVE);
	error_def(ERR_PATTABNOTFND);
	error_def(ERR_PATLOAD);
	error_def(ERR_YDIRTSZ);
	error_def(ERR_REQDVIEWPARM);
	error_def(ERR_ACTRANGE);
	error_def(ERR_COLLATIONUNDEF);
	error_def(ERR_COLLDATAEXISTS);
	error_def(ERR_ISOLATIONSTSCHNG);
	error_def(ERR_TRACEON);
	error_def(ERR_INVZDIRFORM);

	VAR_START(var);
	numarg = va_arg(var, int4);
	jnl_status = 0;
	if (numarg < 1)
		GTMASSERT;
	keyword = va_arg(var, mval *);
	MV_FORCE_STR(keyword);
	numarg--;	/* remove keyword from count */
	if (numarg > 0)
	{
		arg = va_arg(var, mval *);
		MV_FORCE_STR(arg);
	} else
		arg = (mval *)0;
	vtp = viewkeys(&keyword->str);
	view_arg_convert(vtp, arg, &parmblk);
	switch(vtp->keycode)
	{
	case VTK_BREAKMSG:
		break_message_mask = MV_FORCE_INT(parmblk.value);
		break;
	case VTK_DEBUG1:
		outval.mvtype = MV_STR;
		view_debug1 = (0 != MV_FORCE_INT(parmblk.value));
		break;
	case VTK_DEBUG2:
		outval.mvtype = MV_STR;
		view_debug2 = (0 != MV_FORCE_INT(parmblk.value));
		break;
	case VTK_DEBUG3:
		outval.mvtype = MV_STR;
		view_debug3 = (0 != MV_FORCE_INT(parmblk.value));
		break;
	case VTK_DEBUG4:
		outval.mvtype = MV_STR;
		view_debug4 = (0 != MV_FORCE_INT(parmblk.value));
		break;
	case VTK_GDSCERT0:
		outval.mvtype = MV_STR;
		op_wteol(1);
		WRITE_LITERAL(msg1);
		certify_all_blocks = FALSE;
		WRITE_LITERAL(msg2);
		op_wteol(1);
		break;
	case VTK_GDSCERT1:
		outval.mvtype = MV_STR;
		op_wteol(1);
		WRITE_LITERAL(msg1);
		certify_all_blocks = TRUE;
		WRITE_LITERAL(msg3);
		op_wteol(1);
		break;
	case VTK_GDSCERT:
		outval.mvtype = MV_STR;
		op_wteol(1);
		WRITE_LITERAL(msg1);
		certify_all_blocks = (0 != MV_FORCE_INT(parmblk.value));
		if (certify_all_blocks)
			WRITE_LITERAL(msg3);
		else
			WRITE_LITERAL(msg2);
		op_wteol(1);
		break;
	case VTK_LVDUPCHECK:
		outval.mvtype = MV_STR;
		op_wteol(1);
		WRITE_LITERAL(lv_msg1);
		lv_dupcheck = (0 != MV_FORCE_INT(parmblk.value));
		if (lv_dupcheck)
			WRITE_LITERAL(msg3);
		else
			WRITE_LITERAL(msg2);
		op_wteol(1);
		break;
	case VTK_LVNULLSUBS:
		lv_null_subs = TRUE;
		break;
	case VTK_NOLVNULLSUBS:
		lv_null_subs = FALSE;
		break;
	case VTK_JNLFLUSH:
		if (NULL == gd_header)		/* open gbldir */
			gvinit();
		save_reg = gv_cur_region;
		if (NULL == parmblk.gv_ptr)
		{	/* flush all journal files */
			gv_cur_region = gd_header->regions;
			r_top = gv_cur_region + gd_header->n_regions - 1;
		} else	/* flush journal for selected region */
			r_top = gv_cur_region = parmblk.gv_ptr;
		for (;  gv_cur_region <= r_top;  gv_cur_region++)
		{
			if (!gv_cur_region->open)
				gv_init_reg(gv_cur_region);
			change_reg();
			if (JNL_ENABLED(cs_addrs->hdr))
			{
				if (FALSE == cs_addrs->now_crit)
					grab_crit(gv_cur_region);
				else
					GTMASSERT;
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
					jnl_flush(gv_cur_region);
				else
				{
					send_msg(VARLSTCNT(6) jnl_status, 4, cs_addrs->hdr->jnl_file_len,
						cs_addrs->hdr->jnl_file_name, DB_LEN_STR(gv_cur_region));
				}
				rel_crit(gv_cur_region);
			}
		}
		gv_cur_region = save_reg;
		change_reg();
		break;
	case VTK_JNLWAIT:
		/* go through all regions that could have possibly been open across all global directories */
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
			for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions;  reg < r_top;  reg++)
				jnl_wait(reg);
		break;
	case VTK_JOBPID:
		outval.mvtype = MV_STR;
		jobpid = (0 != MV_FORCE_INT(parmblk.value));
		break;
	case VTK_LABELS:
		if ((sizeof(upper) - 1) == parmblk.value->str.len)
		{
			if (!memcmp(upper, parmblk.value->str.addr, sizeof(upper) - 1))
				glb_cmd_qlf.qlf &= ~CQ_LOWER_LABELS;
			else  if (!memcmp(lower, parmblk.value->str.addr, sizeof(lower) - 1))
				glb_cmd_qlf.qlf |= CQ_LOWER_LABELS;
			cmd_qlf.qlf = glb_cmd_qlf.qlf;
		}
		break;
	case VTK_NOISOLATION:
		if (NOISOLATION_NULL == parmblk.ni_list.type)
		{
			for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
				SET_GVNH_NOISOLATION_STATUS(gvnh, FALSE);
			parmblk.ni_list.type = NOISOLATION_PLUS;
		}
		assert(NOISOLATION_PLUS == parmblk.ni_list.type || NOISOLATION_MINUS == parmblk.ni_list.type);
		if (NOISOLATION_PLUS == parmblk.ni_list.type)
		{
			for (gvnh_entry = parmblk.ni_list.gvnh_list; gvnh_entry; gvnh_entry = gvnh_entry->next)
				SET_GVNH_NOISOLATION_STATUS(gvnh_entry->gvnh, TRUE);
		} else
		{
			for (gvnh_entry = parmblk.ni_list.gvnh_list; gvnh_entry; gvnh_entry = gvnh_entry->next)
				SET_GVNH_NOISOLATION_STATUS(gvnh_entry->gvnh, FALSE);
		}
		break;
	case VTK_PATCODE:
		if (arg)
			tmpstr = parmblk.value->str;
		else
			tmpstr.len = 0;
		status = setpattab(&tmpstr);
		if (0 == status)
			rts_error(VARLSTCNT(4) ERR_PATTABNOTFND, 2, parmblk.value->str.len, parmblk.value->str.addr);
		break;
	case VTK_YDIRTVAL:
		/* This is an internal use only call to VIEW which is used to modify directory
		tree entries.  It places a string in in the static array ydirt_str.  A subsequent
		view to YDIRTREE causes the value to be put in the directory tree.
		For example VIEW "YDIRTVAL":$C(55,0,0,0),"YDIRTREE":XYZ would cause global ^XYZ
		to have a root block of 0x00000055, on a little-endian computer.

		Note that it is easy to corrupt a database with these calls.

		The use of the "Y" sentinal character indicates that the VIEW keyword is interal
		Greystone use only, and that Greystone will not provide upward compatibility to
		the user. These keywords should NOT be used outside of Greystone's software
		engineering department.
		*/
		ydirt_str_len = parmblk.value->str.len;
		if (ydirt_str_len > MAX_YDIRTSTR)
			rts_error(VARLSTCNT(1) ERR_YDIRTSZ);
		if (ydirt_str_len > 0)
			memcpy(ydirt_str, parmblk.value->str.addr, ydirt_str_len);
		break;
	case VTK_YDIRTREE:
		/* See comment under YDIRTVAL above
		Restriction: This program will update an existing entry in a directory tree but
		will fail if the entry is not present.  So make sure that the global exists before
		a YDIRTREE update is performed
		*/
		op_gvname(VARLSTCNT(1) parmblk.value);
		assert(INVALID_GV_TARGET == reset_gv_target);
		reset_gv_target = gv_target;
		gv_target = cs_addrs->dir_tree;		/* Trick the put program into using the directory tree */
		outval.mvtype = MV_STR;
		outval.str.len = ydirt_str_len;
		outval.str.addr = (char *)ydirt_str;
		op_gvput(&outval);
		RESET_GV_TARGET;
		gv_target->root = 0;
		break;
	case VTK_YLCT:
		{
		int	lct;
		collseq	*new_lcl_collseq;
		if (arg)
			lct = MV_FORCE_INT(parmblk.value);
		else
			rts_error(VARLSTCNT(1) ERR_REQDVIEWPARM);
		if (lct < MIN_COLLTYPE || lct > MAX_COLLTYPE)
			rts_error(VARLSTCNT(3) ERR_ACTRANGE, 1, lct);
		/* at this point, verify that there is no local data with subscripts */
		{
			ht_entry *p, *top;
			lv_sbs_tbl *tbl;
			lv_val *lv;
			symval *cstab;
			for (cstab = curr_symval;  cstab;  cstab = cstab->last_tab)
			{
				p = cstab->h_symtab.base;
				top = p + cstab->h_symtab.size;
				for (;  p < top;  p++)
				{
					if (p->nb.txt[0])
					{
						lv = (lv_val *)p->ptr;
						if (lv && lv->ptrs.val_ent.children)
							rts_error(VARLSTCNT(1) ERR_COLLDATAEXISTS);
					}
				}
			}
		}
		if (0 != lct)
		{
			new_lcl_collseq = ready_collseq(lct);
			if (0 == new_lcl_collseq)
				rts_error(VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, lct);
			local_collseq = new_lcl_collseq;
			if (NULL == lcl_coll_xform_buff)
				lcl_coll_xform_buff = (char *)malloc(MAX_LCL_COLL_XFORM_BUFSIZ);
		} else
		{
			local_collseq = 0;
			if (NULL != lcl_coll_xform_buff)
			{
				free(lcl_coll_xform_buff);
				lcl_coll_xform_buff = NULL;
			}
		}
		break;
		}
	case VTK_PATLOAD:
		if (!load_pattern_table(parmblk.value->str.len, parmblk.value->str.addr))
			rts_error(VARLSTCNT(4) ERR_PATLOAD, 2, parmblk.value->str.len, parmblk.value->str.addr);
		break;
	case VTK_NOUNDEF:
		undef_inhibit = TRUE;
		break;
	case VTK_UNDEF:
		undef_inhibit = FALSE;
		break;
	case VTK_TESTPOINT:
		testvalue = MV_FORCE_INT(parmblk.value);
		set_testpoint(testvalue);
		break;
	case VTK_ZDEFER:
		if (zdefactive)
			rts_error(VARLSTCNT(1) ERR_ZDEFACTIVE);
		zdefactive = TRUE;
		zdefbufsiz = MV_FORCE_INT(parmblk.value);
		if (1 == zdefbufsiz)
			zdefbufsiz = ZDEFDEF;
		else  if (zdefbufsiz < ZDEFMIN)
			zdefbufsiz = ZDEFMIN;
		else  if (zdefbufsiz > ZDEFMAX)
			zdefbufsiz = ZDEFMAX;
		break;
	case VTK_ZFLUSH:
		gvcmz_zflush();
		break;
	case VTK_TRACE :
                 testvalue = MV_FORCE_INT(parmblk.value);
                 if (testvalue)
		 {
			if (numarg < 2)
				rts_error(VARLSTCNT(1) ERR_TRACEON);
			arg = va_arg(var, mval *);
			MV_FORCE_STR(arg);
                        turn_tracing_on(arg);
		 } else
                 {
			if (2 == numarg)
			{
				arg = va_arg(var, mval *);
				MV_FORCE_STR(arg);
				turn_tracing_off(arg);
			} else
			{
				turn_tracing_off(NULL);
			}
                 }
                 break;
	case VTK_ZDIR_FORM:
		 VMS_ONLY(
			if (NULL != arg)
			{
		 		testvalue = MV_FORCE_INT(parmblk.value);
				if (!IS_VALID_ZDIR_FORM(testvalue))
					rts_error(VARLSTCNT(3) ERR_INVZDIRFORM, 1, testvalue);
			} else
				testvalue = ZDIR_FORM_FULLPATH;
		 	zdir_form = testvalue;
		 )
		 break;
	default:
		rts_error(VARLSTCNT(1) ERR_VIEWCMD);
	}
	return;
}
