/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc.*
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
	REPL_GENERAL_LOG
} repl_log_file_t ;

#define NULL_DEVICE 	"/dev/null"
#define DEVICE_PREFIX	"/dev/"

#include "gtm_stdio.h"

int repl_log_fd2fp(FILE **fp, int fd);
int repl_log(FILE *fp, boolean_t stamptime, boolean_t flush, char *fmt, ...);
int repl_log_init(repl_log_file_t log_type, int *log_fd, char *log);

#define LOGTRNUM_INTERVAL 1000	/* default interval (jnlseqno count) at which source/receiver/upd log */

#ifndef GTMRECV_LOGSTATS_INTERVAL /* so we can re-default GTMRECV_LOGSTATS_INTERVAL while building without changing this file */
#define GTMRECV_LOGSTATS_INTERVAL 300 /* sec; time period at which statistics are printed by receiver (can't be changed by user) */
#endif
#ifndef GTMSOURCE_LOGSTATS_INTERVAL /* so we can re-default GTMSOURCE_LOGSTATS_INTERVAL while building without changing this file */
#define GTMSOURCE_LOGSTATS_INTERVAL 300 /* sec; time period at which statistics are printed by source (can't be changed by user) */
#endif

#define REPLIC_CHANGE_LOGFILE		0x0000001 /* bit flag to indicate log file needs to be changed (source/receiver) */
#define REPLIC_CHANGE_LOGINTERVAL	0x0000002 /* bit flag to indicate log interval needs to be changed (source/receiver) */
#define REPLIC_CHANGE_UPD_LOGINTERVAL	0x0000004 /* bit flag to indicate log interval needs to be changed (update process);
						   * separate from receiver because we may specify change in either receiver or
						   * update or both in the same command line */

#define GTMRECV_LOGINTERVAL_DELIM	','
#endif
