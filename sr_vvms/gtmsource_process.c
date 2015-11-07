/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_time.h"
#include "gtm_stat.h"

#include <errno.h>
#include <signal.h>
#include <descrip.h> /* Required for gtmsource.h */
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
#include "hashtab_mname.h"    /* needed for muprec.h */
#include "hashtab_int4.h"     /* needed for muprec.h */
#include "hashtab_int8.h"     /* needed for muprec.h */
#include "buddy_list.h"
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "gt_timer.h"
#include "gtmsource_heartbeat.h"
#include "repl_filter.h"
#include "repl_log.h"
#include "min_max.h"
#include "rel_quant.h"
#include "copy.h"
#include "repl_sort_tr_buff.h"
#include "replgbl.h"

#define MAX_HEXDUMP_CHARS_PER_LINE	26 /* 2 characters per byte + space, 80 column assumed */

GBLDEF	seq_num			gtmsource_save_read_jnl_seqno;
GBLDEF	gtmsource_state_t	gtmsource_state = GTMSOURCE_DUMMY_STATE;
GBLDEF	repl_msg_ptr_t		gtmsource_msgp = NULL;
GBLDEF	int			gtmsource_msgbufsiz = 0;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;

GBLDEF	qw_num			repl_source_data_sent = 0;
GBLDEF	qw_num			repl_source_msg_sent = 0;
GBLDEF	qw_num			repl_source_lastlog_data_sent = 0;
GBLDEF	qw_num			repl_source_lastlog_msg_sent = 0;
GBLDEF	time_t			repl_source_prev_log_time;
GBLDEF  time_t			repl_source_this_log_time;
GBLDEF	gd_region		*gtmsource_mru_reg;
GBLDEF	time_t			gtmsource_last_flush_time;
GBLDEF	seq_num			gtmsource_last_flush_reg_seq, gtmsource_last_flush_resync_seq;

GBLREF	volatile time_t		gtmsource_now;
GBLREF	int			gtmsource_sock_fd;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gd_addr			*gd_header;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	repl_ctl_element	*repl_ctl_list;
GBLREF	gtmsource_options_t	gtmsource_options;

GBLREF	int			gtmsource_log_fd;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_filter;
GBLREF	gd_addr			*gd_header;
GBLREF	seq_num			seq_num_zero, seq_num_minus_one, seq_num_one;
GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	boolean_t 		primary_side_std_null_coll;
GBLREF	boolean_t 		secondary_side_std_null_coll;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;
GBLREF	qw_num			trans_sent_cnt, last_log_tr_sent_cnt;

error_def(ERR_REPLCOMM);
error_def(ERR_TEXT);
error_def(ERR_REPLRECFMT);
error_def(ERR_JNLSETDATA2LONG);
error_def(ERR_JNLNEWREC);
error_def(ERR_REPLGBL2LONG);
error_def(ERR_SECNODZTRIGINTP);

