/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#ifdef UNIX
#include <sys/wait.h>
#include "fork_init.h"
#elif defined(VMS)
#include <descrip.h>
#endif

#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "repl_errno.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "io.h"
#include "is_proc_alive.h"
#include "gtmmsg.h"
#include "trans_log_name.h"
#include "repl_log.h"
#include "eintr_wrappers.h"
#include "gtmimagename.h"
#include "wbox_test_init.h"

#define UPDPROC_CMD_MAXLEN	GTM_PATH_MAX
#define UPDPROC_CMD		"%s/mupip"
#define UPDPROC_CMD_FILE	"mupip"
#define UPDPROC_CMD_ARG1	"replicate"
#define UPDPROC_CMD_ARG2	"-updateproc"
#define UPDPROC_CMD_STR		"REPLICATE/UPDATEPROC"

GBLREF recvpool_addrs	recvpool;
GBLREF int		recvpool_shmid;

GBLREF char		gtm_dist[GTM_PATH_MAX];
GBLREF boolean_t	gtm_dist_ok_to_use;
GBLREF int		gtmrecv_log_fd;
GBLREF FILE		*gtmrecv_log_fp;
GBLREF int		updproc_log_fd;
LITREF gtmImageName	gtmImageNames[];

error_def(ERR_GTMDISTUNVERIF);
error_def(ERR_LOGTOOLONG);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLINFO);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_UPDPROC);

int gtmrecv_upd_proc_init(boolean_t fresh_start)
{
	/* Update Process initialization */

	char	upd_proc_cmd[UPDPROC_CMD_MAXLEN];
	int	upd_proc_cmd_len;
	int	status, save_errno;
	int	upd_status, save_upd_status;
#ifdef UNIX
	pid_t	upd_pid, waitpid_res;
#elif defined(VMS)
	uint4	upd_pid;
	uint4	cmd_channel;
	$DESCRIPTOR(cmd_desc, UPDPROC_CMD_STR);
#endif
	pthread_mutexattr_t	write_updated_ctl_attr;

	/* Check if the update process is alive */
	if ((upd_status = is_updproc_alive()) == SRV_ERR)
	{
		save_errno = errno;	/* errno from get_sem_info() called from is_updproc_alive() */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			   	RTS_ERROR_LITERAL("Receive pool semctl failure"),
				UNIX_ONLY(save_errno) VMS_ONLY(REPL_SEM_ERRNO));
		repl_errno = EREPL_UPDSTART_SEMCTL;
		return(UPDPROC_START_ERR);
	} else if (upd_status == SRV_ALIVE && !fresh_start)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Update process already exists. Not starting it"));
		return(UPDPROC_EXISTS);
	} else if (upd_status == SRV_ALIVE)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDPROC, 0, ERR_TEXT, 2,
			   RTS_ERROR_LITERAL("Update process already exists. Please kill it before a fresh start"));
		return(UPDPROC_EXISTS);
	}

	save_upd_status = recvpool.upd_proc_local->upd_proc_shutdown;
	recvpool.upd_proc_local->upd_proc_shutdown = NO_SHUTDOWN;

#ifdef UNIX
	if (!gtm_dist_ok_to_use)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GTMDISTUNVERIF, 4, STRLEN(gtm_dist), gtm_dist,
				gtmImageNames[image_type].imageNameLen, gtmImageNames[image_type].imageName);
	if (WBTEST_ENABLED(WBTEST_MAXGTMDIST_UPDATE_PROCESS))
	{
		memset(gtm_dist, 'a', GTM_PATH_MAX-2);
		gtm_dist[GTM_PATH_MAX-1] = '\0';
	}
	upd_proc_cmd_len = SNPRINTF(upd_proc_cmd, UPDPROC_CMD_MAXLEN, UPDPROC_CMD, gtm_dist);
	if ((-1 == upd_proc_cmd_len) || (UPDPROC_CMD_MAXLEN <= upd_proc_cmd_len))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDPROC, 0, ERR_TEXT, 2,
			   RTS_ERROR_LITERAL("Could not find path of Update Process. Check value of $gtm_dist"));
		repl_errno = EREPL_UPDSTART_BADPATH;
		return(UPDPROC_START_ERR);
	}
	/* Destroy/Reinitialize the mutex.
	 * Needed here in case the update process exited while holding the mutex, and the system didn't clean it up.
	 * Robust mutexes should handle this case, in theory, but they are unreliable, at least on Ubuntu 12.04.
	 */
	pthread_mutex_destroy(&recvpool.recvpool_ctl->write_updated_ctl);
	status = pthread_mutexattr_init(&write_updated_ctl_attr);
	if (0 != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				LEN_AND_LIT("pthread_mutexattr_init"), CALLFROM, status, 0);
	status = pthread_mutexattr_setpshared(&write_updated_ctl_attr, PTHREAD_PROCESS_SHARED);
	if (0 != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				LEN_AND_LIT("pthread_mutexattr_setpshared"), CALLFROM, status, 0);
	status = pthread_mutex_init(&recvpool.recvpool_ctl->write_updated_ctl, &write_updated_ctl_attr);
	if (0 != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				LEN_AND_LIT("pthread_mutex_init"), CALLFROM, status, 0);
	FORK(upd_pid);
	if (0 > upd_pid)
	{
		recvpool.upd_proc_local->upd_proc_shutdown = save_upd_status;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_UPDPROC, 0, ERR_TEXT, 2,
			   RTS_ERROR_LITERAL("Could not fork update process"), errno);
		repl_errno = EREPL_UPDSTART_FORK;
		return(UPDPROC_START_ERR);
	}
	if (0 == upd_pid)
	{
		/* Update Process */
		if (WBTEST_ENABLED(WBTEST_BADEXEC_UPDATE_PROCESS))
		                STRCPY(upd_proc_cmd, "ersatz");
		if (EXECL(upd_proc_cmd, upd_proc_cmd, UPDPROC_CMD_ARG1, UPDPROC_CMD_ARG2, NULL) < 0)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_UPDPROC, 0, ERR_TEXT, 2,
				   RTS_ERROR_LITERAL("Could not exec Update Process"), errno);
			repl_errno = EREPL_UPDSTART_EXEC;
			UNDERSCORE_EXIT(UPDPROC_START_ERR);
		}
	}
