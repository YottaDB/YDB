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

#include "error.h"
#include "gvzwrite_clnup.h"

CONDITION_HANDLER(gvzwrite_ch)
{
	START_CH;
	gvzwrite_clnup();	/* this routine is called by gvzwr_fini() too */
	NEXTCH;
}

