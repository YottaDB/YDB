/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

LITREF	mval		literal_null;

/* This code is very similar to op_fngvget1.c except if the gvn is undefined, this one returns the default empty string
 * while op_fngvget1 returns an undefined value as a signal to op_fnget2, which, in turn, returns a specified "default" string;
 * that slight of hand deals with order of evaluation issues.
 * Any changes to this routine most likely have to be made in op_fngvget1.c as well.
 */
void op_fngvget(mval *dst)
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
			gotit = gv_target->root ? gvcst_get(dst) : FALSE;
			break;
		case dba_cm :
			gotit = gvcmx_get(dst);
			break;
		default :
			;
			if (gotit = gvusr_get(dst))	/* NOTE: assignment */
				s2pool(&dst->str);
			break;
	}
	if (!gotit)
		*dst = literal_null;
	assert(0 == (dst->mvtype & MV_ALIASCONT));	/* Should be no alias container flag */
	return;
}