#elif defined(VMS)
	/* Create detached server and write startup commands to it */
	status = repl_create_server(&cmd_desc, "GTMU", "", &cmd_channel, &upd_pid, ERR_RECVPOOLSETUP);
	if (SS_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to spawn Update process"), status);
		recvpool.upd_proc_local->upd_proc_shutdown = save_upd_status;
		repl_errno = EREPL_UPDSTART_FORK;
		return(UPDPROC_START_ERR);
	}
#endif
	if (recvpool.upd_proc_local->upd_proc_pid)
		recvpool.upd_proc_local->upd_proc_pid_prev = recvpool.upd_proc_local->upd_proc_pid;
	else
		recvpool.upd_proc_local->upd_proc_pid_prev = upd_pid;
	recvpool.upd_proc_local->upd_proc_pid = upd_pid;
	/* Receiver Server; wait for the update process to startup */
	REPL_DPRINT2("Waiting for update process %d to startup\n", upd_pid);
	while (get_sem_info(RECV, UPD_PROC_COUNT_SEM, SEM_INFO_VAL) == 0 && is_proc_alive(upd_pid, 0))
	{
		/* To take care of reassignment of PIDs, the while condition should be && with the
		 * condition (PPID of pid == process_id)
		 */
		REPL_DPRINT2("Waiting for update process %d to startup\n", upd_pid);
		UNIX_ONLY(WAITPID(upd_pid, &status, WNOHANG, waitpid_res);) /* Release defunct update process if dead */
		SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);
	}
#ifdef VMS
	/* Deassign the send-cmd mailbox channel */
	if (SS_NORMAL != (status = sys$dassgn(cmd_channel)))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to close upd-send-cmd mbox channel"), status);
		recvpool.upd_proc_local->upd_proc_shutdown = save_upd_status;
		repl_errno = EREPL_UPDSTART_BADPATH; /* Just to make an auto-shutdown */
		return(UPDPROC_START_ERR);
	}
#endif
	repl_log(gtmrecv_log_fp, TRUE, FALSE, "Update Process started. PID %d [0x%X]\n", upd_pid, upd_pid);
	return(UPDPROC_STARTED);
}

int gtmrecv_start_updonly(void)
{
	int start_status, recvr_status, upd_status;

	if ((upd_status = is_updproc_alive()) == SRV_ALIVE)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDPROC, 0, ERR_TEXT, 2,
			   RTS_ERROR_LITERAL("Update Process exists already. New process not started"));
		return(UPDPROC_START_ERR);
	} else if (upd_status == SRV_ERR)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDPROC, 0, ERR_TEXT, 2,
			   RTS_ERROR_LITERAL("Error in starting up update process"));
		return(UPDPROC_START_ERR);
	}

	assert(upd_status == SRV_DEAD);
#ifdef VMS
	recvpool.upd_proc_local->changelog |= REPLIC_CHANGE_LOGFILE;
#endif
	recvpool.upd_proc_local->start_upd = UPDPROC_START;
	while ((start_status = recvpool.upd_proc_local->start_upd) == UPDPROC_START &&
	       (recvr_status = is_recv_srv_alive()) == SRV_ALIVE)
		SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);

	if (start_status == UPDPROC_STARTED)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REPLINFO, 2,
				RTS_ERROR_LITERAL("Update Process started successfully"));
		return(UPDPROC_STARTED);
	}
#ifdef VMS
	recvpool.upd_proc_local->changelog &= ~REPLIC_CHANGE_LOGFILE;
#endif
	if (start_status == UPDPROC_START)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDPROC, 0, ERR_TEXT, 2,
			   RTS_ERROR_LITERAL("Receiver server is not alive to start update process. Please start receiver server"));
	} else if (start_status == UPDPROC_START_ERR)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDPROC, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error starting update process"));
	} else if (start_status == UPDPROC_EXISTS)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDPROC, 0, ERR_TEXT, 2,
			   RTS_ERROR_LITERAL("Update Process exists already. New process not started"));
	}
	return(UPDPROC_START_ERR);
}
