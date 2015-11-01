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
#include "gvcst_get.h"
#include "gvcmx.h"
#include "gvusr.h"
#include "sgnl.h"
#include "op.h"

GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		gv_curr_subsc_null;
GBLREF gv_key		*gv_currkey;
GBLREF bool undef_inhibit;
LITREF mval literal_null;

/* From the generated code's point of view, this is a void function.
	The defined state isn't looked at by MUMPS code.  This routine,
	however, is used by some servers who would dearly like to know
	the status of the get, though they don't want to cause any
	errors.
*/

bool op_gvget(mval *v)
{
	bool gotit;

	if (gv_curr_subsc_null && gv_cur_region->null_subs == FALSE)
		sgnl_gvnulsubsc();
	if (gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm)
	{
	 	if (gv_target->root == 0)		/* global does not exist */
			gotit = FALSE;
		else
		 	gotit = gvcst_get(v);
	} else  if (gv_cur_region->dyn.addr->acc_meth == dba_cm)
	 	gotit = gvcmx_get(v);
	else
	{
		gotit = gvusr_get(v);
		if (gotit)
			s2pool(&v->str);
	}
	if (!gotit)
	{
		if (undef_inhibit)
			*v = literal_null;
		else
			sgnl_gvundef();
	}
	return gotit;
}
