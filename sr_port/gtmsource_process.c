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

#include "gtm_string.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#include <sys/stat.h>
#include <signal.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_comm.h"
#include "jnl.h"
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "gtmsource_heartbeat.h"
#include "repl_filter.h"
#include "repl_log.h"

#define OVERFLOWN(qw)		(0 == (DWASSIGNQW(temp_dw, qw)))

GBLDEF	seq_num			gtmsource_save_read_jnl_seqno;
GBLDEF	struct timeval		gtmsource_poll_wait, gtmsource_poll_immediate;
GBLDEF	qw_off_t		jnlpool_size;
GBLDEF	gtmsource_state_t	gtmsource_state = GTMSOURCE_DUMMY_STATE;
GBLDEF	repl_msg_ptr_t		gtmsource_msgp = NULL;
GBLDEF	int			gtmsource_msgbufsiz = 0;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;

GBLDEF	long			repl_source_data_sent;
GBLDEF	long			repl_source_msg_sent;
GBLDEF	long			repl_source_lastlog_data_sent = 0;
GBLDEF	long			repl_source_lastlog_msg_sent = 0;
GBLDEF	time_t			repl_source_prev_log_time;
GBLDEF  time_t			repl_source_this_log_time;

GBLREF	int			gtmsource_sock_fd;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gd_addr			*gd_header;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	repl_ctl_element	*repl_ctl_list;
GBLREF	gtmsource_options_t	gtmsource_options;

GBLREF	int			gtmsource_log_fd;
GBLREF	int			gtmsource_statslog_fd;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	FILE			*gtmsource_statslog_fp;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_filter;
GBLREF	gd_addr			*gd_header;
GBLREF	seq_num			seq_num_zero, seq_num_minus_one, seq_num_one;
GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;

void	conn_reset_handler(int signal);

void	conn_reset_handler(int signal)
{
	repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset...waiting for port to shutdown on recv end\n");
 	LONG_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT);
	return;
}

