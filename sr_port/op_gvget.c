/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "gvcst_protos.h"	/* for gvcst_get prototype */
#include "gvcmx.h"
#include "gvusr.h"
#include "sgnl.h"
#include "op.h"

GBLREF	gv_namehead	*gv_target;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	bool		undef_inhibit;

LITREF	mval		literal_null;

/* From the generated code's point of view, this is a void function.
 * The defined state isn't looked at by MUMPS code.  This routine,
 * however, is used by some servers who would dearly like to know
 * the status of the get, though they don't want to cause any
 * errors.
 */
boolean_t op_gvget(mval *v)
{
	boolean_t	 gotit;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();
	if ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth))
	{
	 	if (0 == gv_target->root)		/* global does not exist */
		{	/* Assert that if gtm_gvundef_fatal is non-zero, then we better not be about to signal a GVUNDEF */
			assert(!TREF(gtm_gvundef_fatal));
			gotit = FALSE;
		} else
		{
			DEBUG_ONLY(TREF(in_op_gvget) = TRUE;)
		 	gotit = gvcst_get(v);
			assert(FALSE == TREF(in_op_gvget)); /* gvcst_get should have reset it right away */
		}
	} else  if (dba_cm == gv_cur_region->dyn.addr->acc_meth)
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
