/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
GBLREF bool		gv_curr_subsc_null;

void op_fngvget(mval *v, mval *def)
{
	bool gotit;

	if (gv_curr_subsc_null && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();

	gotit = FALSE;
	if (gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm)
	{	if (gv_target->root)
		{	gotit = gvcst_get(v);
		}
	}else if (gv_cur_region->dyn.addr->acc_meth == dba_cm)
	{	gotit = gvcmx_get(v);
	}else
	{	gotit = gvusr_get(v);
		if (gotit)
			s2pool(&v->str);
	}

	if (!gotit)
	{	*v = *def;
	}
	return;
}
