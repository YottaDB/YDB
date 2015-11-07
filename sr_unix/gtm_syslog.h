/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_syslog.h - interlude to <syslog.h> system header file.  */
#ifndef GTM_SYSLOGH
#define GTM_SYSLOGH

#include <syslog.h>

#define OPENLOG		openlog
#define SYSLOG		syslog
#define CLOSELOG	closelog

#endif
