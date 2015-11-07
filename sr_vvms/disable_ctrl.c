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

void disable_ctrl(mask_addr, ctrl_mask)
    uint4   *mask_addr;
    int4	    *ctrl_mask;

    {
	lib$disable_ctrl(mask_addr, ctrl_mask);
    }
