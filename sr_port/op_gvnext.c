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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_order prototype */
#include "gvsub2str.h"
#include "gvcmx.h"
#include "gvusr.h"

GBLREF gv_key		*gv_altkey;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF spdesc		stringpool;

/****************** SHOULD BE IN .H FILES !!!!!!!! ***********************/
#define MAX_NUM_SLEN		128
/*************************************************************************/

void op_gvnext(mval *v)
{
	boolean_t		found;
	enum db_acc_method 	acc_meth;
	int4			n;
	register char		*c;
	gvnh_reg_t		*gvnh_reg;
	mstr			opstr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	acc_meth = REG_ACC_METH(gv_cur_region);
	/* if the lowest subscript is -1, then make it null */
	if ((gv_currkey->end == gv_currkey->prev + 4)
		&& *(gv_currkey->base + gv_currkey->prev) == 0x40
		&& *(gv_currkey->base + gv_currkey->prev + 1) == 0xEE
		&& *(gv_currkey->base + gv_currkey->prev + 2) == 0xFF)
	{
		*(gv_currkey->base + gv_currkey->prev) = 01;
		*(char *)(gv_currkey->base + gv_currkey->prev + 2) = KEY_DELIMITER;
		if (0 == gv_cur_region->std_null_coll)
		{
			*(char *)(gv_currkey->base + gv_currkey->prev + 1) = KEY_DELIMITER;
			gv_currkey->end -= 2;
		} else
		{
			*(char *)(gv_currkey->base + gv_currkey->prev + 1) = 1;
			*(char *)(gv_currkey->base + gv_currkey->prev + 3) = KEY_DELIMITER;
			gv_currkey->end --;
		}
	} else
	{
		if (!TREF(gv_last_subsc_null) || gv_cur_region->std_null_coll)
		{
			GVKEY_INCREMENT_ORDER(gv_currkey);
		} else
			*(gv_currkey->base + gv_currkey->prev) = 01;
	}
	if (IS_ACC_METH_BG_OR_MM(acc_meth))
	{
		gvnh_reg = TREF(gd_targ_gvnh_reg);
		if (NULL == gvnh_reg)
			found = (gv_target->root ? gvcst_order() : FALSE);
		else
			INVOKE_GVCST_SPR_XXX(gvnh_reg, found = gvcst_spr_order());
	} else if (acc_meth == dba_cm)
		found = gvcmx_order();
	else
		found = gvusr_order();
	v->mvtype = 0; /* so stp_gcol, if invoked below, can free up space currently occupied by this to-be-overwritten mval */
	if (!found)
	{
		ENSURE_STP_FREE_SPACE(2);
		c = v->str.addr = (char *) stringpool.free;
		*c++ = '-';
		*c = '1';
	 	v->str.len = 2;
		stringpool.free += 2;

	} else
	{
		gv_altkey->prev = gv_currkey->prev;
		if (!IS_STP_SPACE_AVAILABLE(MAX_KEY_SZ + 1))
		{
			if (*(&gv_altkey->base[0] + gv_altkey->prev) != 0xFF)
				n = MAX_NUM_SLEN;
			else
			{
				n = gv_altkey->top - gv_altkey->prev;
				assert (n > 0);
			}
			ENSURE_STP_FREE_SPACE(n);
		}
		v->str.addr = (char *)stringpool.free;
		c = (char *)(&gv_altkey->base[0] + gv_altkey->prev);
		opstr.addr = v->str.addr;
		opstr.len = MAX_ZWR_KEY_SZ;
		stringpool.free = gvsub2str((uchar_ptr_t)c, &opstr, FALSE);
		v->str.len = INTCAST(stringpool.free - (unsigned char *) v->str.addr);
		assert (v->str.addr < (char *) stringpool.top && v->str.addr >= (char *) stringpool.base);
		assert (v->str.addr + v->str.len <= (char *) stringpool.top
			&& v->str.addr + v->str.len >= (char *) stringpool.base);

	}
	v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
	if ((2 == v->str.len) && ('-' == *v->str.addr) && ('1' == *(v->str.addr + 1)))
	{	/* adjust so $REFERENCE would be correct */
		gv_currkey->end = gv_currkey->prev;
		*(gv_currkey->base + gv_currkey->end++) = 0x40;
		*(gv_currkey->base + gv_currkey->end++) = 0xEE;
		*(gv_currkey->base + gv_currkey->end++) = 0xFF;
		*(gv_currkey->base + gv_currkey->end++) = KEY_DELIMITER;
		*(gv_currkey->base + gv_currkey->end) = KEY_DELIMITER;
	}
	return;
}
