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
#include "subscript.h"
#include "op.h"
#include "gvcst_root_search.h"
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "sgnl.h"
#include "mvalconv.h"
#include "tp_set_sgm.h"
#include <varargs.h>

GBLREF short            dollar_tlevel;
GBLREF gd_addr		*gd_header;
GBLREF bool		gv_curr_subsc_null;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF sgm_info         *first_sgm_info;
GBLREF bool		transform;
GBLREF mstr		extnam_str;

void op_gvnaked(va_alist)
va_dcl
{
	va_list		var;
	int		count, len;
	bool		was_null, is_null, sbs_cnt;
	mval		*val;
	short 		max_key;
	unsigned char	*ptr, *end_ptr;
	unsigned char	buff[MAX_KEY_SZ + 1], *end;

	error_def(ERR_GVNAKED);
	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);
	error_def(ERR_MAXNRSUBSCRIPTS);

	extnam_str.len = 0;
	if (!gv_currkey || (0 == gv_currkey->prev))
		rts_error(VARLSTCNT(1) ERR_GVNAKED);

	if ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth))
	{
		assert(INVALID_GV_TARGET != gv_target);
                if (dollar_tlevel && !first_sgm_info)
			tp_set_sgm();
		if (!gv_target->root || (DIR_ROOT == gv_target->root))
			gvcst_root_search();
	}

	VAR_START(var);
	gv_currkey->end = gv_currkey->prev;
	is_null = FALSE;
	was_null = FALSE;
	max_key = gv_cur_region->max_key_size;
	sbs_cnt = 0;
	count = va_arg(var, int4);
	if (1 < count)
	{
		/* Use of naked reference can cause increase in number of subscripts.   So count the subscripts */
		ptr = &gv_currkey->base[0];
		end_ptr = ptr + gv_currkey->end;
		while (ptr < end_ptr)
			if (KEY_DELIMITER == *ptr++)
				sbs_cnt++;
		if (MAX_GVSUBSCRIPTS < count + sbs_cnt)
			rts_error(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	}
	/* else naked reference will not increase number of subscripts, so do not worry about exceeding the limit */
	for (; count > 0 ; count--)
	{
		sbs_cnt++;
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
	if (was_null && (FALSE == gv_cur_region->null_subs))
		sgnl_gvnulsubsc();
	return;
}
