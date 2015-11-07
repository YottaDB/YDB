/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>
#include "mupip_exit.h"

GBLREF	boolean_t	mupip_exit_status_displayed;

void mupip_exit(int4 stat)
{
	mupip_exit_status_displayed = TRUE;
	sys$exit(stat ? stat : SS$_NORMAL);
}
