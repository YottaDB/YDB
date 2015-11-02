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
#include "gvcst_protos.h"	/* for gvcst_data prototype */
#include "gvcmx.h"
#include "gvusr.h"
#include "sgnl.h"
#include "op.h"

GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;

LITREF mval		*fndata_table[2][2];

void op_gvdata(mval *v)
{
	mint x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();

	x = 0;
	if (gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm)
	{
		if (gv_target->root)
			x = gvcst_data();
	} else if (gv_cur_region->dyn.addr->acc_meth == dba_cm)
		x = gvcmx_data();
	else
		x = gvusr_data();
	*v = *fndata_table[x / 10][x & 1];
}
