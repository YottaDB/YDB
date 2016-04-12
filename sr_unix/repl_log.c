/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include <errno.h>
#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtmio.h"
#include "have_crit.h"

#include "repl_log.h"
#include "iosp.h"

GBLREF int		gtmsource_log_fd;
GBLREF int		gtmrecv_log_fd;
GBLREF int		updproc_log_fd;

GBLREF FILE		*gtmsource_log_fp;
GBLREF FILE		*gtmrecv_log_fp;
GBLREF FILE		*updproc_log_fp;

int repl_log(FILE *fp, boolean_t stamptime, boolean_t flush, char *fmt, ...)
{
	va_list printargs;
	char	time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro */
	char	fmt_str[BUFSIZ];
	int	rc;

	assert(NULL != fp);
	if (stamptime)
	{
		GET_CUR_TIME(time_str);
		strcpy(fmt_str, time_str);
		fmt_str[CTIME_BEFORE_NL] = ' '; /* Overwrite \n */
		fmt_str[CTIME_BEFORE_NL + 1] = ':';
		fmt_str[CTIME_BEFORE_NL + 2] = ' ';
		strcpy(fmt_str + CTIME_BEFORE_NL + 3, fmt);
		fmt = &fmt_str[0];
	}

	va_start(printargs, fmt);
	VFPRINTF(fp, fmt, printargs, rc);
	assert(0 <= rc);
	va_end(printargs);

	if (flush)
		FFLUSH(fp);

	return(SS_NORMAL);
}
