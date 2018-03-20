/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_syslog.h"

#include "libyottadb_int.h"
#include "get_syslog_flags.h"

#define YDBNOTACTIVE "YDB-F-YDBNOTACTIVE The ydb_fork_n_core() facility is not available until the YottaDB runtime "	\
		     "is initialized - Core generation request denied\n"

GBLREF	boolean_t	gtm_startup_active;

/* Externalized wrapper for gtm_fork_n_core() routine that creates a core of a running YottaDB initialized
 * process. The key is "initialized". If YottaDB has not yet been initialized in the process where this
 * call is made by a call to either a call-ins routine or to a simpleAPI routine, the core cannot be
 * created as requested. This is because gtm_fork_n_core() may invoke error or messaging routines that
 * require the YottaDB runtime to be initialized. Consequently, any routine that is called that could
 * potentially invoke this routine as the result of an error should make sure it has the runtime environment
 * fully initialized. It is inappropriate to expect this routine to run that initialization prior to creating
 * a core as it is usually called when the situation is already bad and running something as large as YottaDB
 * initialization is almost certain to make it worse.
 *
 * Note no errors are raised by gtm_fork_n_core() and no user parameter pointers are being handled so no
 * condition handler wrapper has been added. It also already protects itself against being re-entered after
 * ydb_exit() because gtm_startup_active is cleared in that case.
 */

void ydb_fork_n_core(void)
{
	if (!gtm_startup_active)
	{
		OPENLOG("YottaDB/SimpleAPI", get_syslog_flags(), LOG_USER);
		syslog(LOG_USER | LOG_INFO, YDBNOTACTIVE);
		closelog();
		fprintf(stderr, YDBNOTACTIVE);		/* TODO - review - echo to stderr as well? Or no? */
		return;
	}
	gtm_fork_n_core();
}
