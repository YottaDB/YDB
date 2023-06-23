/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "is_defunct_pid.h"

#ifdef DEBUG
GBLREF	uint4			process_id;
#endif

/* ----------------------------------------------
 * Check if input pid is a defunct process (zombie)
 *
 * Arguments:
 *	pid	- process ID
 *
 * Return:
 *	TRUE	- If process is a defunct process (i.e. shows up as <defunct> in ps -ef listing)
 *	FALSE	- Otherwise
 * ----------------------------------------------
 */

boolean_t	is_defunct_pid(int4 pid)
{
	int		status;
	char		procfilename[64];
	boolean_t	is_defunct;
	FILE		*fp;
	char		pidstate;

	assert(0 != pid);
	assert(process_id != pid);
	is_defunct = FALSE;	/* by default it is not a defunct process */
#	ifdef __linux__
	/* open the /proc/<pid>/stat file */
	SNPRINTF(procfilename, sizeof(procfilename), "/proc/%d/stat", (int)pid);
	fp = fopen(procfilename, "r");
	if (NULL != fp)
	{
		fscanf(fp, "%*d %*s %c", &pidstate);
		is_defunct = ('Z' == pidstate);
		fclose(fp);
	}
#	else
	/* This is likely MacOS or Cygwin. Those ports need to implement this functionality using other methods
	 * (maybe do a "system" call with a "ps -ef | grep defunct" if nothing else is possible).
	 */
#	error unsupported platform
#	endif
	return is_defunct;
}
