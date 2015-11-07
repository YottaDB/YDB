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
#include "outofband.h"
#include "curr_dev_outbndset.h"
#include "deferred_events.h"

GBLREF bool	std_dev_outbnd;

void curr_dev_outbndset(int4 ob_char)
{

	if (ob_char > MAXOUTOFBAND)
	{
		GTMASSERT;
	}
	else
	{	std_dev_outbnd = FALSE;
		ctrap_set(ob_char);
	}
}
