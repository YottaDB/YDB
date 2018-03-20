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

#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_syslog.h"

#include "get_syslog_flags.h"

#define LOGLOCATION		"/dev/log"
#define LOGFLAGSWDEVLOG		(LOG_PID | LOG_CONS | LOG_NOWAIT)
#define LOGFLAGSWOUTDEVLOG	(LOG_PID | LOG_PERROR | LOG_NOWAIT)

/* Routine to check for the existence of /dev/log and if it exists, use LOG_CONS, else use LOG_PERROR */
int get_syslog_flags(void)
{
	struct stat	buf;
	int		rc;

	rc = stat(LOGLOCATION, &buf);
	if (0 == rc)
		return LOGFLAGSWDEVLOG;
	return LOGFLAGSWOUTDEVLOG;
}
