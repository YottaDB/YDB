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

#include <lckdef.h>
#include <psldef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "locks.h"

void	gtm_blkast(gd_region *region)
{
	vms_gds_info	*vbi;

	if (region != NULL)
	{
		vbi = FILE_INFO(region);
		gtm_deq(vbi->file_cntl_lsb.lockid, NULL, PSL$C_USER, LCK$M_CANCEL);
		gtm_enqw(EFN$C_ENF, LCK$K_PWMODE, &vbi->file_cntl_lsb, LCK$M_CONVERT, NULL, 0,
			 NULL, 0, NULL, PSL$C_USER, 0);
		/* Update the lock value block and reestablish the blocking AST */
		vbi->file_cntl_lsb.valblk[0] = region->node;
		gtm_enq(0, LCK$K_CRMODE, &vbi->file_cntl_lsb, LCK$M_CONVERT | LCK$M_VALBLK, NULL, 0,
			NULL, region, gtm_blkast, PSL$C_USER, 0);
	}
}
