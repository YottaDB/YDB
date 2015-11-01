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

#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"

#include "io.h"

GBLREF mstr	sys_input;
GBLREF mstr	sys_output;

bool io_is_tt(char *name)
{

	int    file_des;

	if (memcmp(name, sys_input.addr, sys_input.len) == 0)
	{
		file_des = 0;
	}
	else if (memcmp(name, sys_output.addr, sys_output.len) == 0)
	{
		file_des = 1;
	}
	if (isatty(file_des))
		return (TRUE);
	else
		return (FALSE);
}
