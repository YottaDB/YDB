/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_string.h"
#include <errno.h>
#include <descrip.h> /* Required for gtmsource.h */
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
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_log.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		update_disable;

int gtmsource_mode_change(int to_mode)
{
	uint4		savepid;
	int		exit_status;
	int		status, detach_status, remove_status;

	/* Grab the jnlpool jnlpool option write lock */
	if (0 > grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM))
	{
		repl_log(stderr, FALSE, TRUE,
		  "Error grabbing jnlpool access control/jnlpool option write lock : %s. Could not change mode\n", REPL_SEM_ERROR);
		return (ABNORMAL_SHUTDOWN);
	}

	if ((jnlpool.gtmsource_local->mode == GTMSOURCE_MODE_ACTIVE_REQUESTED)
		|| (jnlpool.gtmsource_local->mode == GTMSOURCE_MODE_PASSIVE_REQUESTED))
	{
		repl_log(stderr, FALSE, TRUE, "Source Server %s already requested, not changing mode\n",
				(to_mode == GTMSOURCE_MODE_ACTIVE_REQUESTED) ? "ACTIVATE" : "DEACTIVATE");
		rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}
	if (((GTMSOURCE_MODE_ACTIVE == jnlpool.gtmsource_local->mode) && (GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode))
		|| ((GTMSOURCE_MODE_PASSIVE == jnlpool.gtmsource_local->mode) && (GTMSOURCE_MODE_PASSIVE_REQUESTED == to_mode)))
	{
		repl_log(stderr, FALSE, TRUE, "Source Server already %s, not changing mode\n",
				(to_mode == GTMSOURCE_MODE_ACTIVE_REQUESTED) ? "ACTIVE" : "PASSIVE");
		rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	repl_log(stdout, FALSE, FALSE, "Initiating change of mode from %s to %s\n", (GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode) ?
			"PASSIVE" : "ACTIVE", (GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode) ? "ACTIVE" : "PASSIVE");

	if (GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode)
	{
		jnlpool.gtmsource_local->secondary_port = gtmsource_options.secondary_port;
		strcpy(jnlpool.gtmsource_local->secondary_host, gtmsource_options.secondary_host);
		jnlpool.gtmsource_local->secondary_port = gtmsource_options.secondary_port;
		memcpy(&jnlpool.gtmsource_local->connect_parms[0], &gtmsource_options.connect_parms[0],
				SIZEOF(gtmsource_options.connect_parms));
	}
	if ('\0' != gtmsource_options.log_file[0] && 0 != strcmp(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file))
	{
		repl_log(stdout, FALSE, TRUE, "Signaling change in log file from %s to %s\n",
				jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
		strcpy(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
		jnlpool.gtmsource_local->changelog |= REPLIC_CHANGE_LOGFILE;
	}
	if (0 != gtmsource_options.src_log_interval && jnlpool.gtmsource_local->log_interval != gtmsource_options.src_log_interval)
	{
		repl_log(stdout, FALSE, TRUE, "Signaling change in log interval from %u to %u\n",
				jnlpool.gtmsource_local->log_interval, gtmsource_options.src_log_interval);
		jnlpool.gtmsource_local->log_interval = gtmsource_options.src_log_interval;
		jnlpool.gtmsource_local->changelog |= REPLIC_CHANGE_LOGINTERVAL;
	}

	jnlpool.gtmsource_local->mode = to_mode;
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
	if (update_disable)
	{
		jnlpool.jnlpool_ctl->upd_disabled = TRUE;
		repl_log(stdout, FALSE, TRUE, "Updates are disabled now \n");
	}
	else
	{
		jnlpool.jnlpool_ctl->upd_disabled = FALSE;
		repl_log(stdout, FALSE, TRUE, "Updates are allowed now \n");
	}
	rel_lock(jnlpool.jnlpool_dummy_reg);

	rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);

	REPL_DPRINT1("Change mode signalled\n");

	return (NORMAL_SHUTDOWN);
}
