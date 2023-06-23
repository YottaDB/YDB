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
#include "gtm_stdio.h"
#include "iosp.h"
#include "mupip_exit.h"
#include "mupip_quit.h"

void mupip_quit(void)
{	PRINTF("\n");
	mupip_exit(SS_NORMAL);
}
