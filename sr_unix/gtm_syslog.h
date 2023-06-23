/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

/* gtm_syslog.h - interlude to <syslog.h> system header file.  */
#ifndef GTM_SYSLOGH
#define GTM_SYSLOGH

#include <syslog.h>

#include "have_crit.h"

/* Assert that we are never inside an OS signal handler if any of the syslog() functions are invoked as they have the
 * potential of causing a hang (YDB#464).
 */
#define OPENLOG		assert(!in_os_signal_handler); openlog
#define SYSLOG		assert(!in_os_signal_handler); syslog
#define CLOSELOG	assert(!in_os_signal_handler); closelog

#endif
