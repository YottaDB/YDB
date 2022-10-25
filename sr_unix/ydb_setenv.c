/****************************************************************
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "ydb_setenv.h"

/* This function sets the env var with a name stored in "name" to the value stored in "value" */
void	ydb_setenv(mval *name, mval *value)
{
	char	*envvarname, *envvarvalue;
	int	status;

	assert(MV_IS_STRING(name));	/* caller should ensure this */
	assert(name->str.len);		/* caller should ensure this */
	/* "setenv" needs null-terminated strings whereas the strings pointed to by the "mval" are not null-terminated.
	 * So take a copy in temporarily malloced memory and free it up before returning.
	 */
	/* Set up the env var name first */
	envvarname = malloc(name->str.len + 1);	/* + 1 for null terminated string, needed by "setenv" */
	memcpy(envvarname, name->str.addr, name->str.len);
	envvarname[name->str.len] = '\0';
	/* Set up the env var value next */
	MV_FORCE_STR(value);
	envvarvalue = malloc(value->str.len + 1); /* + 1 for null terminated string, needed by "setenv" */
	if (value->str.len)
		memcpy(envvarvalue, value->str.addr, value->str.len);
	envvarvalue[value->str.len] = '\0';
	status = setenv(envvarname, envvarvalue, TRUE);
	if (-1 == status)
	{
		int	save_errno;

		save_errno = errno;
		free(envvarname);
		free(envvarvalue);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_SETENVFAIL, 2, name->str.len, name->str.addr,
				ERR_SYSCALL, 5, RTS_ERROR_LITERAL("setenv()"), CALLFROM, save_errno);
	}
	free(envvarname);
	free(envvarvalue);
}
