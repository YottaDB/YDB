/****************************************************************
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "libyottadb_int.h"
#include "have_crit.h"

GBLREF	boolean_t	noThreadAPI_active;
GBLREF	boolean_t	simpleThreadAPI_active;
GBLREF	int		fork_after_ydb_init;
GBLREF 	boolean_t	ydb_init_complete;
GBLREF	int		process_exiting;

/* This is the callback function invoked by "fork" in the parent process just BEFORE the "fork" */
void	ydb_stm_atfork_prepare(void)
{
	if (process_exiting)
		return;	/* A "fork" is happening after "ydb_exit" has been called. Return right away. */
	DEFERRED_SIGNAL_HANDLING_CHECK;	/* Handle deferred signals (e.g. timer handlers), if any, BEFORE the fork */
	assert(0 <= fork_after_ydb_init);
	if (simpleThreadAPI_active || noThreadAPI_active)
	{
		fork_after_ydb_init++;	/* Record the fact that a "fork" occurred. We do a ++ to account for the fact
					 * that FORK macro calls happen more than once in ojstartchild.c to create a middle
					 * child which can then do another FORK to create a grandchild. In this case, we
					 * need to remember the FORK nesting depth hence the ++.
					 */
		assert(ydb_init_complete || fork_after_ydb_init);
	}
}

/* This is the callback function invoked by "fork" in the parent process just AFTER the "fork" */
void	ydb_stm_atfork_parent(void)
{
	if (process_exiting)
		return;	/* A "fork" is happening after "ydb_exit" has been called. Return right away. */
	/* Undo work done by "ydb_stm_atfork_prepare" */
	if (simpleThreadAPI_active || noThreadAPI_active)
	{
		assert(fork_after_ydb_init);
		fork_after_ydb_init--;
		assert(ydb_init_complete || fork_after_ydb_init);
	}
}

/* This is the callback function invoked by "fork" in the child process just AFTER the "fork" */
void	ydb_stm_atfork_child(void)
{
	if (process_exiting)
		return;	/* A "fork" is happening after "ydb_exit" has been called. Return right away. */
	if (simpleThreadAPI_active || noThreadAPI_active)
	{
		assert(ydb_init_complete || fork_after_ydb_init);
		ydb_init_complete = FALSE;	/* Force a "ydb_init" whenever a YottaDB call happens.
						 * That will check "fork_after_ydb_init" value and handle as appropriate.
						 */
		assert(fork_after_ydb_init);
		/* In case "noThreadAPI_active" is TRUE, "ydb_child_init" will reset "fork_after_ydb_init" to FALSE.
		 * In case "simpleThreadAPI_active" is TRUE, an "exec" is needed to reset "fork_after_ydb_init" to FALSE.
		 */
	}
	return;
}
