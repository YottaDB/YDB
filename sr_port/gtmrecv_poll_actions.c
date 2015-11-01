/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

#include <sys/wait.h>
#include <errno.h>
#include <arpa/inet.h>
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
#include "repl_shutdcode.h"

#ifdef REPL_RECVR_HELP_UPD
#include "jnl.h"
#endif

#include "gtmrecv.h"
#include "repl_comm.h"
#include "repl_msg.h"
#include "repl_dbg.h"
#include "repl_log.h"
#include "repl_errno.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#ifdef UNIX
#include "gtmio.h"
#endif

#ifdef REPL_RECVR_HELP_UPD
#include "error.h"
#endif
#include "util.h"
#include "tp_change_reg.h"

#ifdef REPL_RECVR_HELP_UPD
GBLREF	gd_addr                 *gd_header;
GBLREF	gd_region               *gv_cur_region;
GBLREF	sgmnt_data_ptr_t        cs_data;
#endif

GBLREF	repl_msg_ptr_t		gtmrecv_msgp;
GBLREF	int			gtmrecv_max_repl_msglen;
GBLREF  struct timeval          gtmrecv_poll_interval, gtmrecv_poll_immediate;
GBLREF	int			gtmrecv_listen_sock_fd;
GBLREF	int			gtmrecv_sock_fd;
GBLREF	boolean_t		repl_connection_reset;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			gtmrecv_log_fd;
GBLREF 	FILE			*gtmrecv_log_fp;
GBLREF	int			gtmrecv_statslog_fd;
GBLREF 	FILE			*gtmrecv_statslog_fp;
GBLREF	boolean_t		gtmrecv_logstats;
GBLREF	boolean_t		gtmrecv_wait_for_jnl_seqno;
GBLREF	boolean_t		gtmrecv_bad_trans_sent;
GBLREF	pid_t			updproc_pid;

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

#ifdef REPL_RECVR_HELP_UPD
CONDITION_HANDLER(helper_ch)
{
	int	dummy1, dummy2;
	START_CH;
	PRN_ERROR;
	UNWIND(dummy1, dummy2);
}
#endif

