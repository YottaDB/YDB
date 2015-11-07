/****************************************************************
 *								*
 *	Copyright 2008, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_time.h"
#include <sys/wait.h>
#include <errno.h>
#include "gtm_inet.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_shutdcode.h"

#include "gtmrecv.h"
#include "repl_comm.h"
#include "repl_msg.h"
#include "repl_dbg.h"
#include "repl_log.h"
#include "repl_errno.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "gt_timer.h"
#include "gtmio.h"

#include "util.h"
#include "tp_change_reg.h"
#include "memcoherency.h"
#include "replgbl.h"
#include "gtmsource.h"

GBLREF	repl_msg_ptr_t		gtmrecv_msgp;
GBLREF	int			gtmrecv_max_repl_msglen;
GBLREF	int			gtmrecv_listen_sock_fd;
GBLREF	int			gtmrecv_sock_fd;
GBLREF	boolean_t		repl_connection_reset;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			gtmrecv_log_fd;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	boolean_t		gtmrecv_logstats;
GBLREF	boolean_t		gtmrecv_wait_for_jnl_seqno;
GBLREF	boolean_t		gtmrecv_bad_trans_sent;
GBLREF	uint4			log_interval;
GBLREF	volatile time_t		gtmrecv_now;
GBLREF	boolean_t		gtmrecv_send_cmp2uncmp;
GBLREF	repl_conn_info_t	*remote_side;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	jnlpool_addrs		jnlpool;

error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLCOMM);
error_def(ERR_TEXT);

#ifdef INT8_SUPPORTED
static	seq_num			last_ack_seqno = 0;
#else
static	seq_num			last_ack_seqno = {0, 0};
#endif

#define GTMRECV_NEXT_REPORT_FACTOR	2

enum
{
	CONTINUE_POLL,
	STOP_POLL
};

int gtmrecv_poll_actions1(int *pending_data_len, int *buff_unprocessed, unsigned char *buffp)
{
	static int		report_cnt = 1;
	static int		next_report_at = 1;
	static boolean_t	send_xoff = FALSE;
	static boolean_t	xoff_sent = FALSE;
	static seq_num		send_seqno;
	static boolean_t	log_draining_msg = FALSE;
	static boolean_t	send_badtrans = FALSE;
	static boolean_t	send_cmp2uncmp = FALSE;
	static boolean_t	upd_shut_too_early_logged = FALSE;
	static time_t		last_reap_time = 0;
	repl_msg_t		xoff_msg;
	repl_badtrans_msg_t	bad_trans_msg;
	boolean_t		alert = FALSE, info = FALSE;
	int			return_status;
	gd_region		*region_top;
	unsigned char		*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int			status;					/* needed for REPL_{SEND,RECV}_LOOP */
	int			temp_len, pending_msg_size;
	int			upd_start_status, upd_start_attempts;
	int			buffered_data_len;
	int			upd_exit_status;
	seq_num			temp_send_seqno;
	boolean_t		bad_trans_detected = FALSE, onln_rlbk_flg_set = FALSE;
	uint4			jnl_status;
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	pid_t			waitpid_res;
	int4			msg_type, msg_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	upd_helper_ctl = recvpool.upd_helper_ctl;
	jnl_status = 0;
	if (SHUTDOWN == gtmrecv_local->shutdown)
	{
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Shutdown signalled\n");
		gtmrecv_end(); /* Won't return */
	}
	/* Reset report_cnt and next_report_at to 1 when a new upd proc is forked */
	if ((1 == report_cnt) || (report_cnt == next_report_at))
	{
		/* A comment on the usage of NO_SHUTDOWN below for the alert variable. Since upd_proc_local->upd_proc_shutdown is
		 * a shared memory field (and could be concurrently changed by either the receiver server or the update process),
		 * we want to make sure it is the same value BEFORE and AFTER checking whether the update process is alive or not.
		 * If it is not NO_SHUTDOWN (i.e. is SHUTDOWN or NORMAL_SHUTDOWN or ABNORMAL_SHUTDOWN) it has shut down due to
		 * an external request so we do want to send out a false update-process-is-not-alive alert.
		 */
		if ((alert = ((NO_SHUTDOWN == upd_proc_local->upd_proc_shutdown) && (SRV_DEAD == is_updproc_alive())
				&& (NO_SHUTDOWN == upd_proc_local->upd_proc_shutdown)))
			|| (info = (((NORMAL_SHUTDOWN == upd_proc_local->upd_proc_shutdown)
				|| (ABNORMAL_SHUTDOWN == upd_proc_local->upd_proc_shutdown)) && (SRV_DEAD == is_updproc_alive()))))
		{
			if (alert)
				repl_log(gtmrecv_log_fp, TRUE, TRUE,
					"ALERT : Receiver Server detected that Update Process is not ALIVE\n");
			else
				repl_log(gtmrecv_log_fp, TRUE, TRUE,
					"INFO : Update process not running due to user initiated shutdown\n");
			if (1 == report_cnt)
			{
				send_xoff = TRUE;
				QWASSIGN(recvpool_ctl->old_jnl_seqno, recvpool_ctl->jnl_seqno);
				QWASSIGNDW(recvpool_ctl->jnl_seqno, 0);
				/* Even though we have identified that the update process is NOT alive, a waitpid on the update
				 * process PID is necessary so that the system doesn't leave any zombie process lying around.
				 * This is possible since any child process that dies without the parent doing a waitpid on it
				 * will be defunct unless the parent dies at which point the "init" process takes the role of
				 * the parent and invokes waitpid to remove the zombies.
				 * NOTE: It is possible that the update process was killed before the receiver server got a
				 * chance to record it's PID in the recvpool.upd_proc_local structure. In such a case, don't
				 * invoke waitpid as that will block us (receiver server) if this instance of the receiver
				 * server was started with helper processes.
				 */
				if (0 < upd_proc_local->upd_proc_pid)
				{
					WAITPID(upd_proc_local->upd_proc_pid, &upd_exit_status, 0, waitpid_res);
					/* Since the update process as part of its shutdown does NOT reset the upd_proc_pid, reset
					 * it here ONLY if the update process was NOT kill -9ed. This is needed because receiver
					 * server as part of its shutdown relies on this field (upd_proc_pid) to determine if the
					 * update process was cleanly shutdown or was kill -9ed.
					 */
					if (!alert)
						upd_proc_local->upd_proc_pid = 0;
				}
				upd_proc_local->bad_trans = FALSE; /* No point in doing bad transaction processing */
				upd_proc_local->onln_rlbk_flg = FALSE; /* No point handling online rollback */
			}
			gtmrecv_wait_for_jnl_seqno = TRUE;
			REPL_DPRINT1(
			       "gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because of upd crash/shutdown\n");
			next_report_at *= GTMRECV_NEXT_REPORT_FACTOR;
			report_cnt++;
		}
	} else
		report_cnt++;
	/* Check if REPL_CMP2UNCMP or REPL_BADTRANS message needs to be sent */
	if (upd_proc_local->onln_rlbk_flg)
	{	/* Update process detected an online rollback and is requesting us to restart the connection. But before that, send
		 * REPL_XOFF source side and drain the replication pipe
		 */
		onln_rlbk_flg_set = TRUE;
		send_xoff = TRUE;
	}
	else if (!send_cmp2uncmp && gtmrecv_send_cmp2uncmp)
	{
		send_xoff = TRUE;
		send_seqno = recvpool_ctl->jnl_seqno;
		send_cmp2uncmp = TRUE;
	} else if (!send_badtrans && upd_proc_local->bad_trans)
	{
		send_xoff = TRUE;
		send_seqno = upd_proc_local->read_jnl_seqno;
		send_badtrans = TRUE;
		bad_trans_detected = TRUE;
	} else if (!upd_proc_local->bad_trans && send_badtrans && 1 != report_cnt)
	{
		send_badtrans = FALSE;
		bad_trans_detected = FALSE;
	}
	if (send_xoff && !xoff_sent)
	{	/* Send XOFF_ACK_ME if the receiver has a connection to the source. Do not attempt to send it if we dont even
		 * know the endianness of the remote side. In that case, we are guaranteed no initial handshake occurred and
		 * so no point sending the XOFF too. This saves us lots of trouble in case of cross-endian replication connections.
		 */
		assert((FD_INVALID  != gtmrecv_sock_fd) || repl_connection_reset);
		if ((FD_INVALID != gtmrecv_sock_fd) && remote_side->endianness_known)
		{
			send_seqno = upd_proc_local->read_jnl_seqno;
			if (!remote_side->cross_endian)
			{
				xoff_msg.type = REPL_XOFF_ACK_ME;
				xoff_msg.len = MIN_REPL_MSGLEN;
				memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&send_seqno, SIZEOF(seq_num));
			} else
			{
				xoff_msg.type = GTM_BYTESWAP_32(REPL_XOFF_ACK_ME);
				xoff_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
				temp_send_seqno = GTM_BYTESWAP_64(send_seqno);
				memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&temp_send_seqno, SIZEOF(seq_num));
			}
			REPL_SEND_LOOP(gtmrecv_sock_fd, &xoff_msg, MIN_REPL_MSGLEN, REPL_POLL_NOWAIT)
				; /* Empty Body */
			if (SS_NORMAL != status)
			{
				if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset while sending XOFF_ACK_ME. "
							"Status = %d ; %s\n", status, STRERROR(status));
					repl_close(&gtmrecv_sock_fd);
					repl_connection_reset = TRUE;
					xoff_sent = FALSE;
					send_badtrans = FALSE;

				} else if (EREPL_SEND == repl_errno)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error sending XOFF msg due to BAD_TRANS or UPD crash/shutdown. "
								"Error in send"), status);
				else
				{
					assert(EREPL_SELECT == repl_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error sending XOFF msg due to BAD_TRANS or UPD crash/shutdown. "
								"Error in select"), status);
				}
			} else
			{
				xoff_sent = TRUE;
				log_draining_msg = TRUE;
			}
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_XOFF_ACK_ME sent due to upd shutdown/crash or bad trans "
					"or ONLINE_ROLLBACK\n");
			send_xoff = FALSE;
		} else
		{	/* Connection has been lost OR initial handshake needs to happen again, so no point sending XOFF/BADTRANS */
			send_xoff = FALSE;
			send_badtrans = FALSE;
		}
	}
	/* Drain pipe */
	if (xoff_sent)
	{
		if (log_draining_msg)
		{	/* avoid multiple logs per instance */
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Draining replication pipe due to %s\n",
					send_cmp2uncmp ? "CMP2UNCMP" : (send_badtrans ? "BAD_TRANS" :
							(onln_rlbk_flg_set ? "ONLINE_ROLLBACK" : "UPD shutdown/crash")));
			log_draining_msg = FALSE;
		}
		if (0 != *buff_unprocessed)
		{	/* Throw away the current contents of the buffer */
			buffered_data_len = ((*pending_data_len <= *buff_unprocessed) ? *pending_data_len : *buff_unprocessed);
			*buff_unprocessed -= buffered_data_len;
			buffp += buffered_data_len;
			*pending_data_len -= buffered_data_len;
			REPL_DPRINT2("gtmrecv_poll_actions : (1) Throwing away %d bytes from old buffer while draining\n",
				buffered_data_len);
			assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
			while (REPL_MSG_HDRLEN <= *buff_unprocessed)
			{
				assert(0 == (((unsigned long)buffp) % REPL_MSG_ALIGN));
				msg_len = ((repl_msg_ptr_t)buffp)->len;
				msg_type = ((repl_msg_ptr_t)buffp)->type;
		        	if (remote_side->cross_endian)
			        {
			                msg_len = GTM_BYTESWAP_32(msg_len);
			                msg_type = GTM_BYTESWAP_32(msg_type);
			        }
				msg_type = (msg_type & REPL_TR_CMP_MSG_TYPE_MASK);
				assert((REPL_TR_CMP_JNL_RECS == msg_type) || (0 == (msg_len % REPL_MSG_ALIGN)));
				*pending_data_len = ROUND_UP2(msg_len, REPL_MSG_ALIGN);
				buffered_data_len = ((*pending_data_len <= *buff_unprocessed) ?
								*pending_data_len : *buff_unprocessed);
				*buff_unprocessed -= buffered_data_len;
				buffp += buffered_data_len;
				*pending_data_len -= buffered_data_len;
				REPL_DPRINT3("gtmrecv_poll_actions : (1) Throwing away message of "
					"type %d and length %d from old buffer while draining\n", msg_type, buffered_data_len);
			}
			if (0 < *buff_unprocessed)
			{
				memmove((unsigned char *)gtmrecv_msgp, buffp, *buff_unprocessed);
				REPL_DPRINT2("gtmrecv_poll_actions : Incomplete header of length %d while draining\n",
					*buff_unprocessed);
			}
		}
		status = SS_NORMAL;
		if (0 != *buff_unprocessed || 0 == *pending_data_len)
		{	/* Receive the header of a message */
			assert(REPL_MSG_HDRLEN > *buff_unprocessed);	/* so we dont pass negative length in REPL_RECV_LOOP */
			REPL_RECV_LOOP(gtmrecv_sock_fd, ((unsigned char *)gtmrecv_msgp) + *buff_unprocessed,
				       (REPL_MSG_HDRLEN - *buff_unprocessed), REPL_POLL_WAIT)
				; /* Empty Body */
			if (SS_NORMAL == status)
			{
				assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
		        	if (!remote_side->cross_endian)
	        		{
			                msg_len = gtmrecv_msgp->len;
			                msg_type = gtmrecv_msgp->type;
			        } else
			        {
			                msg_len = GTM_BYTESWAP_32(gtmrecv_msgp->len);
			                msg_type = GTM_BYTESWAP_32(gtmrecv_msgp->type);
			        }
				msg_type = (msg_type & REPL_TR_CMP_MSG_TYPE_MASK);
				assert((REPL_TR_CMP_JNL_RECS == msg_type) || (0 == (msg_len % REPL_MSG_ALIGN)));
				msg_len = ROUND_UP2(msg_len, REPL_MSG_ALIGN);
				REPL_DPRINT3("gtmrecv_poll_actions : Received message of type %d and length %d while draining\n",
					msg_type, msg_len);
			}
		}
		if ((SS_NORMAL == status) && (0 != *buff_unprocessed || 0 == *pending_data_len) && (REPL_XOFF_ACK == msg_type))
		{	/* Receive the rest of the XOFF_ACK msg and signal the drain as complete */
			REPL_RECV_LOOP(gtmrecv_sock_fd, gtmrecv_msgp, (MIN_REPL_MSGLEN - REPL_MSG_HDRLEN), REPL_POLL_WAIT)
				; /* Empty Body */
			if (SS_NORMAL == status)
			{
				repl_log(gtmrecv_log_fp, TRUE, TRUE,
						"REPL INFO - XOFF_ACK received. Drained replication pipe completely\n");
				upd_shut_too_early_logged = FALSE;
				xoff_sent = FALSE;
				return_status = STOP_POLL;
			}
		} else if (SS_NORMAL == status)
		{	/* Drain the rest of the message */
			if (0 < *pending_data_len)
			{
				pending_msg_size = *pending_data_len;
				REPL_DPRINT2("gtmrecv_poll_actions : (2) Throwing away %d bytes from pipe\n", pending_msg_size);
			} else
			{
				pending_msg_size = msg_len - REPL_MSG_HDRLEN;
				REPL_DPRINT3("gtmrecv_poll_actions : (2) Throwing away message of "
					"type %d and length %d from pipe\n", msg_type, msg_len);
			}
			for ( ; SS_NORMAL == status && 0 < pending_msg_size; pending_msg_size -= gtmrecv_max_repl_msglen)
			{
				temp_len = (pending_msg_size < gtmrecv_max_repl_msglen)? pending_msg_size : gtmrecv_max_repl_msglen;
				REPL_RECV_LOOP(gtmrecv_sock_fd, gtmrecv_msgp, temp_len, REPL_POLL_WAIT)
					; /* Empty Body */
			}
			*buff_unprocessed = 0; *pending_data_len = 0;
			if (SS_NORMAL == status && info && !upd_shut_too_early_logged)
			{
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "ALERT : User initiated shutdown of Update Process done "
						"when there was data in the replication pipe\n");
				upd_shut_too_early_logged = TRUE;
			}
			return_status = CONTINUE_POLL;
		}
		if (SS_NORMAL != status)
		{
			if (EREPL_RECV == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset while receiving XOFF_ACK. "
							"Status = %d ; %s\n", status, STRERROR(status));
					repl_close(&gtmrecv_sock_fd);
					repl_connection_reset = TRUE;
					xoff_sent = FALSE;
					send_badtrans = FALSE;
					return_status = STOP_POLL;
				} else
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error while draining replication pipe. Error in recv"), status);
			} else
			{
				assert(EREPL_SELECT == repl_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error while draining replication pipe. Error in select"), status);
			}
		}
	} else
		return_status = STOP_POLL;
	/* Like was done before for the XOFF_ACK_ME message, send a BADTRANS/CMP2UNCMP message only if we know
	 * the endianness of the other side. If not, no point in sending one anyways and saves us trouble in
	 * case of cross-endian replication connections.
	 */
	if ((STOP_POLL == return_status) && (send_badtrans || send_cmp2uncmp)
		&& (FD_INVALID != gtmrecv_sock_fd) && remote_side->endianness_known)
	{	/* Send REPL_BADTRANS or REPL_CMP2UNCMP message */
		if (!remote_side->cross_endian)
		{
			bad_trans_msg.type = send_cmp2uncmp ? REPL_CMP2UNCMP : REPL_BADTRANS;
			bad_trans_msg.len  = MIN_REPL_MSGLEN;
			bad_trans_msg.start_seqno = send_seqno;
		} else
		{
			bad_trans_msg.type = send_cmp2uncmp ? GTM_BYTESWAP_32(REPL_CMP2UNCMP) : GTM_BYTESWAP_32(REPL_BADTRANS);
			bad_trans_msg.len  = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
			bad_trans_msg.start_seqno = GTM_BYTESWAP_64(send_seqno);
		}
		REPL_SEND_LOOP(gtmrecv_sock_fd, &bad_trans_msg, bad_trans_msg.len, REPL_POLL_NOWAIT)
			; /* Empty Body */
		if (SS_NORMAL == status)
		{
			if (send_cmp2uncmp)
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_CMP2UNCMP message sent with seqno %llu\n", send_seqno);
			else
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_BADTRANS message sent with seqno %llu\n", send_seqno);
		} else
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				if (send_cmp2uncmp)
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset while sending REPL_CMP2UNCMP. "
							"Status = %d ; %s\n", status, STRERROR(status));
				} else
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset while sending REPL_BADTRANS. "
							"Status = %d ; %s\n", status, STRERROR(status));
				}
				repl_close(&gtmrecv_sock_fd);
				repl_connection_reset = TRUE;
				return_status = STOP_POLL;
			} else if (EREPL_SEND == repl_errno)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error sending REPL_BADTRANS/REPL_CMP2UNCMP. Error in send"), status);
			else
			{
				assert(EREPL_SELECT == repl_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error sending REPL_BADTRANS/REPL_CMP2UNCMP. Error in select"), status);
			}
		}
		send_badtrans = FALSE;
		if (send_cmp2uncmp)
		{
			REPL_DPRINT1("gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because this receiver"
				"server requested a fall-back from compressed to uncompressed operation\n");
			gtmrecv_wait_for_jnl_seqno = TRUE;/* set this to TRUE to break out and go back to a fresh "do_main_loop" */
			gtmrecv_bad_trans_sent = TRUE;
			gtmrecv_send_cmp2uncmp = FALSE;
			send_cmp2uncmp = FALSE;
		}
	}
	if ((upd_proc_local->bad_trans && bad_trans_detected) || onln_rlbk_flg_set
		|| (UPDPROC_START == upd_proc_local->start_upd) && (1 != report_cnt))
	{
		if (UPDPROC_START == upd_proc_local->start_upd)
		{
			assert(is_updproc_alive() != SRV_ALIVE);
			upd_proc_local->upd_proc_shutdown = NO_SHUTDOWN;
		}
		recvpool_ctl->wrapped = FALSE;
		recvpool_ctl->write_wrap = recvpool_ctl->recvpool_size;
		recvpool_ctl->write = 0;
		/* Reset last_rcvd_histinfo, last_valid_histinfo etc. as they reflect context from unprocessed data
		 * in the receive pool and those are no longer valid because we have drained the receive pool.
		 */
		GTMRECV_CLEAR_CACHED_HISTINFO(recvpool.recvpool_ctl, jnlpool, jnlpool_ctl, INSERT_STRM_HISTINFO_FALSE);
		if (UPDPROC_START == upd_proc_local->start_upd)
		{
			/* Attempt starting the update process */
			for (upd_start_attempts = 0;
			     UPDPROC_START_ERR == (upd_start_status = gtmrecv_upd_proc_init(FALSE)) &&
			     GTMRECV_MAX_UPDSTART_ATTEMPTS > upd_start_attempts;
			     upd_start_attempts++)
			{
				if (EREPL_UPDSTART_SEMCTL == repl_errno || EREPL_UPDSTART_BADPATH == repl_errno)
				{
					gtmrecv_autoshutdown();
				} else if (EREPL_UPDSTART_FORK == repl_errno)
				{
					/* Couldn't start up update now, can try later */
					LONG_SLEEP(GTMRECV_WAIT_FOR_PROC_SLOTS);
					continue;
				} else if (EREPL_UPDSTART_EXEC == repl_errno)
				{
					/* In forked child, could not exec, should exit */
					gtmrecv_exit(ABNORMAL_SHUTDOWN);
				}
			}
			if (UPDPROC_STARTED == (upd_proc_local->start_upd = upd_start_status))
			{
				REPL_DPRINT1("gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because of "
					     "upd restart\n");
				gtmrecv_wait_for_jnl_seqno = TRUE;
				report_cnt = next_report_at = 1;
				if (send_xoff && (FD_INVALID == gtmrecv_sock_fd))
				{
					/* Update start command was issued before connection was established,
					 * no point in sending XOFF.  */
					send_xoff = FALSE;
				}
			} else
			{
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "%d failed attempts to fork update process. Try later\n",
					 upd_start_attempts);
			}
		} else
		{
			gtmrecv_wait_for_jnl_seqno = TRUE;/* set this to TRUE to break out and go back to a fresh "do_main_loop" */
			if (onln_rlbk_flg_set)
			{
				assert(NULL != jnlpool_ctl);
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "Closing connection due to ONLINE ROLLBACK\n");
 				repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Current Jnlpool Seqno : %llu\n",
 						jnlpool_ctl->jnl_seqno);
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Current Receive Pool Seqno : %llu\n",
						recvpool_ctl->jnl_seqno);
				repl_close(&gtmrecv_sock_fd);
				repl_connection_reset = TRUE;
				xoff_sent = FALSE;
				send_badtrans = FALSE;
				upd_proc_local->onln_rlbk_flg = FALSE;
				/* Before restarting afresh, sync the online rollback cycles. This way any future grab_lock that
				 * we do after restarting should not realize an unhandled online rollback.  For receiver, it is
				 * just syncing the journal pool cycles as the databases are not opened. But, to be safe, grab
				 * the lock and sync the cycles.
				 */
				grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
				SYNC_ONLN_RLBK_CYCLES;
				rel_lock(jnlpool.jnlpool_dummy_reg);
				return_status = STOP_POLL;
				recvpool_ctl->jnl_seqno = 0;
			} else
			{
				REPL_DPRINT1("gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because bad trans"
						"sent\n");
				gtmrecv_bad_trans_sent = TRUE;
				upd_proc_local->bad_trans = FALSE;
				recvpool_ctl->jnl_seqno = upd_proc_local->read_jnl_seqno;
			}
		}
	}
	if ((0 == *pending_data_len) && (0 != gtmrecv_local->changelog))
	{
		if (gtmrecv_local->changelog & REPLIC_CHANGE_LOGINTERVAL)
		{
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Changing log interval from %u to %u\n",
					log_interval, gtmrecv_local->log_interval);
			log_interval = gtmrecv_local->log_interval;
			gtmrecv_reinit_logseqno(); /* will force a LOG on the first recv following the interval change */
		}
		if (gtmrecv_local->changelog & REPLIC_CHANGE_LOGFILE)
		{
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Changing log file to %s\n", gtmrecv_local->log_file);
			repl_log_init(REPL_GENERAL_LOG, &gtmrecv_log_fd, gtmrecv_local->log_file);
			repl_log_fd2fp(&gtmrecv_log_fp, gtmrecv_log_fd);
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Change log to %s successful\n",gtmrecv_local->log_file);
		}
		upd_proc_local->changelog = gtmrecv_local->changelog; /* Pass changelog request to the update process */
		/* NOTE: update process and receiver each ignore any setting specific to the other (REPLIC_CHANGE_UPD_LOGINTERVAL,
		 * REPLIC_CHANGE_LOGINTERVAL) */
		gtmrecv_local->changelog = 0;
	}
	if (0 == *pending_data_len && !gtmrecv_logstats && gtmrecv_local->statslog)
	{
		gtmrecv_logstats = TRUE;
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Begin statistics logging\n");
	} else if (0 == *pending_data_len && gtmrecv_logstats && !gtmrecv_local->statslog)
	{
		gtmrecv_logstats = FALSE;
		/* Force all data out to the file before closing the file */
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "End statistics logging\n");
	}
	if (0 == *pending_data_len)
	{
		if (upd_helper_ctl->start_helpers)
		{
			gtmrecv_helpers_init(upd_helper_ctl->start_n_readers, upd_helper_ctl->start_n_writers);
			upd_helper_ctl->start_helpers = FALSE;
		}
		if (HELPER_REAP_NONE != (status = upd_helper_ctl->reap_helpers) ||
			(double)GTMRECV_REAP_HELPERS_INTERVAL <= difftime(gtmrecv_now, last_reap_time))
		{
			gtmrecv_reap_helpers(HELPER_REAP_WAIT == status);
			last_reap_time = gtmrecv_now;
		}
	}
	return (return_status);
}

int gtmrecv_poll_actions(int pending_data_len, int buff_unprocessed, unsigned char *buffp)
{
	while (CONTINUE_POLL == gtmrecv_poll_actions1(&pending_data_len, &buff_unprocessed, buffp))
		;
	return (SS_NORMAL);
}
