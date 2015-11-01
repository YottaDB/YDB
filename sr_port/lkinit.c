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

GBLDEF bool remlkreq=FALSE;
GBLDEF mlk_pvtblk *mlk_pvt_root = 0;
GBLDEF unsigned short lks_this_cmd;
GBLDEF unsigned char cm_action;
