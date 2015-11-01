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
#include "op.h"
#include "sgnl.h"
#include "gvcst_queryget.h"
#include "gvcmx.h"
#include "stringpool.h"

GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		gv_curr_subsc_null;
GBLREF gv_key		*gv_currkey, *gv_altkey;
LITREF mval		literal_null;

boolean_t op_gvqueryget(mval *key, mval *val)
{
	boolean_t 	gotit;

	if (gv_curr_subsc_null && gv_cur_region->null_subs == FALSE)
		sgnl_gvnulsubsc();
	if (gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm)
	{
	 	if (gv_target->root == 0)		/* global does not exist */
			gotit = FALSE;
		else
		 	gotit = gvcst_queryget(val);
	} else if (gv_cur_region->dyn.addr->acc_meth == dba_cm)
		gotit = gvcmx_query(val);
	else
		GTMASSERT;
	if (gotit)
	{
		key->mvtype = MV_STR;
		key->str.addr = (char *)gv_altkey->base;
		key->str.len = gv_altkey->end + 1;
		s2pool(&key->str);
	} else
	{
		*key = literal_null;
		*val = literal_null;
	}
	return gotit;
}
