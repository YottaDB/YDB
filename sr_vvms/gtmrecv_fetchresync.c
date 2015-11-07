/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_inet.h"
#include "gtm_time.h" /* needed for difftime() definition */
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
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
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "error.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "repl_comm.h"
#include "repl_msg.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "gtm_logicals.h"
#include "eintr_wrappers.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "repl_log.h"
#include "io.h"
#include "is_file_identical.h"
#include "trans_log_name.h"

#define MAX_ATTEMPTS_FOR_FETCH_RESYNC	60 /* max-wait in seconds for source server response after connection is established */
#define MAX_WAIT_FOR_FETCHRESYNC_CONN	60 /* max-wait in seconds to establish connection with the source server */
#define FETCHRESYNC_PRIMARY_POLL	(MICROSEC_IN_SEC - 1) /* micro seconds, almost 1 second */
#define GRAB_SEM_ERR_OUT 		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,\
					  ERR_TEXT, 2,\
					  LEN_AND_LIT("Error with receive pool semaphores. Receiver Server possibly exists"))

GBLREF	uint4			process_id;
GBLREF	int			gtmrecv_listen_sock_fd, gtmrecv_sock_fd;
GBLREF	struct addrinfo		primary_ai;
GBLREF	struct sockaddr_storage	primary_sas;
GBLREF	seq_num			seq_num_zero;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;

error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLCOMM);
error_def(ERR_TEXT);

CONDITION_HANDLER(gtmrecv_fetchresync_ch)
{
	START_CH;
	/* Remove semaphores created */
	remove_sem_set(RECV);
	repl_close(&gtmrecv_listen_sock_fd);
	repl_close(&gtmrecv_sock_fd);
	PRN_ERROR;
	NEXTCH;
}

