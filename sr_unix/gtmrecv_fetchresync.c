/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
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
#include "repl_sp.h"
#include "repl_log.h"
#include "io.h"
#include "trans_log_name.h"
#include "util.h"

#define MAX_ATTEMPTS_FOR_FETCH_RESYNC	30
#define MAX_WAIT_FOR_FETCHRESYNC_CONN	30 /* s */
#define FETCHRESYNC_PRIMARY_POLL	(MICROSEC_IN_SEC - 1) /* micro seconds, almost 1 second */

GBLREF uint4			process_id;
GBLREF int			recvpool_shmid;
GBLREF int			gtmrecv_listen_sock_fd, gtmrecv_sock_fd;
GBLREF struct sockaddr_in	primary_addr;
GBLREF seq_num			seq_num_zero;
GBLREF jnl_gbls_t		jgbl;
GBLREF int			repl_max_send_buffsize, repl_max_recv_buffsize;

CONDITION_HANDLER(gtmrecv_fetchresync_ch)
{
	START_CH;

	if (gtmrecv_listen_sock_fd != -1)
		close(gtmrecv_listen_sock_fd);

	if (gtmrecv_sock_fd != -1)
		close(gtmrecv_sock_fd);

	PRN_ERROR;
	NEXTCH;
}

int gtmrecv_fetchresync(int port, seq_num *resync_seqno)
{
	size_t		primary_addr_len;
	repl_msg_t	msg;
	unsigned char	*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int		tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int		torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int		status;					/* needed for REPL_{SEND,RECV}_LOOP */
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


	ESTABLISH_RET(gtmrecv_fetchresync_ch, (!SS_NORMAL));

	QWASSIGN(*resync_seqno, seq_num_zero);

	gtmrecv_fetchresync_max_wait.tv_sec = MAX_WAIT_FOR_FETCHRESYNC_CONN;
	gtmrecv_fetchresync_max_wait.tv_usec = 0;

	gtmrecv_fetchresync_immediate.tv_sec = 0;
	gtmrecv_fetchresync_immediate.tv_usec = 0;

	gtmrecv_fetchresync_poll.tv_sec = 0;
	gtmrecv_fetchresync_poll.tv_usec = FETCHRESYNC_PRIMARY_POLL;

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
	repl_close(&gtmrecv_listen_sock_fd);
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize))
		|| 0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &repl_max_recv_buffsize)))
	{
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_LIT("Error getting socket send/recv buffsizes"),
				status);
		return ERR_REPLCOMM;
	}
	msg.type = REPL_FETCH_RESYNC;
	memset(&msg.msg[0], 0, MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
	QWASSIGN(*(seq_num *)&msg.msg[0], jgbl.max_resync_seqno);
	msg.len = MIN_REPL_MSGLEN;
	REPL_SEND_LOOP(gtmrecv_sock_fd, &msg, msg.len, FALSE, &gtmrecv_fetchresync_immediate)
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
	REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, FALSE, &gtmrecv_fetchresync_poll)
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
	/* Wait till connection is broken or REPL_CONN_CLOSE
	 * is received */
	REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, FALSE, &gtmrecv_fetchresync_poll)
	{
		REPL_DPRINT1("FETCH_RESYNC : Waiting for source to send CLOSE_CONN or connection breakage\n");
	}
	repl_close(&gtmrecv_sock_fd);
	REPL_DPRINT2("FETCH RESYNC : Waiting for pid %d rollback process to complete\n", rollback_pid);
	return SS_NORMAL;
}
