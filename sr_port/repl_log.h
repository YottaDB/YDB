/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_LOG_H
#define _REPL_LOG_H

typedef enum
{
	REPL_GENERAL_LOG,
	REPL_STATISTICS_LOG
} repl_log_file_t ;

#define NULL_DEVICE 	"/dev/null"
#define DEVICE_PREFIX	"/dev/"

#include "gtm_stdio.h"

int repl_log_fd2fp(FILE **fp, int fd);
int repl_log(FILE *fp, boolean_t stamptime, boolean_t flush, char *fmt, ...);
int repl_log_init(repl_log_file_t log_type, int *log_fd, int *stats_fd, char *log,
	char *stats_log);

#define LOGTRNUM_INTERVAL			1000
#define GTMRECV_LOGSTATS_INTERVAL		300 /* sec */
#define GTMSOURCE_LOGSTATS_INTERVAL		300 /* sec */

#endif
