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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gbldirnam.h"
#include "op.h"
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "dpgbldir.h"
#include "sgnl.h"
#include "mvalconv.h"
#include <varargs.h>

GBLREF bool		gv_curr_subsc_null;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		transform;
GBLREF gd_addr		*gd_targ_addr;
GBLREF mstr             extnam_str;

static int	extnam_str_alloc = 0;
static mstr	gtmgbldir_mstr;

void op_gvextnam(va_alist)
va_dcl
{
	va_list		var;
	int		count, len;
	bool		was_null, is_null;
	mstr		*tmp_mstr_ptr;
	mval		*val;
	short		max_key;
	unsigned char	buff[MAX_KEY_SZ + 1], *end;

	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);

	VAR_START(var);
	count = va_arg(var,int4);
	val = va_arg(var, mval *);
	MV_FORCE_STR(val);
	assert(!extnam_str_alloc || (NULL != extnam_str.addr));
	if (val->str.len)
		tmp_mstr_ptr = &val->str;
	else if (gtmgbldir_mstr.len)
		tmp_mstr_ptr = &gtmgbldir_mstr;
	else
	{
		gtmgbldir_mstr.addr = DEF_GDR;
		gtmgbldir_mstr.len = sizeof(DEF_GDR) - 1;
		tmp_mstr_ptr = get_name(&gtmgbldir_mstr);
	}
	extnam_str.len = tmp_mstr_ptr->len;
	if (extnam_str.len > extnam_str_alloc)
	{
		if (NULL != extnam_str.addr)
			free(extnam_str.addr);
		extnam_str_alloc = extnam_str.len;
		extnam_str.addr = (char *)malloc(extnam_str_alloc);
	}
	memcpy(extnam_str.addr, tmp_mstr_ptr->addr, tmp_mstr_ptr->len);
	gd_targ_addr = zgbldir(val);
	if (gv_target)
	{
		assert(INVALID_GV_TARGET != gv_target);
		gv_target->clue.end = 0;
	}
	val = va_arg(var, mval *);	/* ignore 2nd argument of ^[x,y]z */
	val = va_arg(var, mval *);
	if (!MV_IS_STRING(val))
		GTMASSERT;
	gv_bind_name(gd_targ_addr, &(val->str));
	was_null = is_null = FALSE;
	max_key = gv_cur_region->max_key_size;
	for (count -= 3;  count > 0;  count--)
	{
		was_null |= is_null;
		val = va_arg(var, mval *);
		if (val->mvtype & MV_SUBLIT)
		{
			is_null = ((STR_SUB_PREFIX == *(unsigned char *)val->str.addr) && (KEY_DELIMITER == *(val->str.addr + 1)));
			if (gv_target->collseq)
			{
				mval temp;

				transform = FALSE;
				end = gvsub2str((uchar_ptr_t)val->str.addr, buff, FALSE);
				transform = TRUE;
				temp.mvtype = MV_STR;
				temp.str.addr = (char *)buff;
				temp.str.len = end - buff;
				mval2subsc(&temp, gv_currkey);
			} else
			{
				len = val->str.len;
				if (gv_currkey->end + len - 1 >= max_key)
				{
					if (0 == (end = format_targ_key(buff, MAX_KEY_SZ + 1, gv_currkey, TRUE)))
						end = &buff[MAX_KEY_SZ];
					rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff , buff);
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
				rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff , buff);
			}
			is_null = (MV_IS_STRING(val) && (0 == val->str.len));
		}
	}
	gv_curr_subsc_null = is_null;
	if (was_null && (FALSE == gv_cur_region->null_subs))
		sgnl_gvnulsubsc();
	return;
}
