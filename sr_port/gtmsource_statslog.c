/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include "gtm_string.h"
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#ifdef UNIX
#include <sys/sem.h>
#endif
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;

int gtmsource_statslog(void)
{
	/* Grab the jnlpool jnlpool option write lock */
	if (0 > grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM))
	{
		util_out_print("Error grabbing jnlpool option write lock. Could not initiate stats log", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}

	if (gtmsource_options.statslog == jnlpool.gtmsource_local->statslog)
	{
		util_out_print("STATSLOG is already !AD. Not initiating change in stats log", TRUE, gtmsource_options.statslog ?
				strlen("ON") : strlen("OFF"), gtmsource_options.statslog ? "ON" : "OFF");
		rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	if (!gtmsource_options.statslog)
	{
		jnlpool.gtmsource_local->statslog = FALSE;
		jnlpool.gtmsource_local->statslog_file[0] = '\0';
		util_out_print("STATSLOG turned OFF", TRUE);
		rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (NORMAL_SHUTDOWN);
	}

	if ('\0' == gtmsource_options.log_file[0]) /* Stats log file not specified, use general log file */
	{
		util_out_print("No file specified for stats log. Using general log file !AD\n", TRUE,
				LEN_AND_STR(jnlpool.gtmsource_local->log_file));
		strcpy(gtmsource_options.log_file, jnlpool.gtmsource_local->log_file);
	} else if (0 == strcmp(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file))
	{
		util_out_print("Stats log file is already !AD. Not initiating change in log file", TRUE,
				LEN_AND_STR(gtmsource_options.log_file));
		rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	strcpy(jnlpool.gtmsource_local->statslog_file, gtmsource_options.log_file);
	jnlpool.gtmsource_local->statslog = TRUE;

	util_out_print("Stats log turned on with file !AD", TRUE, strlen(jnlpool.gtmsource_local->statslog_file),
			jnlpool.gtmsource_local->statslog_file);

	rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
	return (NORMAL_SHUTDOWN);
}
