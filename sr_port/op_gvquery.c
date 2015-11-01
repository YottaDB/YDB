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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_query.h"
#include "format_targ_key.h"
#include "gvcmx.h"
#include "gvusr.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF spdesc		stringpool;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		gv_curr_subsc_null;
GBLREF mstr             extnam_str;

#define LCL_BUF_SIZE 256

void op_gvquery (mval *v)
{
	int4			size;
	unsigned char		buff[LCL_BUF_SIZE], *end;
 	bool			found;
	enum db_acc_method 	acc_meth;
	char			*extnamsrc, *extnamdst, *extnamtop;
	int			maxlen;
	char			extnamdelim[] = "^|\"\"|";

	acc_meth = gv_cur_region->dyn.addr->acc_meth;
	if (gv_curr_subsc_null)
		*(&gv_currkey->base[0] + gv_currkey->prev) = 01;
	else
	{	*(&gv_currkey->base[0] + gv_currkey->end) = 1;
		*(&gv_currkey->base[0] + gv_currkey->end + 1) = 0;
		*(&gv_currkey->base[0] + gv_currkey->end + 2) = 0;
		gv_currkey->end += 2;
	}
	if (acc_meth == dba_bg || acc_meth == dba_mm)
	{	if (gv_target->root == 0)	/* global does not exist */
			found = FALSE;
		else
			found = gvcst_query();
	} else if (acc_meth == dba_cm)
		found = gvcmx_query();
	else
	{
		assert(acc_meth == dba_usr);
		found = gvusr_query(v);
		s2pool(&v->str);
	}

	v->mvtype = MV_STR;
	if (!found)
		v->str.len = 0;
	else
	{	if (acc_meth != dba_usr)
		{
			if ((end = format_targ_key(&buff[0], LCL_BUF_SIZE, gv_altkey, TRUE)) == 0)
				end = &buff[LCL_BUF_SIZE - 1];
			size = end - &buff[0];
			/* you need to return a double-quote for every single-quote. assume worst case. */
			maxlen = size + (!extnam_str.len ? 0 : ((extnam_str.len * 2) + sizeof(extnamdelim)));
			if ((stringpool.top - stringpool.free) < maxlen)
				stp_gcol(maxlen);
			extnamdst = v->str.addr = (char *)stringpool.free;
			*extnamdst++ = extnamdelim[0];
			if (extnam_str.len > 0)
			{
				*extnamdst++ = extnamdelim[1];
				*extnamdst++ = extnamdelim[2];
				extnamsrc = &extnam_str.addr[0];
				extnamtop = extnamsrc + extnam_str.len;
				for ( ; extnamsrc < extnamtop; )
				{
					*extnamdst++ = *extnamsrc;
					if ('"' == *extnamsrc++)	/* caution : pointer increment side-effect */
						*extnamdst++ = '"';
				}
				*extnamdst++ = extnamdelim[3];
				*extnamdst++ = extnamdelim[4];
				extnam_str.len = 0;
			}
			memcpy(extnamdst, &buff[1], size - 1);
			v->str.len = extnamdst - v->str.addr + size - 1;
			stringpool.free += v->str.len;
	 		assert (v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
	 		assert (v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
		}
	}
	return;
}
