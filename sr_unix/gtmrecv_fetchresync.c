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

#include "gtm_ipc.h"
#include "gtm_socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "gtm_time.h" /* needed for difftime() definition; if this file is not included, difftime returns bad values on AIX */
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/sem.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "error.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "repl_comm.h"
#include "repl_msg.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "gtm_logicals.h"
#include "gtm_stdio.h"
#include "eintr_wrappers.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "repl_log.h"
#include "io.h"
#include "trans_log_name.h"
#include "util.h"

#define MAX_ATTEMPTS_FOR_FETCH_RESYNC	30
#define MAX_WAIT_FOR_FETCHRESYNC_CONN	30 /* s */
#define FETCHRESYNC_PRIMARY_POLL	1 /* s */
#define GRAB_SEM_ERR_OUT 		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,\
					  ERR_TEXT, 2,\
					  RTS_ERROR_LITERAL("Error with receive pool semaphores. Receiver Server possibly exists"))

GBLREF uint4			process_id;
GBLREF int			recvpool_shmid;
GBLREF int			gtmrecv_listen_sock_fd, gtmrecv_sock_fd;
GBLREF struct sockaddr_in	primary_addr;
GBLREF seq_num			max_resync_seqno;
GBLREF seq_num			seq_num_zero;
GBLREF uint4			repl_max_send_buffsize, repl_max_recv_buffsize;

static int gtmrecv_fetchresync_detach(ctl_list *jnl_files);

CONDITION_HANDLER(gtmrecv_fetchresync_ch)
{
	START_CH;

	/* Remove semaphores created */

	remove_sem_set(RECV);

	if (gtmrecv_listen_sock_fd != -1)
		close(gtmrecv_listen_sock_fd);

	if (gtmrecv_sock_fd != -1)
		close(gtmrecv_sock_fd);

	PRN_ERROR;
	NEXTCH;
}

