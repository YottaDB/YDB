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

#ifndef SIGNAL_EXIT_HANDLER_INCLUDED
#define SIGNAL_EXIT_HANDLER_INCLUDED

#define	IS_DEFERRED_EXIT_FALSE	FALSE
#define	IS_DEFERRED_EXIT_TRUE	TRUE

void signal_exit_handler(char *exit_handler_name, int sig, siginfo_t *info, void *context, boolean_t is_deferred_exit);

#endif /* SIGNAL_EXIT_HANDLER_INCLUDED */