int gtmrecv_poll_actions1(int *pending_data_len, int *buff_unprocessed, unsigned char *buffp)
{
	static int		report_cnt = 1;
	static int		next_report_at = 1;
	static boolean_t	send_xoff = FALSE;
	static boolean_t	xoff_sent = FALSE;
	static boolean_t	send_badtrans = FALSE;
	static boolean_t	upd_shut_too_early_logged = FALSE;
	static repl_msg_t	xoff_msg, bad_trans_msg;

	boolean_t	alert = FALSE, info = FALSE;
	int		return_status;
	gd_region       *region_top;
	unsigned char	*msg_ptr;
	int		send_len, sent_len, recvd_len, recv_len, temp_len;
	int		pending_msg_size;
	int		upd_start_status, upd_start_attempts, status;
	int		buffered_data_len;
	int		upd_exit_status;
	boolean_t	bad_trans_detected = FALSE;
	uint4		jnl_status;

	error_def(ERR_REPLCOMM);
	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_TEXT);
	jnl_status = 0;

	if (SHUTDOWN == recvpool.gtmrecv_local->shutdown)
	{
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Shutdown signalled\n");
		gtmrecv_end(); /* Won't return */
	}

	/* Reset report_cnt and next_report_at to 1 when a new upd proc is
	 * forked */
	if (1 == report_cnt || report_cnt == next_report_at)
	{
		if ((alert =
		     (NO_SHUTDOWN == recvpool.upd_proc_local->upd_proc_shutdown
		      && SRV_DEAD == is_updproc_alive() &&
		      NO_SHUTDOWN == recvpool.upd_proc_local->upd_proc_shutdown)) ||
		    (info = ((NORMAL_SHUTDOWN == recvpool.upd_proc_local->upd_proc_shutdown ||
			     ABNORMAL_SHUTDOWN == recvpool.upd_proc_local->upd_proc_shutdown)) &&
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
				pid_t waitpid_res;

				send_xoff = TRUE;
				QWASSIGN(recvpool.recvpool_ctl->old_jnl_seqno, recvpool.recvpool_ctl->jnl_seqno);
				QWASSIGNDW(recvpool.recvpool_ctl->jnl_seqno, 0);
				UNIX_ONLY(
					WAITPID(updproc_pid, &upd_exit_status, 0, waitpid_res); /* Release defunct upd proc */
					assert(updproc_pid == waitpid_res);
				)
				recvpool.upd_proc_local->bad_trans = FALSE; /* No point in doing bad transaction processing */
			}
			gtmrecv_wait_for_jnl_seqno = TRUE;
			REPL_DPRINT1(
			       "gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because of upd crash/shutdown\n");
			next_report_at *= GTMRECV_NEXT_REPORT_FACTOR;
			report_cnt++;
		}
	} else
		report_cnt++;

	if (recvpool.upd_proc_local->bad_trans && !send_badtrans)
	{
		send_xoff = TRUE;
		send_badtrans = TRUE;
		bad_trans_detected = TRUE;
	} else if (!recvpool.upd_proc_local->bad_trans && send_badtrans && 1 != report_cnt)
	{
		send_badtrans = FALSE;
		bad_trans_detected = FALSE;
	}

	if (send_xoff && !xoff_sent && -1 != gtmrecv_sock_fd)
	{
		/* Send XOFF */
		xoff_msg.type = REPL_XOFF_ACK_ME;
		memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&recvpool.upd_proc_local->read_jnl_seqno, sizeof(seq_num));
		xoff_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xoff_msg, xoff_msg.len, &gtmrecv_poll_immediate)
			; /* Empty Body */
		if (SS_NORMAL != status)
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_close(&gtmrecv_sock_fd);
				repl_connection_reset = TRUE;
				xoff_sent = FALSE;
				send_badtrans = FALSE;
			} else if (EREPL_SEND == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error sending XOFF msg due to BAD_TRANS or UPD crash/shutdown. "
							"Error in send"), status);
			else if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error sending XOFF msg due to BAD_TRANS or UPD crash/shutdown. "
							"Error in select"), status);
		} else
			xoff_sent = TRUE;
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
		repl_log(gtmrecv_log_fp, TRUE, TRUE,
				"REPL INFO - Draining replication pipe due to %s\n",
				send_badtrans ? "BAD_TRANS" : "UPD shutdown/crash");

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
				assert(0 == ((unsigned long)buffp & (sizeof(((repl_msg_ptr_t)buffp)->type) - 1)));
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
				       (REPL_MSG_HDRLEN - *buff_unprocessed), &gtmrecv_poll_interval)
				; /* Empty Body */

			REPL_DPRINT3("gtmrecv_poll_actions : Received %d type of message of length %d while draining\n",
					((repl_msg_ptr_t)gtmrecv_msgp)->type, ((repl_msg_ptr_t)gtmrecv_msgp)->len);
		}

		if (SS_NORMAL == status &&
				(0 != *buff_unprocessed || 0 == *pending_data_len) && REPL_XOFF_ACK == gtmrecv_msgp->type)
		{
			/* The rest of the XOFF_ACK msg */
			REPL_RECV_LOOP(gtmrecv_sock_fd, gtmrecv_msgp, (MIN_REPL_MSGLEN - REPL_MSG_HDRLEN), &gtmrecv_poll_interval)
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
				REPL_RECV_LOOP(gtmrecv_sock_fd, gtmrecv_msgp, temp_len, &gtmrecv_poll_interval)
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
				if (REPL_CONN_RESET(status) || ETIMEDOUT == status)
				{
					repl_close(&gtmrecv_sock_fd);
					repl_connection_reset = TRUE;
					xoff_sent = FALSE;
					send_badtrans = FALSE;
					return_status = STOP_POLL;
				} else
					rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Error while draining replication pipe. Error in recv"), status);
			} else if (EREPL_SELECT == repl_errno)
			{
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error while draining replication pipe. Error in select"), status);
			}
		}
	} else
		return_status = STOP_POLL;

	if (STOP_POLL == return_status && send_badtrans && -1 != gtmrecv_sock_fd)
	{
		/* Send BAD_TRANS */
		bad_trans_msg.type = REPL_BADTRANS;
		memcpy((uchar_ptr_t)&bad_trans_msg.msg[0], (uchar_ptr_t)&recvpool.upd_proc_local->read_jnl_seqno, sizeof(seq_num));
		bad_trans_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &bad_trans_msg, bad_trans_msg.len, &gtmrecv_poll_immediate)
			; /* Empty Body */
		if (SS_NORMAL == status)
		{
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
					"Sent bad trans msg with seqno %ld\n", recvpool.upd_proc_local->read_jnl_seqno);
		} else
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_close(&gtmrecv_sock_fd);
				repl_connection_reset = TRUE;
				return_status = STOP_POLL;
			} else if (EREPL_SEND == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Error sending BAD_TRANS. Error in send"), status);
			else if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Error sending BAD_TRANS. Error in select"), status);
		}
		send_badtrans = FALSE;
	}

	if (recvpool.upd_proc_local->bad_trans && bad_trans_detected ||
	    UPDPROC_START == recvpool.upd_proc_local->start_upd && 1 != report_cnt)
	{
		if (UPDPROC_START == recvpool.upd_proc_local->start_upd)
		{
			assert(is_updproc_alive() != SRV_ALIVE);
			recvpool.upd_proc_local->upd_proc_shutdown = NO_SHUTDOWN;
		}
		recvpool.recvpool_ctl->wrapped = FALSE;
		recvpool.recvpool_ctl->write_wrap = recvpool.recvpool_ctl->recvpool_size;
		recvpool.recvpool_ctl->write = 0;

		if (UPDPROC_START == recvpool.upd_proc_local->start_upd)
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
			if (UPDPROC_STARTED == (recvpool.upd_proc_local->start_upd = upd_start_status))
			{
				REPL_DPRINT1("gtmrecv_poll_actions : Setting gtmrecv_wait_for_jnl_seqno to TRUE because of "
					     "upd restart\n");
				gtmrecv_wait_for_jnl_seqno = TRUE;
				report_cnt = next_report_at = 1;
				if (send_xoff && -1 == gtmrecv_sock_fd)
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
			gtmrecv_wait_for_jnl_seqno = TRUE;
			gtmrecv_bad_trans_sent = TRUE;
			QWASSIGN(recvpool.recvpool_ctl->jnl_seqno,
					recvpool.upd_proc_local->read_jnl_seqno); /* This was the bad transaction */
			recvpool.upd_proc_local->bad_trans = FALSE;
		}
	}

	if (0 == *pending_data_len && recvpool.gtmrecv_local->changelog)
	{
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Changing log file to %s\n", recvpool.gtmrecv_local->log_file);
#ifdef UNIX
		repl_log_init(REPL_GENERAL_LOG, &gtmrecv_log_fd, NULL, recvpool.gtmrecv_local->log_file, NULL);
		repl_log_fd2fp(&gtmrecv_log_fp, gtmrecv_log_fd);
#elif defined(VMS)
		util_log_open(STR_AND_LEN(recvpool.gtmrecv_local->log_file));
#else
#error Unsupported platform
#endif
		recvpool.gtmrecv_local->changelog = FALSE;
		/* Ask the update process to change log file */
		recvpool.upd_proc_local->changelog = TRUE;
	}

	if (0 == *pending_data_len && !gtmrecv_logstats && recvpool.gtmrecv_local->statslog)
	{
#ifdef UNIX
		gtmrecv_logstats = TRUE;
		repl_log_init(REPL_STATISTICS_LOG, &gtmrecv_log_fd, &gtmrecv_statslog_fd, recvpool.gtmrecv_local->log_file,
				recvpool.gtmrecv_local->statslog_file);
		repl_log_fd2fp(&gtmrecv_statslog_fp, gtmrecv_statslog_fd);
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Starting stats log to %s\n", recvpool.gtmrecv_local->statslog_file);
		repl_log(gtmrecv_statslog_fp, TRUE, TRUE, "Begin statistics logging\n");
#else
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Stats logging not supported on VMS\n");
#endif
	} else if (0 == *pending_data_len && gtmrecv_logstats && !recvpool.gtmrecv_local->statslog)
	{
		gtmrecv_logstats = FALSE;
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Stopping stats log\n");
		/* Force all data out to the file before closing the file */
		repl_log(gtmrecv_statslog_fp, TRUE, TRUE, "End statistics logging\n");
		UNIX_ONLY(CLOSEFILE(gtmrecv_statslog_fd, status);) VMS_ONLY(close(gtmrecv_statslog_fd);)
		gtmrecv_statslog_fd = -1;
		/* We need to FCLOSE because a later open() in repl_log_init() might return the same file descriptor as the one
		 * that we just closed. In that case, FCLOSE done in repl_log_fd2fp() affects the newly opened file and
		 * FDOPEN will fail returning NULL for the file pointer. So, we close both the file descriptor and file pointer.
		 * Note the same problem does not occur with GENERAL LOG because the current log is kept open while opening
		 * the new log and hence the new file descriptor will be different (we keep the old log file open in case there
		 * are errors during DUPing. In such a case, we do not switch the log file, but keep the current one).
		 * We can FCLOSE the old file pointer later in repl_log_fd2fp() */
		FCLOSE(gtmrecv_statslog_fp, status);
		gtmrecv_statslog_fp = NULL;
	}

