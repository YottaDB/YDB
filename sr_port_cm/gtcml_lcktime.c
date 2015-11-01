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

#include "mlkdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

GBLREF	ABS_TIME	chkreg_time;

bool gtcml_lcktime(cm_lckblklck *lck)
{
	ABS_TIME	new_blktime;
	uint4		status;

	add_int_to_abs_time(&lck->blktime, CM_LKBLK_TIME, &new_blktime);
	return (0 > abs_time_comp(&chkreg_time, &new_blktime) ? FALSE : TRUE);
}
