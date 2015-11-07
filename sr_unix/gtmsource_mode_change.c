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
#include "gtmio.h"
#include "repl_sp.h"
#include <errno.h>
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
#include "repl_instance.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
error_def(ERR_REPLLOGOPN);

int gtmsource_mode_change(int to_mode)
{
	uint4		savepid;
	int		exit_status;
	int		status, detach_status, remove_status;
	int 	        log_fd = 0, close_status = 0;
	char*           err_code;
	int	        save_errno;
	sgmnt_addrs	*repl_csa;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	repl_log(stdout, TRUE, TRUE, "Initiating %s operation on source server pid [%d] for secondary instance [%s]\n",
		(GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode) ? "ACTIVATE" : "DEACTIVATE",
		jnlpool.gtmsource_local->gtmsource_pid, jnlpool.gtmsource_local->secondary_instname);
	if ((jnlpool.gtmsource_local->mode == GTMSOURCE_MODE_ACTIVE_REQUESTED)
		|| (jnlpool.gtmsource_local->mode == GTMSOURCE_MODE_PASSIVE_REQUESTED))
	{
		repl_log(stderr, FALSE, TRUE, "Source Server %s already requested, not changing mode\n",
				(to_mode == GTMSOURCE_MODE_ACTIVE_REQUESTED) ? "ACTIVATE" : "DEACTIVATE");
		return (ABNORMAL_SHUTDOWN);
	}
	if (((GTMSOURCE_MODE_ACTIVE == jnlpool.gtmsource_local->mode) && (GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode))
		|| ((GTMSOURCE_MODE_PASSIVE == jnlpool.gtmsource_local->mode) && (GTMSOURCE_MODE_PASSIVE_REQUESTED == to_mode)))
	{
		repl_log(stderr, FALSE, TRUE, "Source Server already %s, not changing mode\n",
				(to_mode == GTMSOURCE_MODE_ACTIVE_REQUESTED) ? "ACTIVE" : "PASSIVE");
		return (ABNORMAL_SHUTDOWN);
	}
	assert(ROOTPRIMARY_UNSPECIFIED != gtmsource_options.rootprimary);
	/*check if the new log file is writable*/
	if ('\0' != gtmsource_options.log_file[0] && 0 != STRCMP(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file))
	{
		OPENFILE3(gtmsource_options.log_file,
			      O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, log_fd);
		if (log_fd < 0) {
			save_errno = ERRNO;
			err_code = STRERROR(save_errno);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_REPLLOGOPN, 6,
					   LEN_AND_STR(gtmsource_options.log_file),
					   LEN_AND_STR(err_code),
					   LEN_AND_STR(NULL_DEVICE));
			return (ABNORMAL_SHUTDOWN);
		}
		CLOSEFILE_IF_OPEN(log_fd, close_status);
		assert(close_status==0);
	}
	if ((GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode)
			&& (ROOTPRIMARY_SPECIFIED == gtmsource_options.rootprimary) && jnlpool.jnlpool_ctl->upd_disabled)
	{	/* ACTIVATE is specified with ROOTPRIMARY on a journal pool that was created with PROPAGATEPRIMARY. This is a
		 * case of transition from propagating primary to root primary. Enable updates in this journal pool and append
		 * a histinfo record to the replication instance file. The function "gtmsource_rootprimary_init" does just that.
		 */
		gtmsource_rootprimary_init(jnlpool.jnlpool_ctl->jnl_seqno);
	}
	DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
	assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
	/* Any ACTIVATE/DEACTIVATE versus ROOTPRIMARY/PROPAGATE incompatibilities have already been checked in the
	 * function "jnlpool_init" so go ahead and document the impending activation/deactivation and return.
	 * This flag will be eventually detected by the concurrently running source server which will then change mode.
	 */
	if (GTMSOURCE_MODE_ACTIVE_REQUESTED == to_mode)
	{
		jnlpool.gtmsource_local->secondary_port = gtmsource_options.secondary_port;
		STRCPY(jnlpool.gtmsource_local->secondary_host, gtmsource_options.secondary_host);
		jnlpool.gtmsource_local->secondary_port = gtmsource_options.secondary_port;
		memcpy(&jnlpool.gtmsource_local->connect_parms[0], &gtmsource_options.connect_parms[0],
				SIZEOF(gtmsource_options.connect_parms));
	}
	if ('\0' != gtmsource_options.log_file[0] && 0 != STRCMP(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file))
	{
		repl_log(stdout, FALSE, TRUE, "Signaling change in log file from %s to %s\n",
				jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
		STRCPY(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
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
	rel_lock(jnlpool.jnlpool_dummy_reg);
	REPL_DPRINT1("Change mode signalled\n");
	return (NORMAL_SHUTDOWN);
}
