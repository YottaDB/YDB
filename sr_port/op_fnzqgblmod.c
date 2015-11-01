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

#include "hashdef.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "op.h"
#include "gvcst_gblmod.h"
#include "sgnl.h"

LITREF mval *fnzqgblmod_table[2];

GBLREF gv_namehead 	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		gv_curr_subsc_null;

void op_fnzqgblmod(mval *v)
{
	bool	gblmod;

	if (gv_curr_subsc_null && gv_cur_region->null_subs == FALSE)
		sgnl_gvnulsubsc();

	gblmod = TRUE;

	if (NULL != gv_cur_region)
	{
		if (gv_cur_region->dyn.addr->acc_meth == dba_bg  ||  gv_cur_region->dyn.addr->acc_meth == dba_mm)
		{
			if (gv_target->root)
				gblmod = gvcst_gblmod(v);
			else
				gblmod = FALSE;
		}
		else
			assert(FALSE);
	}

	*v = *fnzqgblmod_table[gblmod];
}