int gtmsource_process(void)
{
	/* The work-horse of the Source Server */

	gtmsource_local_ptr_t	gtmsource_local;
	jnlpool_ctl_ptr_t	jctl;
	seq_num			recvd_seqno, sav_read_jnl_seqno;
	struct sockaddr_in	secondary_addr;
	unsigned char		*msg_ptr;
	seq_num			recvd_jnl_seqno, tmp_read_jnl_seqno;
	int			data_len;
	int			send_len, sent_len, recv_len, recvd_len;
	int			status, srch_status;
	struct timeval		poll_time;
	int			recvd_msg_type, recvd_start_flags;
	struct sigaction	act;
	repl_msg_t		xoff_ack;
	repl_msg_ptr_t		send_msgp;
	uchar_ptr_t		in_buff, out_buff, save_filter_buff;
	uint4			in_size, out_size, out_bufsiz, tot_out_size, pre_intlfilter_datalen;

	seq_num			lastlog_seqno, log_seqno, diff_seqno;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	boolean_t		xon_wait_logged = FALSE;
	double			time_elapsed;
	long			trans_sent_cnt = 0;
	long			last_log_tr_sent_cnt = 0;
	seq_num			resync_seqno, old_resync_seqno, curr_seqno, filter_seqno;
	gd_region		*reg, *region_top;
	sgmnt_addrs		*csa;
	boolean_t		was_crit;
	uint4			temp_dw;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);
	error_def(ERR_JNLRECFMT);
	error_def(ERR_JNLSETDATA2LONG);
	error_def(ERR_JNLNEWREC);

	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	gtmsource_msgp = NULL;
	gtmsource_msgbufsiz = MAX_REPL_MSGLEN;
	QWASSIGNDW(jnlpool_size, jctl->jnlpool_size);

	assert(GTMSOURCE_POLL_WAIT < MAX_GTMSOURCE_POLL_WAIT);
	gtmsource_poll_wait.tv_sec = 0;
	gtmsource_poll_wait.tv_usec = GTMSOURCE_POLL_WAIT;

	gtmsource_poll_immediate.tv_sec = 0;
	gtmsource_poll_immediate.tv_sec = 0;

	repl_source_data_sent = repl_source_msg_sent = 0;
	repl_source_lastlog_data_sent = 0;
	repl_source_lastlog_msg_sent = 0;

	gtmsource_init_sec_addr(&secondary_addr);

	gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;

	/* Setup the loss of connection handler */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = conn_reset_handler;
	sigaction(SIGPIPE, &act, 0);

	while (TRUE)
	{
		gtmsource_stop_heartbeat();

		QWASSIGN(lastlog_seqno, seq_num_minus_one);
		QWDECRBYDW(lastlog_seqno, (LOGTRNUM_INTERVAL - 1));
		trans_sent_cnt = -LOGTRNUM_INTERVAL + 1;

		repl_source_prev_log_time = time(NULL);

		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
		{
			gtmsource_est_conn(&secondary_addr);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Connected to secondary\n");
			gtmsource_alloc_msgbuff(gtmsource_msgbufsiz);
			gtmsource_state = GTMSOURCE_WAITING_FOR_RESTART;
		}
		if (GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state &&
		    SS_NORMAL != (status = gtmsource_recv_restart(&recvd_seqno, &recvd_msg_type, &recvd_start_flags)))
		{
			if (EREPL_RECV == repl_errno)
			{
				if (REPL_CONN_RESET(status) || ETIMEDOUT == status)
				{
					/* Connection reset */
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				} else
					rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Error receiving RESTART SEQNO. Error in recv"), status);
			} else if (EREPL_SEND == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset\n");
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				}
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending XOFF_ACK_ME message. Error in send"), status);
			} else if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error receiving RESTART SEQNO/sending XOFF_ACK_ME. Error in select"),
					  status);
		}
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			return (SS_NORMAL);

		QWASSIGN(sav_read_jnl_seqno, gtmsource_local->read_jnl_seqno);
		if (GTMSOURCE_SEARCHING_FOR_RESTART == gtmsource_state || REPL_START_JNL_SEQNO == recvd_msg_type)
		{
			assert(gtmsource_state == GTMSOURCE_SEARCHING_FOR_RESTART ||
			       gtmsource_state == GTMSOURCE_WAITING_FOR_RESTART);
			gtmsource_state = GTMSOURCE_SEARCHING_FOR_RESTART;
			if (SS_NORMAL == (srch_status = gtmsource_srch_restart(recvd_seqno, recvd_start_flags)))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Sending REPL_WILL_RESTART\n");
				if (remote_jnl_ver > JNL_VER_EARLIEST_REPL)
				{
					memset(gtmsource_msgp, 0, MIN_REPL_MSGLEN); /* to idenitify older releases in the future */
					gtmsource_msgp->type = REPL_WILL_RESTART_WITH_INFO;
					((repl_start_reply_msg_ptr_t)gtmsource_msgp)->jnl_ver = jnl_ver;
				} else
					gtmsource_msgp->type = REPL_WILL_RESTART;
				recvd_start_flags = START_FLAG_NONE;
			} else /* srch_restart returned EREPL_SEC_AHEAD */
			{
				assert(EREPL_SEC_AHEAD == srch_status);
				gtmsource_msgp->type = REPL_ROLLBACK_FIRST;
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Sending REPL_ROLLBACK_FIRST\n");
			}
		} else
		{
			/* REPL_FETCH_RESYNC received and state is WAITING_FOR_RESTART */
			assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state && REPL_FETCH_RESYNC == recvd_msg_type);
			gtmsource_msgp->type = REPL_RESYNC_SEQNO;
		}

		QWASSIGN(*(seq_num *)&((repl_start_reply_msg_ptr_t)gtmsource_msgp)->start_seqno[0],
			 gtmsource_local->read_jnl_seqno);
		gtmsource_msgp->len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmsource_sock_fd, gtmsource_msgp, gtmsource_msgp->len, &gtmsource_poll_immediate)
		{
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
		}
		if (SS_NORMAL != status)
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset\n");
				repl_close(&gtmsource_sock_fd);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
				gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
				continue;
			}
			if (EREPL_SEND == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending ROLLBACK FIRST message. Error in send"), status);
			if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending ROLLBACK FIRST message. Error in select"), status);
		}
		if (REPL_WILL_RESTART != gtmsource_msgp->type && REPL_WILL_RESTART_WITH_INFO != gtmsource_msgp->type)
		{
			assert(gtmsource_msgp->type == REPL_RESYNC_SEQNO ||
			       gtmsource_msgp->type == REPL_ROLLBACK_FIRST);

			if (REPL_RESYNC_SEQNO == gtmsource_msgp->type)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "RESYNC_SEQNO msg sent with SEQNO "INT8_FMT"\n",
					 INT8_PRINT(*(seq_num *)&gtmsource_msgp->msg[0]));
				QWASSIGN(resync_seqno, recvd_seqno);
				if (QWLE(gtmsource_local->read_jnl_seqno, resync_seqno))
					QWASSIGN(resync_seqno, gtmsource_local->read_jnl_seqno);
				QWASSIGN(old_resync_seqno, seq_num_zero);
				QWASSIGN(curr_seqno, seq_num_zero);
				region_top = gd_header->regions + gd_header->n_regions;
				for (reg = gd_header->regions; reg < region_top; reg++)
				{
					csa = &FILE_INFO(reg)->s_addrs;
					if (REPL_ENABLED(csa->hdr))
					{
						if (QWLT(old_resync_seqno, csa->hdr->old_resync_seqno))
							QWASSIGN(old_resync_seqno, csa->hdr->old_resync_seqno);
						if (QWLT(curr_seqno, csa->hdr->reg_seqno))
							QWASSIGN(curr_seqno, csa->hdr->reg_seqno);
					}
				}
			 	assert(QWNE(old_resync_seqno, seq_num_zero));
				REPL_DPRINT2("BEFORE FINDING RESYNC - old_resync_seqno is "INT8_FMT, INT8_PRINT(old_resync_seqno));
				REPL_DPRINT2(", curr_seqno is "INT8_FMT"\n", INT8_PRINT(curr_seqno));
				if (QWNE(old_resync_seqno, resync_seqno))
				{
					assert(QWGE(curr_seqno, gtmsource_local->read_jnl_seqno));
					QWDECRBY(resync_seqno, seq_num_one);
					gtmsource_update_resync_tn(resync_seqno);
					region_top = gd_header->regions + gd_header->n_regions;
					for (reg = gd_header->regions; reg < region_top; reg++)
					{
						csa = &FILE_INFO(reg)->s_addrs;
						if (REPL_ENABLED(csa->hdr))
						{
							REPL_DPRINT2("Assigning "INT8_FMT, INT8_PRINT(resync_seqno));
							REPL_DPRINT3(" to old_resync_seqno of %s. Prev value "INT8_FMT"\n",
									reg->rname, INT8_PRINT(csa->hdr->old_resync_seqno));
							QWASSIGN(csa->hdr->old_resync_seqno, resync_seqno);
						}
					}
				}
			}

			/* Could send a REPL_CLOSE_CONN message here */

			/*
		 	 * It is expected that on receiving this msg, the
		 	 * Receiver Server will break the connection and exit.
		 	 */

		 	repl_close(&gtmsource_sock_fd);
		 	LONG_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT); /* may not be needed after REPL_CLOSE_CONN is sent */
		 	gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
		 	continue;
		}

		if (QWLT(gtmsource_local->read_jnl_seqno, sav_read_jnl_seqno) && NULL != repl_ctl_list)
		{
			/* The journal files may have been positioned ahead of
			 * the read_jnl_seqno for the next read. Indicate that
			 * they have to be repositioned into the past.
			 */
			assert(READ_FILE == gtmsource_local->read_state);
			gtmsource_set_lookback();
		}

		poll_time = gtmsource_poll_immediate;
		gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
		gtmsource_init_heartbeat();
		assert((intlfltr_t)0 != repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
							    [remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
		assert((intlfltr_t)0 != repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
							    [jnl_ver - JNL_VER_EARLIEST_REPL]);
		if (jnl_ver > remote_jnl_ver && IF_NONE != repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
									       [remote_jnl_ver - JNL_VER_EARLIEST_REPL])
		{
			gtmsource_filter |= INTERNAL_FILTER;
			gtmsource_alloc_filter_buff(gtmsource_msgbufsiz);
			/* reverse transformation should exist */
			assert(IF_NONE != repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
							      [jnl_ver - JNL_VER_EARLIEST_REPL]);
		} else
		{
			gtmsource_filter &= ~INTERNAL_FILTER;
			if (NO_FILTER == gtmsource_filter && repl_filter_buff)
			{
				free(repl_filter_buff);
				repl_filter_buff = NULL;
				repl_filter_bufsiz = 0;
			}
		}

		while (TRUE)
		{
			gtmsource_poll_actions(TRUE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;

			/* Check if receiver sent me anything */
			REPL_RECV_LOOP(gtmsource_sock_fd, gtmsource_msgp, MIN_REPL_MSGLEN, &poll_time)
			{
				if (0 == recvd_len && MIN_REPL_MSGLEN == recv_len)
					break;
				gtmsource_poll_actions(TRUE);
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
				if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
					break;
			}
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;

			if (SS_NORMAL == status && 0 != recvd_len)
			{
				/* Process the received control message */
				switch(gtmsource_msgp->type)
				{
					case REPL_XOFF:
					case REPL_XOFF_ACK_ME:
						gtmsource_state = GTMSOURCE_WAITING_FOR_XON;
						poll_time = gtmsource_poll_wait;
						repl_log(gtmsource_log_fp, TRUE, TRUE,
							 "REPL_XOFF/REPL_XOFF_ACK_ME received. Send stalled...\n");
						xon_wait_logged = FALSE;
						if (REPL_XOFF_ACK_ME == gtmsource_msgp->type)
						{
							xoff_ack.type = REPL_XOFF_ACK;
							QWASSIGN(*(seq_num *)&xoff_ack.msg[0], *(seq_num *)&gtmsource_msgp->msg[0]);
							xoff_ack.len = MIN_REPL_MSGLEN;
							REPL_SEND_LOOP(gtmsource_sock_fd, &xoff_ack, xoff_ack.len,
								       &gtmsource_poll_immediate)
							{
								gtmsource_poll_actions(FALSE);
								if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
									return (SS_NORMAL);
							}
							if (SS_NORMAL == status)
							{
								repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_XOFF_ACK sent...\n");
							} else
							{
								if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
								{
									repl_log(gtmsource_log_fp, TRUE, TRUE,
										"Connection reset\n");
									repl_close(&gtmsource_sock_fd);
									gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
									break;
								}
								if (EREPL_SEND == repl_errno)
									rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
									 RTS_ERROR_LITERAL("Error sending REPL_XOFF_ACK_ME"
									 "message. Error in send"),
									  status);
								if (EREPL_SELECT == repl_errno)
									rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
									 RTS_ERROR_LITERAL("Error sending REPL_XOFF_ACK_ME"
									 "message. Error in select"),
									  status);
							}
						}
						break;

					case REPL_XON:
						gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
						poll_time = gtmsource_poll_immediate;
						repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_XON received\n");
						gtmsource_restart_heartbeat; /*Macro*/
						REPL_DPRINT1("Restarting HEARTBEAT\n");
						xon_wait_logged = FALSE;
						break;

					case REPL_BADTRANS:
					case REPL_START_JNL_SEQNO:
						QWASSIGN(recvd_seqno, *(seq_num *)&gtmsource_msgp->msg[0]);
						gtmsource_state = GTMSOURCE_SEARCHING_FOR_RESTART;
						if (REPL_BADTRANS == gtmsource_msgp->type)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								 "REPL_BADTRANS received with SEQNO "INT8_FMT"\n",
								 INT8_PRINT(recvd_seqno));
						} else
						{
							recvd_start_flags = ((repl_start_msg_ptr_t)gtmsource_msgp)->start_flags;
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								 "REPL_START_JNL_SEQNO received with SEQNO "INT8_FMT". Possible "
								 "crash of recvr/update process\n", INT8_PRINT(recvd_seqno));
						}
						break;

					case REPL_HEARTBEAT:
						gtmsource_process_heartbeat((repl_heartbeat_msg_t *)gtmsource_msgp);
						break;
					default:
						break;
				}
			} else if (SS_NORMAL != status)
			{
				if (EREPL_RECV == repl_errno)
				{
					if (REPL_CONN_RESET(status) || ETIMEDOUT == status)
					{
						/* Connection reset */
						repl_close(&gtmsource_sock_fd);
						SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
						gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
						REPL_DPRINT1("CONN RESET while attempting to receive from secondary\n");
						break;
					} else
						rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
							  RTS_ERROR_LITERAL("Error receiving Control message from Receiver. "
								  	    "Error in recv"), status);
				} else if (EREPL_SELECT == repl_errno)
					rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Error receiving Control message from Receiver. "
							  	    "Error in select"), status);
			}

			if (GTMSOURCE_WAITING_FOR_XON == gtmsource_state)
			{
				if (!xon_wait_logged)
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE, "Waiting to receive XON\n");
					gtmsource_stall_heartbeat; /* Macro */
					REPL_DPRINT1("Stalling HEARTBEAT\n");
					xon_wait_logged = TRUE;
				}
				continue;
			}
			if (GTMSOURCE_SEARCHING_FOR_RESTART == gtmsource_state ||
			    gtmsource_state == GTMSOURCE_WAITING_FOR_CONNECTION)
			{
				xon_wait_logged = FALSE;
				break;
			}

			assert(gtmsource_state == GTMSOURCE_SENDING_JNLRECS);

			status = gtmsource_get_jnlrecs(&gtmsource_msgp->msg[0],
						       &data_len,
			             	     	       gtmsource_msgbufsiz -
						       REPL_MSG_HDRLEN);

			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;

			if (0 == status)
			{
				if (0 < data_len)
				{
					send_msgp = gtmsource_msgp;
					if (gtmsource_filter & EXTERNAL_FILTER)
					{
						QWSUBDW(filter_seqno, gtmsource_local->read_jnl_seqno, 1);
						if (SS_NORMAL != (status = repl_filter(filter_seqno, gtmsource_msgp->msg, &data_len,
									     	       gtmsource_msgbufsiz)))
							repl_filter_error(filter_seqno, status);
					}
					if (gtmsource_filter & INTERNAL_FILTER)
					{
						pre_intlfilter_datalen = data_len;
						in_buff = gtmsource_msgp->msg;
						in_size = pre_intlfilter_datalen;
						out_buff = repl_filter_buff + REPL_MSG_HDRLEN;
						out_bufsiz = repl_filter_bufsiz - REPL_MSG_HDRLEN;
						tot_out_size = 0;
					     	while ((status =
							repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
									    [remote_jnl_ver - JNL_VER_EARLIEST_REPL](
								in_buff, &in_size, out_buff, &out_size, out_bufsiz)) == -1 &&
						       EREPL_INTLFILTER_NOSPC == repl_errno)
						{
							save_filter_buff = repl_filter_buff;
							gtmsource_alloc_filter_buff(repl_filter_bufsiz + (repl_filter_bufsiz >> 1));
							in_buff += in_size;
							in_size = pre_intlfilter_datalen - (in_buff - gtmsource_msgp->msg);
							out_bufsiz = repl_filter_bufsiz - (out_buff - save_filter_buff) - out_size;
							out_buff = repl_filter_buff + (out_buff - save_filter_buff) + out_size;
							tot_out_size += out_size;
						}
						if (0 == status)
						{
							data_len = tot_out_size + out_size;
							send_msgp = (repl_msg_ptr_t)repl_filter_buff;
						} else
						{
							if (EREPL_INTLFILTER_BADREC == repl_errno)
								rts_error(VARLSTCNT(1) ERR_JNLRECFMT);
							else if (EREPL_INTLFILTER_DATA2LONG == repl_errno)
								rts_error(VARLSTCNT(4) ERR_JNLSETDATA2LONG, 2, jnl_source_datalen,
								  	  jnl_dest_maxdatalen);
							else if (EREPL_INTLFILTER_NEWREC == repl_errno)
								rts_error(VARLSTCNT(4) ERR_JNLNEWREC, 2,
									  (unsigned int)jnl_source_rectype,
								  	  (unsigned int)jnl_dest_maxrectype);
							else /* (EREPL_INTLFILTER_INCMPLREC == repl_errno) */
								GTMASSERT;
						}
					}
					send_msgp->type = REPL_TR_JNL_RECS;
					send_msgp->len = data_len + REPL_MSG_HDRLEN;
					REPL_SEND_LOOP(gtmsource_sock_fd, send_msgp, send_msgp->len, &gtmsource_poll_immediate)
					{
						gtmsource_poll_actions(FALSE);
						if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
							return (SS_NORMAL);
					}
					if (SS_NORMAL != status)
					{
						if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset\n");
							repl_close(&gtmsource_sock_fd);
							SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
							gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
							break;
						}
						if (EREPL_SEND == repl_errno)
							rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  RTS_ERROR_LITERAL("Error sending DATA. Error in send"), status);
						if (EREPL_SELECT == repl_errno)
							rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  RTS_ERROR_LITERAL("Error sending DATA. Error in select"), status);
					}
					region_top = gd_header->regions + gd_header->n_regions;
					for (reg = gd_header->regions; reg < region_top; reg++)
					{
						csa = &FILE_INFO(reg)->s_addrs;
						if (REPL_ENABLED(csa->hdr))
						{
#ifndef INT8_SUPPORTED
							/* To deposit correct value of resync_seqno in the file-header,
							 * we grab_crit each time when its lower 4-bytes overflow as the
							 * file-header sync is done in crit,*/
							if (OVERFLOWN(gtmsource_local->read_jnl_seqno) &&
									FALSE == (was_crit = csa->now_crit))
								grab_crit(reg);
#endif
							QWASSIGN(FILE_INFO(reg)->s_addrs.hdr->resync_seqno,
								 gtmsource_local->read_jnl_seqno);
#ifndef INT8_SUPPORTED
							if (OVERFLOWN(gtmsource_local->read_jnl_seqno) && FALSE == was_crit)
								rel_crit(reg);
#endif
						}
					}

					repl_source_data_sent += data_len;
					repl_source_msg_sent += gtmsource_msgp->len;
					QWASSIGN(log_seqno, lastlog_seqno);
					QWINCRBYDW(log_seqno, LOGTRNUM_INTERVAL + 1);
					if (QWLE(log_seqno, gtmsource_local->read_jnl_seqno))
					{
						QWASSIGN(log_seqno, gtmsource_local->read_jnl_seqno);
						QWDECRBYDW(log_seqno, 1);
						trans_sent_cnt += LOGTRNUM_INTERVAL;
						QWSUB(diff_seqno, jctl->jnl_seqno, gtmsource_local->read_jnl_seqno);
						repl_log(gtmsource_log_fp, FALSE, FALSE, "REPL INFO - Tr num : "INT8_FMT,
							 INT8_PRINT(log_seqno));
						repl_log(gtmsource_log_fp, FALSE, FALSE, "  Tr Total : %ld  Msg Total : %ld  ",
							 repl_source_data_sent, repl_source_msg_sent);
						repl_log(gtmsource_log_fp, FALSE, TRUE, "Current backlog : "INT8_FMT"\n",
							 INT8_PRINT(diff_seqno));
						repl_source_this_log_time = time(NULL);
						time_elapsed = difftime(repl_source_this_log_time, repl_source_prev_log_time);
						if ((double)GTMSOURCE_LOGSTATS_INTERVAL <= time_elapsed)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO since last log : "
								 "Time elapsed : %00.f  Tr sent : %ld  Tr bytes : %ld  Msg bytes : "
								 "%ld\n", time_elapsed, trans_sent_cnt - last_log_tr_sent_cnt,
								 repl_source_data_sent - repl_source_lastlog_data_sent,
								 repl_source_msg_sent - repl_source_lastlog_msg_sent);
							repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO since last log : "
								 "Time elapsed : %00.f  Tr sent/s : %f  Tr bytes/s : %f  "
								 "Msg bytes/s : %f\n", time_elapsed,
								 (float)(trans_sent_cnt - last_log_tr_sent_cnt)/time_elapsed,
								 (float)(repl_source_data_sent - repl_source_lastlog_data_sent) /
								 	time_elapsed,
								 (float)(repl_source_msg_sent - repl_source_lastlog_msg_sent) /
								 	time_elapsed);
							repl_source_lastlog_data_sent = repl_source_data_sent;
							repl_source_lastlog_msg_sent = repl_source_msg_sent;
							last_log_tr_sent_cnt = trans_sent_cnt;
							repl_source_prev_log_time = repl_source_this_log_time;
						}
						QWASSIGN(lastlog_seqno, log_seqno);
					}
					if (gtmsource_logstats)
					{
						QWASSIGN(diff_seqno, gtmsource_local->read_jnl_seqno);
						QWDECRBYDW(diff_seqno, 1);
						repl_log(gtmsource_statslog_fp, FALSE, FALSE, "Tr : "INT8_FMT"  Tr Size : %d  "
							 "Tr Total : %d  Msg Size : %d  Msg Total : %d\n", INT8_PRINT(diff_seqno),
							 data_len, repl_source_data_sent, gtmsource_msgp->len,
							 repl_source_msg_sent);
					}
				} else /* data_len == 0 */
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
			} else /* else status < 0, error */
			{
				if (0 < data_len) /* Insufficient buffer space, increase the buffer space */
					gtmsource_alloc_msgbuff(data_len + REPL_MSG_HDRLEN);
				else
					GTMASSERT; /* Major problems */
			}
		}
	}
}
