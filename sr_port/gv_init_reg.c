/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "iosp.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cryptdef.h"
#include "filestruct.h"
#include "targ_alloc.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */

GBLREF bool		licensed ;

void gv_init_reg(gd_region *reg)
{
#	ifdef NOLICENSE
	licensed = TRUE;
#	else
	CRYPT_CHKSYSTEM;
#	endif
#	ifdef DEBUG
	switch (reg->dyn.addr->acc_meth)
	{
		case dba_cm:
		case dba_mm:
		case dba_bg:
			break;
		default:
			assert(FALSE);
			break;
	}
#	endif
	if (!reg->open)
		gvcst_init(reg);
	/* A statsDB region can legitimately come back unopened, for instance when its base db carries
	 * NOSTATS and so has no statistics database to open. Callers that ask for one directly expect
	 * that and cope: "op_fnzpeek" tests "reg->open" right after calling here and issues BADZPEEKARG
	 * when it is not set. Asserting the region is open makes that test dead code in a debug build,
	 * and turns a condition the caller handles into a fatal one.
	 */
	assert(reg->open || IS_STATSDB_REG(reg));
	return;
}
