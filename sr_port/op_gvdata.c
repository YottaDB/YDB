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
	mint			x;
	gvnh_reg_t		*gvnh_reg;
	enum db_acc_method	acc_meth;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();
	acc_meth = REG_ACC_METH(gv_cur_region);
	if (IS_ACC_METH_BG_OR_MM(acc_meth))
	{
		gvnh_reg = TREF(gd_targ_gvnh_reg);
		if (NULL == gvnh_reg)
			x = (gv_target->root ? gvcst_data() : 0);
		else
			INVOKE_GVCST_SPR_XXX(gvnh_reg, x = gvcst_spr_data());
	} else if (REG_ACC_METH(gv_cur_region) == dba_cm)
		x = gvcmx_data();
	else
		x = gvusr_data();
	*v = *fndata_table[x / 10][x & 1];
}
