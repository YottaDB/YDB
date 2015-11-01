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
#include "dskspace_msg_timer.h"
#include "gt_timer.h"

GBLREF volatile uint4 dskspace_msg_counter;

void dskspace_msg_timer(void)
{
	dskspace_msg_counter++;
}

