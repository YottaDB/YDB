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

#include "mlkdef.h"
#include "cmidef.h"
#include "hashdef.h"
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"

bool gtcml_lcktime(cm_lckblklck *lck)
{
	ABS_TIME	time_now, new_blktime;
	uint4		status;

	add_int_to_abs_time(&lck->blktime, CM_LKBLK_TIME, &new_blktime);
	sys_get_curr_time(&time_now);
	return (0 > abs_time_comp(&time_now, &new_blktime) ? FALSE : TRUE);
}