int gtmrecv_fetchresync(ctl_list *jnl_files, int port, seq_num *resync_seqno)
{
	mstr            log_nam, trans_log_nam;
	char            trans_buff[MAX_FN_LEN+1];
	key_t		recvpool_key;
	int		status;
	size_t		primary_addr_len;
	repl_msg_t	msg;
	unsigned char	*msg_ptr;
	int		sent_len, send_len;
	int		recv_len, recvd_len;
	fd_set		input_fds;
	int		wait_count;
	char		seq_num_str[32], *seq_num_ptr;
	pid_t		rollback_pid;
	int		rollback_status;
	int		wait_status;
	time_t		t1, t2;

	struct timeval	gtmrecv_fetchresync_max_wait,
			gtmrecv_fetchresync_immediate,
			gtmrecv_fetchresync_poll;

	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	recvpool_key = -1;
	recvpool_shmid = -1;

	ESTABLISH_RET(gtmrecv_fetchresync_ch, (!SS_NORMAL));

	QWASSIGN(*resync_seqno, seq_num_zero);

	/* Verify that a receiver server is not already running */
	log_nam.addr = ZGBLDIR;
	log_nam.len = sizeof(ZGBLDIR) - 1;

	if (trans_log_name(&log_nam, &trans_log_nam, trans_buff) != SS_NORMAL)
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("gtmgbldir not defined"));

	trans_buff[trans_log_nam.len] = '\0';

	gtmrecv_fetchresync_max_wait.tv_sec = MAX_WAIT_FOR_FETCHRESYNC_CONN;
	gtmrecv_fetchresync_max_wait.tv_usec = 0;

	gtmrecv_fetchresync_immediate.tv_sec = 0;
	gtmrecv_fetchresync_immediate.tv_usec = 0;

	gtmrecv_fetchresync_poll.tv_sec = FETCHRESYNC_PRIMARY_POLL;
	gtmrecv_fetchresync_poll.tv_usec = 0;

	if ((recvpool_key = FTOK(trans_buff, RECVPOOL_ID)) == -1)
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with receive pool ftok"), errno);

	if (0 > init_sem_set_recvr(recvpool_key, NUM_RECV_SEMS, RWDALL | IPC_CREAT | IPC_EXCL))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with receive pool semget. Receiver Server possibly exists"),
			REPL_SEM_ERRNO);
	/* Lock all access to receive pool */
	status = grab_sem_immediate(RECV, RECV_POOL_ACCESS_SEM);
	if (0 == status)
		status = grab_sem_immediate(RECV, RECV_SERV_COUNT_SEM);
	else
		GRAB_SEM_ERR_OUT;
	if (0 == status)
		status = grab_sem_immediate(RECV, UPD_PROC_COUNT_SEM);
	else
	{
		rel_sem(RECV, RECV_POOL_ACCESS_SEM);
		GRAB_SEM_ERR_OUT;
	}
	if (0 == status)
		status = grab_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
	else
	{
		rel_sem(RECV, RECV_POOL_ACCESS_SEM);
		rel_sem(RECV, RECV_SERV_COUNT_SEM);
		GRAB_SEM_ERR_OUT;
	}
	if (0 != status)
	{
		rel_sem(RECV, RECV_POOL_ACCESS_SEM);
		rel_sem(RECV, RECV_SERV_COUNT_SEM);
		rel_sem(RECV, UPD_PROC_COUNT_SEM);
		GRAB_SEM_ERR_OUT;
	}


	if ((status = shmget(recvpool_key, 0, RWDALL)) > 0)
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Receive pool exists. Receiver Server possibly exist"));

	gtmrecv_comm_init(port);

	primary_addr_len = sizeof(primary_addr);
	repl_log(stdout, TRUE, TRUE, "Waiting for a connection...\n");
	FD_ZERO(&input_fds);
	FD_SET(gtmrecv_listen_sock_fd, &input_fds);
	/*
	 * Note - the following call to select checks for EINTR. The SELECT macro is not used because
	 * the code also checks for EAGAIN and takes action before retrying the select.
	 */
	t1 = time(NULL);
	while ((status = select(gtmrecv_listen_sock_fd + 1, &input_fds, NULL, NULL, &gtmrecv_fetchresync_max_wait)) < 0)
	{
		if (errno == EINTR || errno == EAGAIN)
		{
			t2 = time(NULL);
			if (0 >= (int)(gtmrecv_fetchresync_max_wait.tv_sec =
					(MAX_WAIT_FOR_FETCHRESYNC_CONN - (int)difftime(t2, t1))))
			{
				status = 0;
				break;
			}
			gtmrecv_fetchresync_max_wait.tv_usec = 0;
			FD_SET(gtmrecv_listen_sock_fd, &input_fds);
			continue;
		} else
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error in select on listen socket"), errno);
	}
	if (status == 0)
	{
		repl_log(stdout, TRUE, TRUE, "Waited about %d seconds for connection from primary source server\n", 
				MAX_WAIT_FOR_FETCHRESYNC_CONN);
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Waited too long to get a connection request. Check if primary is alive."));
	}
	ACCEPT_SOCKET(gtmrecv_listen_sock_fd, (struct sockaddr *)&primary_addr, (sssize_t *)&primary_addr_len, gtmrecv_sock_fd);
	if (gtmrecv_sock_fd < 0)
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error accepting connection from Source Server"), errno);

	repl_log(stdout, TRUE, TRUE, "Connection established\n");

	close(gtmrecv_listen_sock_fd);

	if (0 > get_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize, &repl_max_recv_buffsize))
	{
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error getting socket send/recv buffsizes"), ERRNO);
		return (!SS_NORMAL);
	}

	msg.type = REPL_FETCH_RESYNC;
	memset(&msg.msg[0], 0, MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
	QWASSIGN(*(seq_num *)&msg.msg[0], max_resync_seqno);
	msg.len = MIN_REPL_MSGLEN;
	REPL_SEND_LOOP(gtmrecv_sock_fd, &msg, msg.len, &gtmrecv_fetchresync_immediate)
		; /* Empty Body */

	if (status != SS_NORMAL)
	{
		if (repl_errno == EREPL_SEND)
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error sending FETCH RESYNC message. Error in send"), status);
		if (repl_errno == EREPL_SELECT)
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error sending FETCH RESYNC message. Error in select"), status);
	}

	wait_count = MAX_ATTEMPTS_FOR_FETCH_RESYNC;
	REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, &gtmrecv_fetchresync_poll)
	{
		if (0 >= wait_count)
			break;
		repl_log(stdout, TRUE, TRUE, "Waiting for FETCH RESYNC\n");
		wait_count--;
	}

	if (status != SS_NORMAL)
	{
		if (repl_errno == EREPL_RECV)
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error receiving RESYNC JNLSEQNO. Error in recv"), status);
		if (repl_errno == EREPL_SELECT)
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error receiving RESYNC JNLSEQNO. Error in select"), status);
	}

	if (wait_count <= 0)
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Waited too long to get fetch resync message from primary. Check if primary is alive."));

	REVERT;

	QWASSIGN(*resync_seqno, *(seq_num *)&msg.msg[0]);

	repl_log(stdout, TRUE, TRUE, "Received RESYNC SEQNO is "INT8_FMT"\n", INT8_PRINT(*resync_seqno));
	/*
	 * Fork a child which will do the rest of the roll-back. The parent
	 * waits till the Source server signals completion of its task on
	 * receiving FETCH_RESYNC */

	if ((rollback_pid = fork()) > 0)
	{
		/* Parent */
		pid_t waitpid_res;

		gtmrecv_fetchresync_detach(jnl_files);

		/* Wait till connection is broken or REPL_CONN_CLOSE
		 * is received */
		REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, &gtmrecv_fetchresync_poll)
		{
			REPL_DPRINT1("FETCH_RESYNC : Waiting for source to send CLOSE_CONN or connection breakage\n");
		}
		close(gtmrecv_sock_fd);
		REPL_DPRINT2("FETCH RESYNC : Waiting for pid %d rollback process to complete\n", rollback_pid);
		WAITPID(rollback_pid, &rollback_status, 0, waitpid_res);
		if (waitpid_res == rollback_pid)
		{
			REPL_DPRINT2("FETCH RESYNC : Rollback child exited with status %d\n", rollback_status);
		} else if (waitpid_res < 0)
		{
			REPL_DPRINT2("FETCH RESYNC : wait_status from waitpid returned errno = %d\n", errno);
		}

		remove_sem_set(RECV);
		/*
		 * This is an exit from parent. We do not want to call mupip_exit_handler now.
		 */
