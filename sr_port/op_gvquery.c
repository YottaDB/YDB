/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_query prototype */
#include "format_targ_key.h"
#include "gvcmx.h"
#include "gvusr.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF spdesc		stringpool;
GBLREF gd_region	*gv_cur_region;
GBLREF mstr             extnam_str;

void op_gvquery (mval *v)
{
	int4			size;
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end, *glob_begin;
 	boolean_t		found, ok_to_change_currkey;
	enum db_acc_method 	acc_meth;
	unsigned char		*extnamsrc, *extnamdst, *extnamtop;
	int			maxlen;
	char			extnamdelim[] = "^|\"\"|";
	mval			val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* We want to turn QUERY into QUERYGET for all types of access methods so that we can cache the value of the key returned
	 * by $QUERY. The value is very likely to be used shortly after $QUERY - Vinaya, Aug 13, 2001 */
	acc_meth = gv_cur_region->dyn.addr->acc_meth;
	/* Modify gv_currkey such that a gvcst_search of the resulting key will find the next available record in collation order.
	 * But in case of dba_usr (the custom implementation of $ORDER which is overloaded for DDP now but could be more in the
	 * future) it is better to hand over gv_currkey as it is so the custom implementation can decide what to do with it.
	 */
	ok_to_change_currkey = (dba_usr != acc_meth);
	if (ok_to_change_currkey)
	{
		if (TREF(gv_last_subsc_null) && (NEVER == gv_cur_region->std_null_coll))
		{
			assert(STR_SUB_PREFIX == gv_currkey->base[gv_currkey->prev]);
			gv_currkey->base[gv_currkey->prev] = 01;
		} else
		{	/* Note, gv_currkey->prev isn't changed here. We rely on this
			 * in gtcmtr_query to distinguish different forms of the key.
			 */
			gv_currkey->base[gv_currkey->end++]= 1;
			gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
			gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
		}
	}
	switch (acc_meth)
	{
		case dba_bg:
		case dba_mm:
			found = ((0 != gv_target->root) ? gvcst_query() : FALSE); /* global does not exist if root is 0 */
			break;
		case dba_cm:
			found = gvcmx_query(&val); /* val ignored currently - Vinaya Aug 13, 2001*/
			break;
		case dba_usr:
			found = gvusr_query(v); /* $Q result in v for dba_usr, for others, in gv_altkey */
			break;
		default:
			assert(FALSE); /* why didn't we cover all access methods? */
			found = FALSE;
			break;
	}
	if (ok_to_change_currkey)
	{	/* Restore gv_currkey to what it was at function entry time */
		if (TREF(gv_last_subsc_null) && (NEVER == gv_cur_region->std_null_coll))
		{
			assert(01 == gv_currkey->base[gv_currkey->prev]);
			gv_currkey->base[gv_currkey->prev] = STR_SUB_PREFIX;
		} else
		{
			assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
			assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 1]);
			assert(1 == gv_currkey->base[gv_currkey->end - 2]);
			assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 3]);
			gv_currkey->end -= 2;
			gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
		}
	}
	v->mvtype = 0; /* so stp_gcol (if invoked below) can free up space currently occupied by this to-be-overwritten mval */
	if (found)
	{
		if (acc_meth != dba_usr)
		{
			if ((end = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gv_altkey, TRUE)) == 0)
				end = &buff[MAX_ZWR_KEY_SZ - 1];
			size = (int)(end - &buff[0] - 1); /* exclude ^ */
			glob_begin = &buff[1]; /* skip ^ */
		} else
		{
			size = v->str.len - 1; /* exclude ^ */
			glob_begin = (unsigned char *)v->str.addr + 1; /* skip ^ */
		}
		/* Need to return a double-quote for every single-quote; assume worst case. */
		/* Account for ^ in both cases - extnam and no extnam */
		maxlen = size + ((0 == extnam_str.len) ? 1 : ((extnam_str.len * 2) + (int)(STR_LIT_LEN(extnamdelim))));
		ENSURE_STP_FREE_SPACE(maxlen);
		extnamdst = stringpool.free;
		*extnamdst++ = extnamdelim[0];
		if (extnam_str.len > 0)
		{
			*extnamdst++ = extnamdelim[1];
			*extnamdst++ = extnamdelim[2];
			for (extnamsrc = (unsigned char *)extnam_str.addr, extnamtop = extnamsrc + extnam_str.len;
					extnamsrc < extnamtop; )
			{
				*extnamdst++ = *extnamsrc;
				if ('"' == *extnamsrc++)	/* caution : pointer increment side-effect */
					*extnamdst++ = '"';
			}
			*extnamdst++ = extnamdelim[3];
			*extnamdst++ = extnamdelim[4];
			extnam_str.len = 0;
		}
		memcpy(extnamdst, glob_begin, size);
		v->str.len = INTCAST(extnamdst - stringpool.free + size);
		v->str.addr = (char *)stringpool.free;
		stringpool.free += v->str.len;
		assert (v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
		assert (v->str.addr + v->str.len <= (char *)stringpool.top &&
			v->str.addr + v->str.len >= (char *)stringpool.base);
	} else /* !found */
		v->str.len = 0;
	v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
	return;
}
