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
#include <descrip.h> /* Required for gtmrecv.h */

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

GBLREF	repl_msg_ptr_t		gtmrecv_msgp;
GBLREF	int			gtmrecv_max_repl_msglen;
GBLREF	int			gtmrecv_listen_sock_fd;
GBLREF	int			gtmrecv_sock_fd;
GBLREF	boolean_t		repl_connection_reset;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			gtmrecv_log_fd;
GBLREF 	FILE			*gtmrecv_log_fp;
GBLREF	boolean_t		gtmrecv_logstats;
GBLREF	boolean_t		gtmrecv_wait_for_jnl_seqno;
GBLREF	boolean_t		gtmrecv_bad_trans_sent;
GBLREF	pid_t			updproc_pid;
GBLREF	uint4			log_interval;
GBLREF	volatile time_t		gtmrecv_now;

error_def(ERR_REPLCOMM);
error_def(ERR_RECVPOOLSETUP);
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
	static boolean_t	log_draining_msg = FALSE;
	static boolean_t	send_badtrans = FALSE;
	static boolean_t	upd_shut_too_early_logged = FALSE;
	static repl_msg_t	xoff_msg, bad_trans_msg;
 	static time_t		last_reap_time = 0;

	boolean_t		alert = FALSE, info = FALSE;
	int			return_status;
	gd_region       	*region_top;
	unsigned char		*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int			status;					/* needed for REPL_{SEND,RECV}_LOOP */
	int			temp_len, pending_msg_size;
	int			upd_start_status, upd_start_attempts;
	int			buffered_data_len;
	int			upd_exit_status;
	boolean_t		bad_trans_detected = FALSE;
	uint4			jnl_status;
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
 	upd_helper_ctl_ptr_t	upd_helper_ctl;

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
	if (1 == report_cnt || report_cnt == next_report_at)
	{
		if ((alert =
		     (NO_SHUTDOWN == upd_proc_local->upd_proc_shutdown
		      && SRV_DEAD == is_updproc_alive() &&
		      NO_SHUTDOWN == upd_proc_local->upd_proc_shutdown)) ||
		    (info = ((NORMAL_SHUTDOWN == upd_proc_local->upd_proc_shutdown ||
			     ABNORMAL_SHUTDOWN == upd_proc_local->upd_proc_shutdown)) &&
		     	     SRV_DEAD == is_updproc_alive()))
		{
			if (alert)
				repl_log(gtmrecv_log_fp, TRUE, TRUE,
					"ALERT : Receiver Server detected that Update Process is not ALIVE\n");
			else
				repl_log(gtmrecv_log_fp, TRUE, TRUE,
					"INFO : Update process not running. User initiated Update Process shutdown was done\n");
			if (1 == report_cnt)
			{
				send_xoff = TRUE;
				QWASSIGN(recvpool_ctl->old_jnl_seqno, recvpool_ctl->jnl_seqno);
				QWASSIGNDW(recvpool_ctl->jnl_seqno, 0);
				upd_proc_local->bad_trans = FALSE; /* No point in doing bad transaction processing */
			}
			gtmrecv_wait_for_jnl_seqno = TRUE;
			REPL_DPRINT1(
			       "gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because of upd crash/shutdown\n");
			next_report_at *= GTMRECV_NEXT_REPORT_FACTOR;
			report_cnt++;
		}
	} else
		report_cnt++;

	if (upd_proc_local->bad_trans && !send_badtrans)
	{
		send_xoff = TRUE;
		send_badtrans = TRUE;
		bad_trans_detected = TRUE;
	} else if (!upd_proc_local->bad_trans && send_badtrans && 1 != report_cnt)
	{
		send_badtrans = FALSE;
		bad_trans_detected = FALSE;
	}
	if (send_xoff && !xoff_sent && (FD_INVALID != gtmrecv_sock_fd))
	{
		/* Send XOFF */
		xoff_msg.type = REPL_XOFF_ACK_ME;
		memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, SIZEOF(seq_num));
		xoff_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xoff_msg, xoff_msg.len, REPL_POLL_NOWAIT)
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
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error sending XOFF msg due to BAD_TRANS or UPD crash/shutdown. "
							"Error in send"), status);
			else if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error sending XOFF msg due to BAD_TRANS or UPD crash/shutdown. "
							"Error in select"), status);
		} else
		{
			xoff_sent = TRUE;
			log_draining_msg = TRUE;
		}
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_XOFF_ACK_ME sent due to upd shutdown/crash or bad trans\n");
		send_xoff = FALSE;
	} else if (send_xoff && !xoff_sent && repl_connection_reset)
	{
		send_xoff = FALSE; /* connection has been lost, no point sending XOFF */
		send_badtrans = FALSE;
	}
	/* Drain pipe */
	if (xoff_sent)
	{
		if (log_draining_msg)
		{ /* avoid multiple logs per instance */
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Draining replication pipe due to %s\n",
					send_badtrans ? "BAD_TRANS" : "UPD shutdown/crash");
			log_draining_msg = FALSE;
		}
		if (0 != *buff_unprocessed)
		{
			/* Throw away the current contents of the buffer */
			buffered_data_len = ((*pending_data_len <= *buff_unprocessed) ? *pending_data_len : *buff_unprocessed);
			*buff_unprocessed -= buffered_data_len;
			buffp += buffered_data_len;
			*pending_data_len -= buffered_data_len;
			REPL_DPRINT2("gtmrecv_poll_actions : (1) Throwing away %d bytes from old buffer while draining\n",
					buffered_data_len);
			while (REPL_MSG_HDRLEN <= *buff_unprocessed)
			{
				assert(0 == ((unsigned long)buffp & (SIZEOF(((repl_msg_ptr_t)buffp)->type) - 1)));
				*pending_data_len = ((repl_msg_ptr_t)buffp)->len;
				buffered_data_len = ((*pending_data_len <= *buff_unprocessed) ?
								*pending_data_len : *buff_unprocessed);
				*buff_unprocessed -= buffered_data_len;
				buffp += buffered_data_len;
				*pending_data_len -= buffered_data_len;
				REPL_DPRINT2("gtmrecv_poll_actions : (2) Throwing away %d bytes from old buffer while draining\n",
						buffered_data_len);
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
		{
			/* Receive the header of a message */
			REPL_RECV_LOOP(gtmrecv_sock_fd, ((unsigned char *)gtmrecv_msgp) + *buff_unprocessed,
				       (REPL_MSG_HDRLEN - *buff_unprocessed), REPL_POLL_WAIT)
				; /* Empty Body */

			REPL_DPRINT3("gtmrecv_poll_actions : Received %d type of message of length %d while draining\n",
					((repl_msg_ptr_t)gtmrecv_msgp)->type, ((repl_msg_ptr_t)gtmrecv_msgp)->len);
		}
		if (SS_NORMAL == status &&
				(0 != *buff_unprocessed || 0 == *pending_data_len) && REPL_XOFF_ACK == gtmrecv_msgp->type)
		{
			/* The rest of the XOFF_ACK msg */
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
		{
			/* Drain the rest of the message */
			pending_msg_size = ((*pending_data_len > 0) ? *pending_data_len : gtmrecv_msgp->len - REPL_MSG_HDRLEN);
			REPL_DPRINT2("gtmrecv_poll_actions : Throwing away %d bytes from pipe\n", pending_msg_size);
			for (; SS_NORMAL == status && 0 < pending_msg_size;
			     pending_msg_size -= gtmrecv_max_repl_msglen)
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
					rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error while draining replication pipe. Error in recv"), status);
			} else if (EREPL_SELECT == repl_errno)
			{
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error while draining replication pipe. Error in select"), status);
			}
		}
	} else
		return_status = STOP_POLL;

	if (STOP_POLL == return_status && send_badtrans && (FD_INVALID != gtmrecv_sock_fd))
	{
		/* Send BAD_TRANS */
		bad_trans_msg.type = REPL_BADTRANS;
		memcpy((uchar_ptr_t)&bad_trans_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, SIZEOF(seq_num));
		bad_trans_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &bad_trans_msg, bad_trans_msg.len, REPL_POLL_NOWAIT)
			; /* Empty Body */
		if (SS_NORMAL == status)
		{
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
					"REPL_BADTRANS sent with seqno %llu\n", upd_proc_local->read_jnl_seqno);
		} else
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset while sending REPL_BADTRANS. "
						"Status = %d ; %s\n", status, STRERROR(status));
				repl_close(&gtmrecv_sock_fd);
				repl_connection_reset = TRUE;
				return_status = STOP_POLL;
			} else if (EREPL_SEND == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error sending BAD_TRANS. Error in send"), status);
			else if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_LIT("Error sending BAD_TRANS. Error in select"), status);
		}
		send_badtrans = FALSE;
	}
	if (upd_proc_local->bad_trans && bad_trans_detected ||
	    UPDPROC_START == upd_proc_local->start_upd && 1 != report_cnt)
	{
		if (UPDPROC_START == upd_proc_local->start_upd)
		{
			assert(is_updproc_alive() != SRV_ALIVE);
			upd_proc_local->upd_proc_shutdown = NO_SHUTDOWN;
		}
		recvpool_ctl->wrapped = FALSE;
		recvpool_ctl->write_wrap = recvpool_ctl->recvpool_size;
		recvpool_ctl->write = 0;
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
			REPL_DPRINT1("gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because bad trans sent\n");
			gtmrecv_wait_for_jnl_seqno = TRUE;/* set this to TRUE to break out and go back to a fresh "do_main_loop" */
			gtmrecv_bad_trans_sent = TRUE;
			QWASSIGN(recvpool_ctl->jnl_seqno, upd_proc_local->read_jnl_seqno); /* This was the bad transaction */
			upd_proc_local->bad_trans = FALSE;
		}
	}
	if (0 == *pending_data_len && 0 != gtmrecv_local->changelog)
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
			util_log_open(STR_AND_LEN(gtmrecv_local->log_file));
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Change log to %s successful\n",gtmrecv_local->log_file);
		}
		upd_proc_local->changelog = gtmrecv_local->changelog; /* Ask the update process to changelog request */
		/* NOTE: update process and receiver each ignore any setting specific to the other (REPLIC_CHANGE_UPD_LOGINTERVAL,
		 * REPLIC_CHANGE_LOGINTERVAL) */
		gtmrecv_local->changelog = 0;
	}
	if (0 == *pending_data_len && !gtmrecv_logstats && gtmrecv_local->statslog)
	{
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Stats logging not supported on VMS\n");
	} else if (0 == *pending_data_len && gtmrecv_logstats && !gtmrecv_local->statslog)
	{
		gtmrecv_logstats = FALSE;
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
	while (CONTINUE_POLL == gtmrecv_poll_actions1(&pending_data_len, &buff_unprocessed, buffp));
	return (SS_NORMAL);
}