#if defined(_AIX) && defined(_BSD)
		_exit(WEXITSTATUS(*(union wait *)&rollback_status)); /* will exit from here */
#else
		_exit(WEXITSTATUS(rollback_status)); /* will exit from here */
#endif
	} else if (rollback_pid == 0)
	{
		/* Child */
		process_id = getpid();
		return(SS_NORMAL);
	} else
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Could not fork rollback process"), errno);
}

static int gtmrecv_fetchresync_detach(ctl_list *jnl_files)
{
	/* Detach from all regions, and release recvr server count lock */

	ctl_list	*ctl_ptr;
	sgmnt_addrs	*csa;

	for (ctl_ptr = jnl_files; ctl_ptr != NULL; ctl_ptr = ctl_ptr->next)
	{
		if (ctl_ptr->next != NULL && ctl_ptr->gd == ctl_ptr->next->gd)
			continue;
		if (ctl_ptr->gd && ctl_ptr->gd->dyn.addr->fname_len)
		{
			csa = &FILE_INFO(ctl_ptr->gd)->s_addrs;
			if (csa->nl != NULL)
			{
				ctl_ptr->gd->dyn.addr->fname[ctl_ptr->gd->dyn.addr->fname_len] = '\0';
				REPL_DPRINT3("Detaching from %s at %lx\n", ctl_ptr->gd->dyn.addr->fname, (long)csa->nl);
				shmdt((caddr_t)csa->nl);
			}
		}
	}

	return(rel_sem_immediate(RECV, RECV_SERV_COUNT_SEM) == 0 ? SS_NORMAL : errno);
}

int gtmrecv_wait_for_detach(void)
{
	/* Wait till parent detaches from all regions and releases receiver server count lock.
	 * Release the semaphore to protect against hang if this function is re-entered.
	 * Releasing is ok since this semaphore has no use once the parent has detached from the regions
	 */

	if (0 == grab_sem(RECV, RECV_SERV_COUNT_SEM) && 0 == rel_sem(RECV, RECV_SERV_COUNT_SEM))
		return(SS_NORMAL);
	return(REPL_SEM_ERRNO);
}
