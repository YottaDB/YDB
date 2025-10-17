/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gdsbt.h"
#include "gdsfhead.h"
#include "op.h"
#include "glvn_pool.h"
#include "zyencode_zydecode_def.h"	/* for ARG1_LCL and ARG1_GBL */
#include "op_zyencode_zydecode.h"

void op_indzydecode2(uint4 indx)
{
	lv_val		*lv;
	opctype		oc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (OC_SAVLVN == (oc = SLOT_OPCODE(indx)))	/* note assignment */
	{	/* lvn */
		lv = op_rfrshlvn(indx, OC_PUTINDX);
		op_zydecode_arg(ARG1_LCL, lv);
	} else if (OC_NOOP != oc)
	{	/* gvn */
		op_rfrshgvn(indx, oc);
		op_zydecode_arg(ARG1_GBL, NULL);
	}
	return;
}
