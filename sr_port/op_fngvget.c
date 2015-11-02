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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_get prototype */
#include "gvcmx.h"
#include "gvusr.h"
#include "sgnl.h"

GBLREF gv_namehead 	*gv_target;
GBLREF gd_region	*gv_cur_region;

/* This code is very similar to "op_fngvget1.c" except that this one returns the default value
 * (while op_fngvget1 returns an undefined value) if the global variable that we are trying to get is undefined.
 *
 * Any changes to this routine most likely have to be made in op_fngvget1.c as well.
 */
void op_fngvget(mval *v, mval *def)
{
	boolean_t	gotit;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();
	switch (gv_cur_region->dyn.addr->acc_meth)
	{
		case dba_bg :
		case dba_mm :
			if (gv_target->root)
				gotit = gvcst_get(v);
			else
				gotit = FALSE;
			break;
		case dba_cm :
			gotit = gvcmx_get(v);
			break;
		default :
			gotit = gvusr_get(v);
			if (gotit)
				s2pool(&v->str);
			break;
	}
	if (!gotit)
	{
		*v = *def;
		v->mvtype &= ~MV_ALIASCONT;	/* Make sure do not allow alias container property to pass */
	}
	return;
}
