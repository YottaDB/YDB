/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_inet.h" /* Required for gtmsource.h */
#include <errno.h>
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
#include "repl_log.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;

int gtmsource_changelog(void)
{
	uint4	changelog_desired = 0, changelog_accepted = 0;

	if (0 > grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM))
	{
		util_out_print("Error grabbing jnlpool option write lock. Could not initiate change log", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}
	if (0 != jnlpool.gtmsource_local->changelog)
	{
		util_out_print("Change log is already in progress. Not initiating change in log file or log interval", TRUE);
		rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}
	if ('\0' != gtmsource_options.log_file[0]) /* trigger change in log file */
	{
		changelog_desired |= REPLIC_CHANGE_LOGFILE;
		if (0 != strcmp(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file))
		{
			changelog_accepted |= REPLIC_CHANGE_LOGFILE;
			strcpy(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
			util_out_print("Change log initiated with file !AD", TRUE, LEN_AND_STR(gtmsource_options.log_file));
		} else
			util_out_print("Log file is already !AD. Not initiating change in log file", TRUE,
					LEN_AND_STR(gtmsource_options.log_file));
	}
	if (0 != gtmsource_options.src_log_interval) /* trigger change in log interval */
	{
		changelog_desired |= REPLIC_CHANGE_LOGINTERVAL;
		if (gtmsource_options.src_log_interval != jnlpool.gtmsource_local->log_interval)
		{
			changelog_accepted |= REPLIC_CHANGE_LOGINTERVAL;
			jnlpool.gtmsource_local->log_interval = gtmsource_options.src_log_interval;
			util_out_print("Change log initiated with interval !UL", TRUE, gtmsource_options.src_log_interval);
		} else
			util_out_print("Log interval is already !UL. Not initiating change in log interval", TRUE,
					gtmsource_options.src_log_interval);
	}
	if (0 != changelog_accepted)
		jnlpool.gtmsource_local->changelog = changelog_accepted;
	else
		util_out_print("No change to log file or log interval", TRUE);
	rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
	return ((0 != changelog_accepted && changelog_accepted == changelog_desired) ? NORMAL_SHUTDOWN : ABNORMAL_SHUTDOWN);
}
