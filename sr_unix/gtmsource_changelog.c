/****************************************************************
 *								*
 *	Copyright 2006, 2012 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_fcntl.h"
#include "gtmio.h"
#include "repl_sp.h"
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
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
error_def(ERR_REPLLOGOPN);
int gtmsource_changelog(void)
{
	uint4	changelog_desired = 0, changelog_accepted = 0;
	int     log_fd = 0; /*used to indicate whether the new specified log file is writable*/
	int     close_status = 0; /*used to indicate if log file is successfully closed*/
	char*   err_code;
	int	save_errno;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	repl_log(stderr, TRUE, TRUE, "Initiating CHANGELOG operation on source server pid [%d] for secondary instance [%s]\n",
		jnlpool.gtmsource_local->gtmsource_pid, jnlpool.gtmsource_local->secondary_instname);
	if (0 != jnlpool.gtmsource_local->changelog)
	{
		util_out_print("Change log is already in progress. Not initiating change in log file or log interval", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}
	if ('\0' != gtmsource_options.log_file[0]) /* trigger change in log file */
	{
		changelog_desired |= REPLIC_CHANGE_LOGFILE;
		if (0 != STRCMP(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file))
		{	/*check if the new log file is writable*/
			OPENFILE3(gtmsource_options.log_file,
				      O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, log_fd);
			if (log_fd < 0) {
				save_errno = ERRNO;
				err_code = STRERROR(save_errno);
				gtm_putmsg(VARLSTCNT(8) ERR_REPLLOGOPN, 6,
						   LEN_AND_STR(gtmsource_options.log_file),
						   LEN_AND_STR(err_code),
						   LEN_AND_STR(NULL_DEVICE));
			} else {
				CLOSEFILE_IF_OPEN(log_fd, close_status);
				assert(close_status==0);
				changelog_accepted |= REPLIC_CHANGE_LOGFILE;
				STRCPY(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
				util_out_print("Change log initiated with file !AD", TRUE, LEN_AND_STR(gtmsource_options.log_file));
			}
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
	return ((0 != changelog_accepted && changelog_accepted == changelog_desired) ? NORMAL_SHUTDOWN : ABNORMAL_SHUTDOWN);
}