#ifdef REPL_RECVR_HELP_UPD

	VMS_ONLY(GTMASSERT);	/* not clear if the following #ifdefed code will work in VMS */
	if (0 != *pending_data_len)
	{
		ESTABLISH(helper_ch);
		region_top = gd_header->regions + gd_header->n_regions;
		for (gv_cur_region = gd_header->regions; gv_cur_region < region_top; gv_cur_region++)
		{
			tp_change_reg();
			if (JNL_ENABLED(cs_data))
			{
				util_out_print("channel = !UL :: fd_mismatch = !UL :: jnl_file.u.inode = !UL", TRUE,
						cs_addrs->jnl->channel, cs_addrs->jnl->fd_mismatch, cs_addrs->nl->jnl_file.u.inode);
				if (((NOJNL == cs_addrs->jnl->channel)  ||  TRUE == cs_addrs->jnl->fd_mismatch)
					&& (0 != cs_addrs->nl->jnl_file.u.inode))
				{
					grab_crit(gv_cur_region);
					if (0 != cs_addrs->nl->jnl_file.u.inode)
					{
						jnl_status = jnl_ensure_open();
						if (0 != jnl_status)
							 rts_error(VARLSTCNT(6) jnl_status, 4, cs_data->jnl_file_len,
									 cs_data->jnl_file_name, gv_cur_region->dyn.addr->fname_len,
									 gv_cur_region->dyn.addr->fname );
					}

					rel_crit(gv_cur_region);
					cs_addrs->jnl->fd_mismatch = FALSE;
				}
				if (NOJNL != cs_addrs->jnl->channel)
					wcs_wtstart(gv_cur_region, 0);
			}
		}
		REVERT;
	}
#endif
	return (return_status);
}

int gtmrecv_poll_actions(int pending_data_len, int buff_unprocessed, unsigned char *buffp)
{
	while (CONTINUE_POLL == gtmrecv_poll_actions1(&pending_data_len, &buff_unprocessed, buffp));
	return (SS_NORMAL);
}
