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

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_inet.h"

#include <errno.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmrecv.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"
#include "repl_log.h"

#include "gtm_fcntl.h"
#include "gtmio.h"
#include "repl_sp.h"

GBLREF	recvpool_addrs		recvpool;
GBLREF	gtmrecv_options_t	gtmrecv_options;
error_def(ERR_REPLLOGOPN);

int gtmrecv_changelog(void)
{
	uint4	changelog_desired = 0, changelog_accepted = 0;
	int     log_fd = 0; /*used to indicate whether the new specified log file is writable*/
	int     close_status = 0; /*used to indicate if log file is successfully closed*/
	char*   err_code;
	int     save_errno;

	/* Grab the recvpool jnlpool option write lock */
	if (0 > grab_sem(RECV, RECV_SERV_OPTIONS_SEM))
	{
		util_out_print("Error grabbing recvpool option write lock. Could not initiate change log", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}
	if (0 != recvpool.gtmrecv_local->changelog || 0 != recvpool.upd_proc_local->changelog)
	{
		util_out_print("Change log is already in progress. Not initiating change in log file or log interval", TRUE);
		rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}
	if ('\0' != gtmrecv_options.log_file[0]) /* trigger change in log file (for both receiver and update process) */
	{
		changelog_desired |= REPLIC_CHANGE_LOGFILE;
		if (0 != strcmp(recvpool.gtmrecv_local->log_file, gtmrecv_options.log_file))
		{
#ifdef UNIX
			/*check if the new log file is writable*/
			OPENFILE3(gtmrecv_options.log_file,
				      O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, log_fd);
			if (log_fd < 0) {
				save_errno = ERRNO;
				err_code = STRERROR(save_errno);
				gtm_putmsg(VARLSTCNT(8) ERR_REPLLOGOPN, 6,
						   LEN_AND_STR(gtmrecv_options.log_file),
						   LEN_AND_STR(err_code),
						   LEN_AND_STR(NULL_DEVICE));
			} else {
				CLOSEFILE_IF_OPEN(log_fd, close_status);
				assert(close_status==0);
				changelog_accepted |= REPLIC_CHANGE_LOGFILE;
				strcpy(recvpool.gtmrecv_local->log_file, gtmrecv_options.log_file);
				util_out_print("Change log initiated with file !AD", TRUE, LEN_AND_STR(gtmrecv_options.log_file));
			}
#elif defined(VMS)
				changelog_accepted |= REPLIC_CHANGE_LOGFILE;
				strcpy(recvpool.gtmrecv_local->log_file, gtmrecv_options.log_file);
				util_out_print("Change log initiated with file !AD", TRUE, LEN_AND_STR(gtmrecv_options.log_file));
#endif
		} else
			util_out_print("Log file is already !AD. Not initiating change in log file", TRUE,
					LEN_AND_STR(gtmrecv_options.log_file));
	}
	if (0 != gtmrecv_options.rcvr_log_interval) /* trigger change in receiver log interval */
	{
		changelog_desired |= REPLIC_CHANGE_LOGINTERVAL;
		if (gtmrecv_options.rcvr_log_interval != recvpool.gtmrecv_local->log_interval)
		{
			changelog_accepted |= REPLIC_CHANGE_LOGINTERVAL;
			recvpool.gtmrecv_local->log_interval = gtmrecv_options.rcvr_log_interval;
			util_out_print("Change initiated with receiver log interval !UL", TRUE, gtmrecv_options.rcvr_log_interval);
		} else
			util_out_print("Receiver log interval is already !UL. Not initiating change in log interval", TRUE,
					gtmrecv_options.rcvr_log_interval);
	}
	if (0 != gtmrecv_options.upd_log_interval) /* trigger change in update process log interval */
	{
		changelog_desired |= REPLIC_CHANGE_UPD_LOGINTERVAL;
		if (gtmrecv_options.upd_log_interval != recvpool.upd_proc_local->log_interval)
		{
			changelog_accepted |= REPLIC_CHANGE_UPD_LOGINTERVAL;
			recvpool.upd_proc_local->log_interval = gtmrecv_options.upd_log_interval;
			util_out_print("Change initiated with update process log interval !UL", TRUE,
					gtmrecv_options.upd_log_interval);
		} else
			util_out_print("Update process log interval is already !UL. Not initiating change in log interval", TRUE,
					gtmrecv_options.upd_log_interval);
	}
	if (0 != changelog_accepted)
		recvpool.gtmrecv_local->changelog = changelog_accepted;
	else
		util_out_print("No change to log file or log interval", TRUE);
	rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
	return ((0 != changelog_accepted && changelog_accepted == changelog_desired) ? NORMAL_SHUTDOWN : ABNORMAL_SHUTDOWN);
}
