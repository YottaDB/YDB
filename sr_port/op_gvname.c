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


#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "mv_stent.h"
#include "op.h"
#include "gvcst_root_search.h"
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "sgnl.h"
#include "mvalconv.h"
#include "tp_set_sgm.h"
#include <varargs.h>

GBLDEF bool		gv_curr_subsc_null;
GBLDEF gd_addr		*gd_targ_addr = 0;

GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_binding	*gd_map;
GBLREF sgm_info		*first_sgm_info;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF short		dollar_tlevel;
GBLREF bool		transform;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;
GBLREF mstr		extnam_str;

void op_gvname(va_alist)
va_dcl
{
	int		count, len;
	bool		was_null, is_null;
	boolean_t	bgormm;
	mval		*val, *temp;
	short int	max_key;
	va_list		var;
	unsigned char	buff[MAX_KEY_SZ + 1], *end;

	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	extnam_str.len = 0;
	if (!gd_header)
		gvinit();
	gd_targ_addr = gd_header;
	VAR_START(var);
	count = va_arg(var, int) - 1;
	val = va_arg(var, mval *);
	if ((gd_header->maps == gd_map) && gv_currkey && (0 == gv_currkey->base[val->str.len]) &&
			(0 == memcmp(gv_currkey->base, val->str.addr, val->str.len)))
	{
		gv_currkey->end = val->str.len + 1;
		gv_currkey->base[gv_currkey->end] = 0;
		gv_currkey->prev = 0;
		bgormm = ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth));
		if (bgormm)
		{
			if (dollar_tlevel && !first_sgm_info)
				tp_set_sgm();
			assert(INVALID_GV_TARGET != gv_target);
			if ((!gv_target->root) || (DIR_ROOT == gv_target->root))
				gvcst_root_search();
		}
	} else
	{
		gv_bind_name(gd_header, &(val->str));
		DEBUG_ONLY(
			bgormm = ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth));)
	}
	assert(!bgormm || (gv_cur_region && &FILE_INFO(gv_cur_region)->s_addrs == cs_addrs && cs_addrs->hdr == cs_data));
	assert(bgormm || !dollar_tlevel);
	assert(!dollar_tlevel || sgm_info_ptr && sgm_info_ptr->gv_cur_region == gv_cur_region);
	assert(gd_targ_addr == gd_header);
	was_null = is_null = FALSE;
	max_key = gv_cur_region->max_key_size;
	for (;  count > 0;  count--)
	{
		was_null |= is_null;
		val = va_arg(var, mval *);
		if (val->mvtype & MV_SUBLIT)
		{
			is_null = ((STR_SUB_PREFIX == *(unsigned char *)val->str.addr) && (KEY_DELIMITER == *(val->str.addr + 1)));
			if (gv_target->collseq || gv_target->nct)
			{
				PUSH_MV_STENT(MVST_MVAL);
				temp = &mv_chain->mv_st_cont.mvs_mval;
				transform = FALSE;
				end = gvsub2str((uchar_ptr_t)val->str.addr, buff, FALSE);
				transform = TRUE;
				temp->mvtype = MV_STR;
				temp->str.addr = (char *)buff;
				temp->str.len = end - buff;
				mval2subsc(temp, gv_currkey);
				POP_MV_STENT(); /* temp */
			} else
			{
				len = val->str.len;
				if (gv_currkey->end + len - 1 >= max_key)
				{
					if (0 == (end = format_targ_key(buff, MAX_KEY_SZ + 1, gv_currkey, TRUE)))
						end = &buff[MAX_KEY_SZ];
					rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
				}
				memcpy((gv_currkey->base + gv_currkey->end), val->str.addr, len);
				gv_currkey->prev = gv_currkey->end;
				gv_currkey->end += len - 1;
			}
		} else
		{
			mval2subsc(val, gv_currkey);
			if (gv_currkey->end >= max_key)
			{
				if (0 == (end = format_targ_key(buff, MAX_KEY_SZ + 1, gv_currkey, TRUE)))
					end = &buff[MAX_KEY_SZ];
				rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
			}
			is_null = (MV_IS_STRING(val) && (0 == val->str.len));
		}
	}
	gv_curr_subsc_null = is_null;
	if (was_null && !gv_cur_region->null_subs)
		sgnl_gvnulsubsc();
	return;
}
