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
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_gblmod prototype */
#include "sgnl.h"

LITREF mval *fnzqgblmod_table[2];

GBLREF gv_namehead 	*gv_target;
GBLREF gd_region	*gv_cur_region;

void op_fnzqgblmod(mval *v)
{
	bool	gblmod;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();

	gblmod = TRUE;

	if (NULL != gv_cur_region)
	{
		if (IS_REG_BG_OR_MM(gv_cur_region))
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
