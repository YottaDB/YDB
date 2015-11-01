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
#include "op.h"
#include "rtnhdr.h"
#include "gtmci.h"
#include "svnames.h"
#include "gtm_savetraps.h"

/* As of today, we do not save any intrinsic variables across multiple
   call-in environments except for the following three, so that the behavior
   is consistent with VMS.  We need further research/discussion on which
   variables to be saved and which should/need not be */
void save_intrinsic_var(void)
{
	gtm_savetraps();	/* Save either etrap or ztrap as needed */
	op_newintrinsic(SV_ESTACK);
}