int gtmsource_process(void)
{
	/* The work-horse of the Source Server */

	gtmsource_local_ptr_t	gtmsource_local;
	jnlpool_ctl_ptr_t	jctl;
	seq_num			recvd_seqno, sav_read_jnl_seqno;
	seq_num			recvd_jnl_seqno, tmp_read_jnl_seqno;
	int			data_len, srch_status, poll_time;
	unsigned char		*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int			status;					/* needed for REPL_{SEND,RECV}_LOOP */
	int			tot_tr_len, send_tr_len, remaining_len;
	int			recvd_msg_type, recvd_start_flags;
	repl_msg_t		xoff_ack;
	repl_msg_ptr_t		send_msgp;
	uchar_ptr_t		in_buff, out_buff, out_buffmsg;
	uint4			in_buflen, in_size, out_size, tot_out_size, pre_intlfilter_datalen;
	seq_num			log_seqno, diff_seqno, pre_read_jnl_seqno, post_read_jnl_seqno, jnl_seqno;
	char			err_string[1024];
	boolean_t		xon_wait_logged;
	double			time_elapsed;
	seq_num			resync_seqno, old_resync_seqno, curr_seqno, filter_seqno;
	gd_region		*reg, *region_top, *gtmsource_upd_reg, *old_upd_reg;
	sgmnt_addrs		*csa;
	boolean_t		was_crit;
	uint4			temp_dw, out_bufsiz, out_buflen;
	qw_num			backlog_bytes, backlog_count;
	long			prev_msg_sent = 0;
	time_t			prev_now = 0, save_now;
	int			index;
	uint4 			temp_ulong;
	DEBUG_ONLY(uchar_ptr_t	save_inbuff;)
	DEBUG_ONLY(uchar_ptr_t	save_outbuff;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(REPL_MSG_HDRLEN == SIZEOF(jnldata_hdr_struct)); /* necessary for reading multiple transactions from jnlpool in
								* a single attempt */
	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	gtmsource_msgp = NULL;
	gtmsource_msgbufsiz = MAX_REPL_MSGLEN;

	assert(REPL_POLL_WAIT < MILLISECS_IN_SEC);
	assert(GTMSOURCE_IDLE_POLL_WAIT < REPL_POLL_WAIT);

	gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
	while (TRUE)
	{
		gtmsource_stop_heartbeat();
		gtmsource_reinit_logseqno();
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
		{
			gtmsource_est_conn();
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Connected to secondary, using TCP send buffer size %d "
					"receive buffer size %d\n", repl_max_send_buffsize, repl_max_recv_buffsize);
			repl_source_data_sent = repl_source_msg_sent = 0;
			repl_source_lastlog_data_sent = 0;
			repl_source_lastlog_msg_sent = 0;

			gtmsource_alloc_msgbuff(MAX_REPL_MSGLEN);
			gtmsource_state = GTMSOURCE_WAITING_FOR_RESTART;
			repl_source_prev_log_time = time(NULL);
		}
		if (GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state &&
		    SS_NORMAL != (status = gtmsource_recv_restart(&recvd_seqno, &recvd_msg_type, &recvd_start_flags)))
		{
			if (EREPL_RECV == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{
					/* Connection reset */
					repl_log(gtmsource_log_fp, TRUE, TRUE,
						"Connection reset while receiving restart SEQNO. Status = %d ; %s\n",
						status, STRERROR(status));
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				} else
				{
					SNPRINTF(err_string, SIZEOF(err_string),
							"Error receiving RESTART SEQNO. Error in recv : %s", STRERROR(status));
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
							LEN_AND_STR(err_string));
				}
			} else if (EREPL_SEND == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE,
					       "Connection reset while sending XOFF_ACK due to possible update process shutdown. "
					       "Status = %d ; %s\n", status, STRERROR(status));
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				}
				SNPRINTF(err_string, SIZEOF(err_string), "Error sending XOFF_ACK_ME message. Error in send : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_STR(err_string));
			} else if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(err_string, SIZEOF(err_string), "Error receiving RESTART SEQNO/sending XOFF_ACK_ME.  "
						"Error in select : %s", STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_STR(err_string));
			}
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
				memset(gtmsource_msgp, 0, MIN_REPL_MSGLEN); /* to idenitify older releases in the future */
				gtmsource_msgp->type = REPL_WILL_RESTART_WITH_INFO;
				((repl_start_reply_msg_ptr_t)gtmsource_msgp)->jnl_ver = jnl_ver;
				temp_ulong = (0 == primary_side_std_null_coll) ?  START_FLAG_NONE : START_FLAG_COLL_M;
				temp_ulong |= START_FLAG_SRCSRV_IS_VMS;
				PUT_ULONG(((repl_start_reply_msg_ptr_t)gtmsource_msgp)->start_flags, temp_ulong);
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
		REPL_SEND_LOOP(gtmsource_sock_fd, gtmsource_msgp, gtmsource_msgp->len, REPL_POLL_NOWAIT)
		{
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
		}
		if (SS_NORMAL != status)
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE,
						"Connection reset while sending REPL_WILL_RESTART/RESYNC_SEQNO. "
						"Status = %d ; %s\n", status, STRERROR(status));
				repl_close(&gtmsource_sock_fd);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
				gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
				continue;
			}
			if (EREPL_SEND == repl_errno)
			{
				SNPRINTF(err_string, SIZEOF(err_string), "Error sending ROLLBACK FIRST message. Error in send : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_STR(err_string));
			}
			if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(err_string, SIZEOF(err_string), "Error sending ROLLBACK FIRST message. "
						"Error in select : %s", STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_STR(err_string));
			}
		}
		if (REPL_WILL_RESTART_WITH_INFO != gtmsource_msgp->type)
		{
			assert(gtmsource_msgp->type == REPL_RESYNC_SEQNO || gtmsource_msgp->type == REPL_ROLLBACK_FIRST);
			if (REPL_RESYNC_SEQNO == gtmsource_msgp->type)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "RESYNC_SEQNO msg sent with SEQNO "INT8_FMT"\n",
					 (*(seq_num *)&gtmsource_msgp->msg[0]));
				QWASSIGN(resync_seqno, recvd_seqno);
				if (QWLE(gtmsource_local->read_jnl_seqno, resync_seqno))
					QWASSIGN(resync_seqno, gtmsource_local->read_jnl_seqno);
				QWASSIGN(old_resync_seqno, seq_num_zero);
				QWASSIGN(curr_seqno, seq_num_zero);
				region_top = gd_header->regions + gd_header->n_regions;
				for (reg = gd_header->regions; reg < region_top; reg++)
				{
					csa = &FILE_INFO(reg)->s_addrs;
					if (REPL_ALLOWED(csa->hdr))
					{
						if (QWLT(old_resync_seqno, csa->hdr->old_resync_seqno))
							QWASSIGN(old_resync_seqno, csa->hdr->old_resync_seqno);
						if (QWLT(curr_seqno, csa->hdr->reg_seqno))
							QWASSIGN(curr_seqno, csa->hdr->reg_seqno);
					}
				}
			 	assert(QWNE(old_resync_seqno, seq_num_zero));
				REPL_DPRINT2("BEFORE FINDING RESYNC - old_resync_seqno is "INT8_FMT, old_resync_seqno);
				REPL_DPRINT2(", curr_seqno is "INT8_FMT"\n", curr_seqno);
				if (QWNE(old_resync_seqno, resync_seqno))
				{
					assert(QWGE(curr_seqno, gtmsource_local->read_jnl_seqno));
					QWDECRBY(resync_seqno, seq_num_one);
					gtmsource_update_resync_tn(resync_seqno);
					region_top = gd_header->regions + gd_header->n_regions;
					for (reg = gd_header->regions; reg < region_top; reg++)
					{
						csa = &FILE_INFO(reg)->s_addrs;
						if (REPL_ALLOWED(csa->hdr))
						{
							REPL_DPRINT4("Assigning "INT8_FMT" to old_resyc_seqno of %s. Prev value "
								INT8_FMT"\n", resync_seqno, reg->rname, csa->hdr->old_resync_seqno);
							/* Although csa->hdr->old_resync_seqno is only modified by the source
							 * server and never concurremntly, it is read by fileheader_sync() which
							 * does it while in crit. To avoid the latter from reading an inconsistent
							 * value (i.e. neither the pre-update nor the post-update value, which is
							 * possible if the 8-byte operation is not atomic but a sequence of two
							 * 4-byte operations AND if the pre-update and post-update value differ in
							 * their most significant 4-bytes) we grab crit. We could have used the
							 * QWCHANGE_IS_READER_CONSISTENT macro (which checks for most significant
							 * 4-byte differences) instead to determine if it is really necessary to
							 * grab crit. But since the update to old_resync_seqno is a rare operation,
							 * we decide to play it safe.
							 */
							if (FALSE == (was_crit = csa->now_crit))
								grab_crit(reg);
							QWASSIGN(csa->hdr->old_resync_seqno, resync_seqno);
							if (FALSE == was_crit)
								rel_crit(reg);
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

		/* The variable poll_time indicates if we should wait for the receive pipe to be I/O ready and should be set to
		 * a non-zero value ONLY if the source server has nothing to send. At this point we have data to send and so
		 * set poll_time to no-wait.
		 */
		poll_time = REPL_POLL_NOWAIT;
		gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
		gtmsource_init_heartbeat();

		if ((jnl_ver >= remote_jnl_ver) && (IF_NONE != repl_filter_cur2old[remote_jnl_ver - JNL_VER_EARLIEST_REPL]))
		{
			assert(IF_INVALID != repl_filter_cur2old[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
			assert(IF_INVALID != repl_filter_old2cur[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
			/* reverse transformation should exist */
			assert(IF_NONE != repl_filter_old2cur[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
			if (FALSE != ((TREF(replgbl)).null_subs_xform = (primary_side_std_null_coll &&
					!secondary_side_std_null_coll || secondary_side_std_null_coll &&
					!primary_side_std_null_coll)))
				(TREF(replgbl)).null_subs_xform = (primary_side_std_null_coll ?
							STDNULL_TO_GTMNULL_COLL : GTMNULL_TO_STDNULL_COLL);
			/* note that if jnl_ver == remote_jnl_ver and jnl_ver > V15_JNL_VER, the two sides may be running
			 * different null collation. However, we leave the overhead of null collation transformation to
			 * the receiver as source server is generally more loaded than the receiver
			 */
			gtmsource_filter |= INTERNAL_FILTER;
			gtmsource_alloc_filter_buff(gtmsource_msgbufsiz);
		} else
		{
			gtmsource_filter &= ~INTERNAL_FILTER;
			if (NO_FILTER == gtmsource_filter)
				gtmsource_free_filter_buff();
		}
		xon_wait_logged = FALSE;
		gtmsource_upd_reg = gtmsource_mru_reg = NULL;
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
		{
			csa = &FILE_INFO(reg)->s_addrs;
			if (REPL_ALLOWED(csa->hdr))
			{
				if (NULL == gtmsource_upd_reg)
					gtmsource_upd_reg = gtmsource_mru_reg = reg;
				else if (csa->hdr->reg_seqno > FILE_INFO(gtmsource_mru_reg)->s_addrs.hdr->reg_seqno)
					gtmsource_mru_reg = reg;
			}
		}
		/* source server startup and change of mode flush all regions, so we are okay to consider the current state
		 * as completely flushed */
		gtmsource_last_flush_time = gtmsource_now;
		gtmsource_last_flush_reg_seq = jctl->jnl_seqno;
		gtmsource_last_flush_resync_seq = gtmsource_local->read_jnl_seqno;
		prev_now = gtmsource_now;
		while (TRUE)
		{
			gtmsource_poll_actions(TRUE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;
			if (prev_now != gtmsource_now)
			{
				prev_now = gtmsource_now;
				if (gtmsource_msgbufsiz - MAX_REPL_MSGLEN > 2 * OS_PAGE_SIZE)
				{	/* We have expanded the buffer by too much (could have been avoided had we sent one
					 * transaction at a time while reading from journal files); let's revert back to our
					 * initial buffer size. If we don't reduce our buffer, it is possible that the buffer keeps
					 * growing (while reading * from journal file) thus making the size of sends while reading
					 * from journal pool very large (> 1 MB).
					 */
					gtmsource_free_filter_buff();
					gtmsource_free_msgbuff();
					gtmsource_alloc_msgbuff(MAX_REPL_MSGLEN); /* will also allocate filter buffer */
				}
			}
			/* Check if receiver sent us any control message. Typically, the traffic from receiver to source is very
			 * low compared to traffic in the other direction. More often than not, there will be nothing on the pipe
			 * to receive. Ideally, we should let TCP notify us when there is data on the pipe (async I/O on Unix and
			 * VMS). But, we are not there yet. Since we do a select() before a recv(), we won't block if there is
			 * nothing in the pipe. So, it shouldn't be an expensive operation even if done before every send. Also,
			 * in doing so, we react to an XOFF sooner than later.
			 */
			/* Make sure we don't sleep for a longer duration if there is something to be sent across */
			assert((GTMSOURCE_SENDING_JNLRECS != gtmsource_state)
					|| ((0 == poll_time) || (GTMSOURCE_IDLE_POLL_WAIT == poll_time)));
			REPL_RECV_LOOP(gtmsource_sock_fd, gtmsource_msgp, MIN_REPL_MSGLEN, poll_time)
			{
				if (0 == recvd_len) /* nothing received in the first attempt, let's try again later */
					break;
				gtmsource_poll_actions(TRUE);
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
				if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
					break;
			}
			if ((SS_NORMAL == status) && (0 != recvd_len))
			{	/* Process the received control message */
				switch(gtmsource_msgp->type)
				{
					case REPL_XOFF:
					case REPL_XOFF_ACK_ME:
						gtmsource_state = GTMSOURCE_WAITING_FOR_XON;
						poll_time = REPL_POLL_WAIT; /* because we are waiting for a REPL_XON */
						repl_log(gtmsource_log_fp, TRUE, TRUE,
							 "REPL_XOFF/REPL_XOFF_ACK_ME received. Send stalled...\n");
						xon_wait_logged = FALSE;
						if (REPL_XOFF_ACK_ME == gtmsource_msgp->type)
						{
							xoff_ack.type = REPL_XOFF_ACK;
							QWASSIGN(*(seq_num *)&xoff_ack.msg[0], *(seq_num *)&gtmsource_msgp->msg[0]);
							xoff_ack.len = MIN_REPL_MSGLEN;
							REPL_SEND_LOOP(gtmsource_sock_fd, &xoff_ack, xoff_ack.len, REPL_POLL_NOWAIT)
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
										"Connection reset while sending REPL_XOFF_ACK. "
										"Status = %d ; %s\n", status, STRERROR(status));
									repl_close(&gtmsource_sock_fd);
									gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
									break;
								}
								if (EREPL_SEND == repl_errno)
								{
									SNPRINTF(err_string, SIZEOF(err_string),
											"Error sending REPL_XOFF_ACK_ME.  "
											"Error in send : %s",
											STRERROR(status));
									rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0,
											ERR_TEXT, 2,
									 		LEN_AND_STR(err_string));
								}
								if (EREPL_SELECT == repl_errno)
								{
									SNPRINTF(err_string, SIZEOF(err_string),
											"Error sending REPL_XOFF_ACK_ME.  "
											"Error in select : %s",
											STRERROR(status));
									rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0,
											ERR_TEXT, 2, LEN_AND_STR(err_string));
								}
							}
						}
						break;

					case REPL_XON:
						gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
						poll_time = REPL_POLL_NOWAIT; /* because we received XON and data ready for send */
						repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_XON received\n");
						heartbeat_stalled = FALSE;
						REPL_DPRINT1("Restarting HEARTBEAT\n");
						break;

					case REPL_BADTRANS:
					case REPL_START_JNL_SEQNO:
						QWASSIGN(recvd_seqno, *(seq_num *)&gtmsource_msgp->msg[0]);
						gtmsource_state = GTMSOURCE_SEARCHING_FOR_RESTART;
						if (REPL_BADTRANS == gtmsource_msgp->type)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE,
							"REPL_BADTRANS received with SEQNO "INT8_FMT"\n", recvd_seqno);
						} else
						{
							recvd_start_flags = ((repl_start_msg_ptr_t)gtmsource_msgp)->start_flags;
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								 "REPL_START_JNL_SEQNO received with SEQNO "INT8_FMT". Possible "
								 "crash of recvr/update process\n", recvd_seqno);
						}
						break;

					case REPL_HEARTBEAT:
						gtmsource_process_heartbeat((repl_heartbeat_msg_t *)gtmsource_msgp);
						break;
					default:
						repl_log(gtmsource_log_fp, TRUE, TRUE, "Message of unknown type %d length %d"
								"received, hex dump follows\n", gtmsource_msgp->type, recvd_len);
						for (index = 0; index < MIN(recvd_len, gtmsource_msgbufsiz - REPL_MSG_HDRLEN); )
						{
							repl_log(gtmsource_log_fp, FALSE, FALSE, "%.2x ",
									gtmsource_msgp->msg[index]);
							if ((++index) % MAX_HEXDUMP_CHARS_PER_LINE == 0)
								repl_log(gtmsource_log_fp, FALSE, FALSE, "\n");
						}
						assert(FALSE);
						break;
				}
			} else if (SS_NORMAL != status)
			{
				if (EREPL_RECV == repl_errno)
				{
					if (REPL_CONN_RESET(status))
					{
						/* Connection reset */
						repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Connection reset while attempting to receive from secondary. "
								"Status = %d ; %s\n", status, STRERROR(status));
						repl_close(&gtmsource_sock_fd);
						SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
						gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
						break;
					} else
					{
						SNPRINTF(err_string, SIZEOF(err_string),
								"Error receiving Control message from Receiver. Error in recv : %s",
								STRERROR(status));
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								LEN_AND_STR(err_string));
					}
				} else if (EREPL_SELECT == repl_errno)
				{
					SNPRINTF(err_string, SIZEOF(err_string),
							"Error receiving Control message from Receiver. Error in select : %s",
							STRERROR(status));
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
							LEN_AND_STR(err_string));
				}
			}
			if (GTMSOURCE_WAITING_FOR_XON == gtmsource_state)
			{
				if (!xon_wait_logged)
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE, "Waiting to receive XON\n");
					heartbeat_stalled = TRUE;
					REPL_DPRINT1("Stalling HEARTBEAT\n");
					xon_wait_logged = TRUE;
				}
				continue;
			}
			if (GTMSOURCE_SEARCHING_FOR_RESTART  == gtmsource_state ||
			    GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;
			assert(gtmsource_state == GTMSOURCE_SENDING_JNLRECS);
			pre_read_jnl_seqno = gtmsource_local->read_jnl_seqno;
			tot_tr_len = gtmsource_get_jnlrecs(&gtmsource_msgp->msg[0], &data_len,
							   gtmsource_msgbufsiz - REPL_MSG_HDRLEN,
							   !(gtmsource_filter & EXTERNAL_FILTER));
			post_read_jnl_seqno = gtmsource_local->read_jnl_seqno;
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;
			if (0 <= tot_tr_len)
			{
				if (0 < data_len)
				{
					APPLY_EXT_FILTER_IF_NEEDED(gtmsource_filter, gtmsource_msgp, data_len, tot_tr_len);
					gtmsource_msgp->type = REPL_TR_JNL_RECS;
					gtmsource_msgp->len = data_len + REPL_MSG_HDRLEN;
					send_msgp = gtmsource_msgp;
					send_tr_len = tot_tr_len;
					if (gtmsource_filter & INTERNAL_FILTER)
					{
						in_buff = gtmsource_msgp->msg;
						in_buflen = data_len; /* size of the first journal record in the converted buffer */
						out_buffmsg = repl_filter_buff;
						out_buff = out_buffmsg + REPL_MSG_HDRLEN;
						out_bufsiz = repl_filter_bufsiz - REPL_MSG_HDRLEN;
						remaining_len = tot_tr_len;
						while (JREC_PREFIX_SIZE <= remaining_len)
						{
							filter_seqno = ((struct_jrec_null *)(in_buff))->jnl_seqno;
							DEBUG_ONLY(
								save_inbuff = in_buff;
								save_outbuff = out_buff;
							)
							APPLY_INT_FILTER(in_buff, in_buflen, out_buff, out_buflen,
											out_bufsiz, status);
							/* Internal filters should not modify the incoming pointers. Assert that. */
							assert((save_inbuff == in_buff) && (save_outbuff == out_buff));
							if (SS_NORMAL == status)
							{	/* adjust various pointers and book-keeping values to move to next
								 * record.
								 */
								((repl_msg_ptr_t)(out_buffmsg))->type = REPL_TR_JNL_RECS;
								((repl_msg_ptr_t)(out_buffmsg))->len = out_buflen + REPL_MSG_HDRLEN;
								out_buffmsg = (out_buff + out_buflen);
								remaining_len -= (in_buflen + REPL_MSG_HDRLEN);
								assert(0 <= remaining_len);
								if (0 >= remaining_len)
									break;
								in_buff += in_buflen;
								in_buflen = ((repl_msg_ptr_t)(in_buff))->len - REPL_MSG_HDRLEN;
								in_buff += REPL_MSG_HDRLEN;
								out_buff = (out_buffmsg + REPL_MSG_HDRLEN);
								out_bufsiz -= (out_buflen + REPL_MSG_HDRLEN);
								assert(0 <= (int)out_bufsiz);
							} else if (EREPL_INTLFILTER_NOSPC == repl_errno)
							{
								REALLOCATE_INT_FILTER_BUFF(out_buff, out_buffmsg, out_bufsiz);
								/* note that in_buff and in_buflen is not changed so that we can
								 * start from where we left
								 */
							} else
							{
								INT_FILTER_RTS_ERROR(filter_seqno);
							}
						}
						assert(0 == remaining_len);
						send_msgp = (repl_msg_ptr_t)repl_filter_buff;
						send_tr_len = out_buffmsg - repl_filter_buff;
					}
					/* ensure that the head of the buffer has the correct type and len */
					assert(REPL_TR_JNL_RECS == send_msgp->type);
					assert(0 == (send_msgp->len % JNL_REC_START_BNDRY));
					assert(send_tr_len && (0 == (send_tr_len % REPL_MSG_HDRLEN)));
					/* The following loop tries to send multiple seqnos in one shot. resync_seqno gets
					 * updated once the send is completely successful. If an error occurs in the middle
					 * of the send, it is possible that we successfully sent a few seqnos to the other side.
					 * In this case resync_seqno should be updated to reflect those seqnos. Not doing so
					 * might cause the secondary to get ahead of the primary in terms of resync_seqno.
					 * Although it is possible to determine the exact seqno where the send partially failed,
					 * we update resync_seqno as if all seqnos were successfully sent (It is ok for the
					 * resync_seqno on the primary side to be a little more than the actual value as long as
					 * the secondary side has an accurate value of resync_seqno. This is because the
					 * resync_seqno of the system is the minimum of the resync_seqno of both primary
					 * and secondary). This is done by the call to gtmsource_flush_fh() done within the
					 * REPL_SEND_LOOP macro as well as in the (SS_NORMAL != status) if condition below.
					 */
					REPL_SEND_LOOP(gtmsource_sock_fd, send_msgp, send_tr_len, REPL_POLL_WAIT)
					{
						gtmsource_poll_actions(FALSE);
						if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						{
							gtmsource_flush_fh(post_read_jnl_seqno);
							return (SS_NORMAL);
						}
					}
					if (SS_NORMAL != status)
					{
						gtmsource_flush_fh(post_read_jnl_seqno);
						if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Connection reset while sending transaction data from "
								INT8_FMT" to "INT8_FMT". Status = %d ; %s\n", pre_read_jnl_seqno,
								post_read_jnl_seqno, status, STRERROR(status));
							repl_close(&gtmsource_sock_fd);
							SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
							gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
							break;
						}
						if (EREPL_SEND == repl_errno)
						{
							SNPRINTF(err_string, SIZEOF(err_string),
								"Error sending DATA. Error in send : %s", STRERROR(status));
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  LEN_AND_STR(err_string));
						}
						if (EREPL_SELECT == repl_errno)
						{
							SNPRINTF(err_string, SIZEOF(err_string),
								"Error sending DATA. Error in select : %s", STRERROR(status));
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  LEN_AND_STR(err_string));
						}
					}
					/* Record the "last sent seqno" in file header of most recently updated region and one
					 * region picked in round robin order. Updating one region is sufficient since the
					 * system's resync_seqno is computed to be the maximum of file header resync_seqno
					 * across all regions. We choose to update multiple regions to increase the odds of
					 * not losing information in case of a system crash
					 */
					UPDATE_RESYNC_SEQNO(gtmsource_mru_reg, pre_read_jnl_seqno, post_read_jnl_seqno);
					if (gtmsource_mru_reg != gtmsource_upd_reg)
						UPDATE_RESYNC_SEQNO(gtmsource_upd_reg, pre_read_jnl_seqno, post_read_jnl_seqno);
					old_upd_reg = gtmsource_upd_reg;
					do
					{	/* select next region in round robin order */
						gtmsource_upd_reg++;
						if (gtmsource_upd_reg >= gd_header->regions + gd_header->n_regions)
							gtmsource_upd_reg = gd_header->regions; /* wrap back to first region */
					} while (gtmsource_upd_reg != old_upd_reg && /* back to the original region? */
							!REPL_ALLOWED(FILE_INFO(gtmsource_upd_reg)->s_addrs.hdr));
					save_now = gtmsource_now;
					if (GTMSOURCE_FH_FLUSH_INTERVAL <= difftime(save_now, gtmsource_last_flush_time))
					{
						gtmsource_flush_fh(post_read_jnl_seqno);
						gtmsource_last_flush_time = save_now;
					}

					repl_source_msg_sent += (qw_num)send_tr_len;
					repl_source_data_sent += (qw_num)(send_tr_len) -
								(post_read_jnl_seqno - pre_read_jnl_seqno) * REPL_MSG_HDRLEN;
					log_seqno = post_read_jnl_seqno - 1; /* post_read_jnl_seqno is the "next" seqno to be sent,
									      * not the last one we sent */
					if (log_seqno - lastlog_seqno >= log_interval || gtmsource_logstats)
					{ /* print always when STATSLOG is ON, or when the log interval has passed */
						trans_sent_cnt += (log_seqno - lastlog_seqno);
						/* jctl->jnl_seqno >= post_read_jnl_seqno is the most common case;
						 * see gtmsource_readpool() for when the rare case can occur */
						jnl_seqno = jctl->jnl_seqno;
						assert(jnl_seqno >= post_read_jnl_seqno - 1);
						diff_seqno = (jnl_seqno >= post_read_jnl_seqno) ?
								(jnl_seqno - post_read_jnl_seqno) : 0;
						repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL INFO - Seqno : "INT8_FMT, log_seqno);
						repl_log(gtmsource_log_fp, FALSE, FALSE, "  Jnl Total : "INT8_FMT"  Msg Total : "
							INT8_FMT"  ", repl_source_data_sent, repl_source_msg_sent);
						repl_log(gtmsource_log_fp, FALSE, TRUE, "Current backlog : "INT8_FMT"\n",
							diff_seqno);
						/* gtmsource_now is updated by the heartbeat protocol every heartbeat
						 * interval. To cut down on calls to time(), we use gtmsource_now as the
						 * time to figure out if we have to log statistics. This works well as the
						 * logging interval generally is larger than the heartbeat interval, and that
						 * the heartbeat protocol is running when we are sending data. The consequence
						 * although is that we may defer logging when we would have logged. We can live
						 * with that given the benefit of not calling time related system calls.
						 * Currently, the logging interval is not changeable by users. When/if we provide
						 * means of choosing log interval, this code may have to be re-examined.
						 * Vinaya 2003, Sep 08
						 */
						assert(0 != gtmsource_now); /* must hold if we are sending data */
						repl_source_this_log_time = gtmsource_now; /* approximate time, in the worst case,
											    * behind by heartbeat interval */
						assert(repl_source_this_log_time >= repl_source_prev_log_time);
						time_elapsed = difftime(repl_source_this_log_time, repl_source_prev_log_time);
						if ((double)GTMSOURCE_LOGSTATS_INTERVAL <= time_elapsed)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO since last log : "
								 "Time elapsed : %00.f  Tr sent : "INT8_FMT"  Tr bytes : "
								 INT8_FMT"  Msg bytes : "INT8_FMT"\n",
								  time_elapsed, trans_sent_cnt - last_log_tr_sent_cnt,
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
						lastlog_seqno = log_seqno;
					}
					/* Because we sent data to the other side and there might be more to be sent across, don't
					 * wait for the receive pipe to be ready.
					 */
					poll_time = REPL_POLL_NOWAIT;
				} else /* data_len == 0 */
				{	/* nothing to send */
					gtmsource_flush_fh(post_read_jnl_seqno);
					/* Sleep for a while (as part of the next REPL_RECV_LOOP) to avoid spinning when there is no
					 * data to be sent
					 */
					poll_time = GTMSOURCE_IDLE_POLL_WAIT;
				}
			} else /* else tot_tr_len < 0, error */
			{
				if (0 < data_len) /* Insufficient buffer space, increase the buffer space */
					gtmsource_alloc_msgbuff(data_len + REPL_MSG_HDRLEN);
				else
					GTMASSERT; /* Major problems */
			}
		}
	}
}