int gtmrecv_fetchresync(int port, seq_num *resync_seqno)
{
	mstr            log_nam, trans_log_nam;
	char            trans_buff[MAX_FN_LEN+1];
	key_t		recvpool_key;
	repl_msg_t	msg;
	unsigned char	*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int		tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int		torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int		status;					/* needed for REPL_{SEND,RECV}_LOOP */
	fd_set		input_fds;
	int		wait_count;
	char		seq_num_str[32], *seq_num_ptr, err_string[1024];
	pid_t		rollback_pid;
	int		rollback_status;
	int		wait_status;
	gd_id		file_id;
	struct dsc$descriptor_s name_dsc;
	char			res_name[MAX_NAME_LEN + 2]; /* +1 for the terminator and another +1 for the length stored in [0]
								by global_name() */
	mstr			res_name_str;
	time_t			t1, t2;
	struct timeval	gtmrecv_fetchresync_max_wait;

	recvpool_key = -1;
	ESTABLISH(gtmrecv_fetchresync_ch);
	QWASSIGN(*resync_seqno, seq_num_zero);
	/* Verify that a receiver server is not already running */
	log_nam.addr = GTM_GBLDIR;
	log_nam.len = SIZEOF(GTM_GBLDIR) - 1;
	if (trans_log_name(&log_nam, &trans_log_nam, trans_buff) != SS_NORMAL)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				LEN_AND_LIT("gtmgbldir not defined"));
	trans_buff[trans_log_nam.len] = '\0';
	gtmrecv_fetchresync_max_wait.tv_sec = MAX_WAIT_FOR_FETCHRESYNC_CONN;
	gtmrecv_fetchresync_max_wait.tv_usec = 0;
	/* Get Recv. Pool Resource Name : name_dsc holds the resource name */
	set_gdid_from_file((gd_id_ptr_t)&file_id, trans_buff, trans_log_nam.len);
	global_name("GT$R", &file_id, res_name); /* R - Stands for Receiver Pool */
	name_dsc.dsc$a_pointer = &res_name[1];
        name_dsc.dsc$w_length = res_name[0];
        name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
        name_dsc.dsc$b_class = DSC$K_CLASS_S;
	name_dsc.dsc$a_pointer[name_dsc.dsc$w_length] = '\0';
	if (0 != init_sem_set_recvr(&name_dsc))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
				       ERR_TEXT, 2, LEN_AND_LIT("Error with receiver pool sem init."), REPL_SEM_ERRNO);
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
	/* Global section shouldn't already exist */
	if (shm_exists(RECV, &name_dsc))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RECVPOOLSETUP, 0,
			ERR_TEXT, 2, LEN_AND_LIT("Receive pool exists. Receiver Server possibly exists already!"));
	gtmrecv_comm_init(port);
	primary_ai.ai_addr = (sockaddr_ptr)&primary_sas;
	primary_ai.ai_addrlen = SIZEOF(primary_sas);
	repl_log(stdout, TRUE, TRUE, "Waiting for a connection...\n");
	FD_ZERO(&input_fds);
	FD_SET(gtmrecv_listen_sock_fd, &input_fds);
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
		{
			status = ERRNO;
			SNPRINTF(err_string, SIZEOF(err_string), "Error in select on listen socket : %s", STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
		}
	}
	if (status == 0)
	{
		repl_log(stdout, TRUE, TRUE, "Waited about %d seconds for connection from primary source server\n",
				MAX_WAIT_FOR_FETCHRESYNC_CONN);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Waited too long to get a connection request. Check if primary is alive."));
	}

	ACCEPT_SOCKET(gtmrecv_listen_sock_fd, primary_ai.ai_addr, (sssize_t *)&primary_ai.ai_addrlen, gtmrecv_sock_fd);
	if (gtmrecv_sock_fd < 0)
	{
		status = ERRNO;
		SNPRINTF(err_string, SIZEOF(err_string), "Error accepting connection from Source Server : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
	}
	repl_log(stdout, TRUE, TRUE, "Connection established\n");
	repl_close(&gtmrecv_listen_sock_fd);
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize))
		|| 0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &repl_max_recv_buffsize)))
	{
		SNPRINTF(err_string, SIZEOF(err_string), "Error getting socket send/recv bufsizes : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
		return ERR_REPLCOMM;
	}
	msg.type = REPL_FETCH_RESYNC;
	memset(&msg.msg[0], 0, MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
	QWASSIGN(*(seq_num *)&msg.msg[0], jgbl.max_resync_seqno);
	msg.len = MIN_REPL_MSGLEN;
	REPL_SEND_LOOP(gtmrecv_sock_fd, &msg, msg.len, REPL_POLL_NOWAIT);
		; /* Empty Body */
	if (status != SS_NORMAL)
	{
		if (repl_errno == EREPL_SEND)
		{
			SNPRINTF(err_string, SIZEOF(err_string), "Error sending FETCH RESYNC message. Error in send : %s",
				 STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
		}
		if (repl_errno == EREPL_SELECT)
		{
			SNPRINTF(err_string, SIZEOF(err_string), "Error sending FETCH RESYNC message. Error in select : %s",
				 STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
		}
	}
	wait_count = MAX_ATTEMPTS_FOR_FETCH_RESYNC;
	REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, REPL_POLL_WAIT)
	{
		if (0 >= wait_count)
			break;
		repl_log(stdout, TRUE, TRUE, "Waiting for FETCH RESYNC\n");
		wait_count--;
	}
	if (status != SS_NORMAL)
	{
		if (repl_errno == EREPL_RECV)
		{
			SNPRINTF(err_string, SIZEOF(err_string), "Error receiving RESYNC JNLSEQNO. Error in recv : %s",
				 STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
		}
		if (repl_errno == EREPL_SELECT)
		{
			SNPRINTF(err_string, SIZEOF(err_string), "Error receiving RESYNC JNLSEQNO. Error in select : %s",
				 STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
		}
	}
	if (wait_count <= 0)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			LEN_AND_LIT("Waited too long to get fetch resync message from primary. Check if primary is alive."));
	REVERT;
	QWASSIGN(*resync_seqno, *(seq_num *)&msg.msg[0]);
	repl_log(stdout, TRUE, TRUE, "Received RESYNC SEQNO is "INT8_FMT"\n", INT8_PRINT(*resync_seqno));
	/* Fork a child which will do the rest of the roll-back. The parent
	 * waits till the Source server signals completion of its task on
	 * receiving FETCH_RESYNC -- To be done on VMS. As of now it is sequential on VMS.
	 * The functionality doesn't get affected while the performance may
	 * suffer
	 */
	rel_sem_immediate(RECV, RECV_SERV_COUNT_SEM);
	/* Wait till connection is broken or REPL_CONN_CLOSE is received */
	REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, REPL_POLL_WAIT)
	{
		REPL_DPRINT1("FETCH_RESYNC : Waiting for source to send CLOSE_CONN or connection breakage\n");
	}
	repl_close(&gtmrecv_sock_fd);
	remove_sem_set(RECV);
	return(SS_NORMAL);
}

int gtmrecv_wait_for_detach(void)
{	/* Wait till parent detaches from all regions and releases receiver server count lock.
	 * Release the semaphore to protect against hang if this function is re-entered.
	 * Releasing is ok since this semaphore has no use once the parent has detached from the regions
	 */
	if (0 == grab_sem(RECV, RECV_SERV_COUNT_SEM) && 0 == rel_sem(RECV, RECV_SERV_COUNT_SEM))
		return(SS_NORMAL);
	return(REPL_SEM_ERRNO);
}
