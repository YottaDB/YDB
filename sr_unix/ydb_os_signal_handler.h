/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef YDB_OS_SIGNAL_HANDLER_INCLUDED
#define YDB_OS_SIGNAL_HANDLER_INCLUDED

GBLREF	volatile int	in_os_signal_handler;

#define	IS_OS_SIGNAL_HANDLER_FALSE	FALSE
#define	IS_OS_SIGNAL_HANDLER_TRUE	TRUE

#define	INCREMENT_IN_OS_SIGNAL_HANDLER											\
{															\
	assert(0 <= in_os_signal_handler);										\
	/* No need of INTERLOCK_ADD since even with multiple-threads, only this thread owns the YDB engine lock. */	\
	in_os_signal_handler++;												\
}

#define	DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED										\
{																\
	if (in_os_signal_handler)												\
	{															\
		/* No need of INTERLOCK_ADD since even with multiple-threads, only this thread owns the YDB engine lock. */	\
		in_os_signal_handler--;												\
		assert(0 <= in_os_signal_handler);										\
	}															\
}

void ydb_os_signal_handler(int sig, siginfo_t *info, void *context);

#endif /* YDB_OS_SIGNAL_HANDLER_INCLUDED */
