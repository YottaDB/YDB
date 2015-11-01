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
#include "repl_shutdcode.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "repl_sem.h"
#include "util.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;

int gtmsource_changelog(void)
{
	if (0 > grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM))
	{
		util_out_print("Error grabbing jnlpool option write lock. Could not initiate change log", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}

	if (0 == strcmp(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file))
	{
		util_out_print("Log file is already !AD. Not initiating change in log file", TRUE,
				LEN_AND_STR(gtmsource_options.log_file));
		rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	if (jnlpool.gtmsource_local->changelog)
	{
		util_out_print("Change log is already in progress to !AD. Not initiating change in log file", TRUE,
				LEN_AND_STR(jnlpool.gtmsource_local->log_file));
		rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	strcpy(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
	jnlpool.gtmsource_local->changelog = TRUE;

	util_out_print("Change log initiated with file !AD", TRUE,
			LEN_AND_STR(jnlpool.gtmsource_local->log_file));

	rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
	return (NORMAL_SHUTDOWN);
}
