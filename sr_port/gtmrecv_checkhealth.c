/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include <errno.h>
#include "gtm_inet.h"
#include "gtm_string.h"
#ifdef UNIX
#include <sys/sem.h>
#endif
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
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "repl_log.h"
#include "is_proc_alive.h"

GBLREF	recvpool_addrs		recvpool;
GBLREF	gtmrecv_options_t	gtmrecv_options;

int is_srv_alive(int srv_type)
{
	int		status, semval;
	boolean_t	start_wait_logged;
	uint4		srv_pid;
	boolean_t	srv_alive;

	srv_pid = (GTMRECV == srv_type) ? recvpool.gtmrecv_local->recv_serv_pid : recvpool.upd_proc_local->upd_proc_pid;
	if (0 < srv_pid)
	{
		if (srv_alive = is_proc_alive(srv_pid, 0))
			semval = get_sem_info(RECV, (GTMRECV == srv_type) ? RECV_SERV_COUNT_SEM : UPD_PROC_COUNT_SEM, SEM_INFO_VAL);
		if (srv_alive && 1 == semval)
			status = SRV_ALIVE;
		else if (!srv_alive || 0 == semval)
			status = SRV_DEAD;
		else
			status = SRV_ERR;
	} else
		status = SRV_DEAD;
	return (status);
}

int is_updproc_alive(void)
{
	return (is_srv_alive(UPDPROC));
}

int is_recv_srv_alive(void)
{
	return (is_srv_alive(GTMRECV));
}

int gtmrecv_checkhealth(void)
{
	int			rcv_status, upd_status, helper_status, save_errno;
	uint4			gtmrecv_pid, updproc_pid, updproc_pid_prev, helper_pid;
	boolean_t		helper_alive;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	upd_helper_entry_ptr_t	helper, helper_top;
	recvpool_user		helper_type;

	/* Grab the recvpool option write lock */

	if (0 > grab_sem(RECV, RECV_SERV_OPTIONS_SEM))
	{
		UNIX_ONLY(save_errno = errno);
		repl_log(stderr, FALSE, TRUE, "Error grabbing recvpool option write lock : %s. Could not check health of Receiver"
			 "Server/Update Process\n", UNIX_ONLY(STRERROR(save_errno)) VMS_ONLY(REPL_SEM_ERROR));
		return (((SRV_ERR << 2) | SRV_ERR) + NORMAL_SHUTDOWN);
	}

	gtmrecv_pid = recvpool.gtmrecv_local->recv_serv_pid;
	updproc_pid = recvpool.upd_proc_local->upd_proc_pid;
	updproc_pid_prev = recvpool.upd_proc_local->upd_proc_pid_prev;

	REPL_DPRINT1("Checking health of Receiver Server/Update Process\n");

	rcv_status = is_recv_srv_alive();
	upd_status = is_updproc_alive();

	switch(rcv_status)
	{
		case SRV_ALIVE:
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, gtmrecv_pid, "Receiver server", "");
			break;

		case SRV_DEAD:
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, gtmrecv_pid, "Receiver server", " NOT");
			if (0 == gtmrecv_pid)
			{
				if (NO_SHUTDOWN == recvpool.gtmrecv_local->shutdown)
					repl_log(stderr, FALSE, TRUE,
							"Receiver Server crashed during receive pool initialization\n");
				else
					repl_log(stderr, FALSE, TRUE, "Receiver server crashed during shutdown\n");
			}
			break;

		case SRV_ERR:
			repl_log(stderr, FALSE, TRUE, "Error finding health of receiver server\n");
			break;
	}

	switch(upd_status)
	{
		case SRV_ALIVE:
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, updproc_pid, "Update process", "");
			break;

		case SRV_DEAD:
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, updproc_pid ? updproc_pid : updproc_pid_prev,
					"Update process", " NOT");
			if (0 == updproc_pid)
			{
				if (NO_SHUTDOWN == recvpool.upd_proc_local->upd_proc_shutdown)
					repl_log(stderr, FALSE, TRUE, "Update Process crashed during initialization\n");
				else
					repl_log(stderr, FALSE, TRUE, "Update Process crashed during shutdown\n");
			}
			break;

		case SRV_ERR:
			repl_log(stdout, FALSE, TRUE, "Error in finding health of update process\n");
			break;
	}

	helper_status = SRV_ALIVE;
	if (gtmrecv_options.helpers)
	{
		upd_helper_ctl = recvpool.upd_helper_ctl;
		for (helper = upd_helper_ctl->helper_list, helper_top = helper + MAX_UPD_HELPERS; helper < helper_top; helper++)
		{
			if (0 != (helper_pid = helper->helper_pid_prev))
			{
				helper_type = (recvpool_user)helper->helper_type;
				helper_alive = is_proc_alive(helper_pid, 0);
				if (helper_alive && 0 == helper->helper_pid) /* process has vacated its slot, but the rcvr hasn't */
					helper_alive = FALSE;          /* salvaged it yet. Unix zombies are alive,* er, half dead */
				helper_status = (SRV_ALIVE == helper_status && helper_alive) ? SRV_ALIVE : SRV_DEAD;
				repl_log(stderr, FALSE, TRUE, FORMAT_STR, helper_pid,
						(UPD_HELPER_READER == helper_type) ? "Helper reader" : "Helper writer",
					 	helper_alive ? "" : " NOT");
			}
		}
		if (SRV_DEAD == helper_status)
		{ /* indicate to the receiver that it has to reap helpers */
			upd_helper_ctl->reap_helpers = HELPER_REAP_NOWAIT;
			while (HELPER_REAP_NONE != upd_helper_ctl->reap_helpers && SRV_ALIVE == is_recv_srv_alive())
				SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_SHUTDOWN);
			upd_helper_ctl->reap_helpers = HELPER_REAP_NONE; /* just in case recvr died */
		}
	}
	rel_sem(RECV, RECV_SERV_OPTIONS_SEM);
	return ((rcv_status | (upd_status << 2) | (helper_status << 4)) + NORMAL_SHUTDOWN);
}
