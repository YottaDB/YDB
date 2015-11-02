/****************************************************************
 *								*
 *	Copyright 2006, 2007 Fidelity Information Services, Inc.*
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
#include "ftok_sems.h"
#include "repl_instance.h"
#include "gtmmsg.h"
#include "repl_sem.h"

#define MAX_HEXDUMP_CHARS_PER_LINE	26 /* 2 characters per byte + space, 80 column assumed */

GBLDEF	seq_num			gtmsource_save_read_jnl_seqno;
GBLDEF	struct timeval		gtmsource_poll_wait, gtmsource_poll_immediate;
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
GBLDEF	seq_num			gtmsource_last_flush_reg_seq;

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
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	boolean_t		null_subs_xform;
GBLREF	boolean_t 		primary_side_std_null_coll;
GBLREF	boolean_t 		secondary_side_std_null_coll;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;
GBLREF	qw_num			trans_sent_cnt, last_log_tr_sent_cnt;

/* The work-horse of the Source Server */
int gtmsource_process(void)
{
	gtmsource_local_ptr_t		gtmsource_local;
	jnlpool_ctl_ptr_t		jctl;
	seq_num				recvd_seqno, sav_read_jnl_seqno;
	struct sockaddr_in		secondary_addr;
	seq_num				recvd_jnl_seqno, tmp_read_jnl_seqno;
	int				data_len, srch_status;
	unsigned char			*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int				tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int				torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int				status;					/* needed for REPL_{SEND,RECV}_LOOP */
	int				tot_tr_len;
	struct timeval			poll_time;
	int				recvd_msg_type, recvd_start_flags;
	uchar_ptr_t			in_buff, out_buff, save_filter_buff;
	uint4				in_size, out_size, out_bufsiz, tot_out_size, pre_intlfilter_datalen;
	seq_num				log_seqno, diff_seqno, pre_read_seqno, post_read_seqno, jnl_seqno;
	char				err_string[1024];
	boolean_t			xon_wait_logged, prev_catchup, catchup, force_recv_check, is_badtrans;
	double				time_elapsed;
	seq_num				resync_seqno, zqgblmod_seqno, filter_seqno;
	gd_region			*reg, *region_top, *gtmsource_upd_reg, *old_upd_reg;
	sgmnt_addrs			*csa;
	qw_num				backlog_bytes, backlog_count, delta_sent_cnt, delta_data_sent, delta_msg_sent;
	long				prev_msg_sent = 0;
	time_t				prev_now = 0, save_now;
	int				index;
	struct timeval			poll_wait, poll_immediate;
	uint4				temp_ulong;
	unix_db_info			*udi;
	repl_triple			remote_triple, local_triple;
	int4				remote_triple_num, local_triple_num, num_triples;
	seq_num				local_jnl_seqno, dualsite_resync_seqno;
	repl_msg_t			xoff_ack, instnohist_msg, losttncomplete_msg;
	repl_msg_ptr_t			send_msgp;
	repl_start_reply_msg_ptr_t	reply_msgp;
	boolean_t			rollback_first, secondary_ahead, secondary_was_rootprimary, secondary_is_dualsite;
	int				semval;

	error_def(ERR_JNLNEWREC);
	error_def(ERR_JNLSETDATA2LONG);
	error_def(ERR_REPLCOMM);
	error_def(ERR_REPLFTOKSEM);
	error_def(ERR_REPLGBL2LONG);
	error_def(ERR_REPLINSTNOHIST);
	error_def(ERR_REPLRECFMT);
	error_def(ERR_REPLUPGRADESEC);
	error_def(ERR_TEXT);

	assert(REPL_MSG_HDRLEN == sizeof(jnldata_hdr_struct)); /* necessary for reading multiple transactions from jnlpool in
								* a single attempt */
	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	gtmsource_msgp = NULL;
	gtmsource_msgbufsiz = MAX_REPL_MSGLEN;

	assert(GTMSOURCE_POLL_WAIT < MAX_GTMSOURCE_POLL_WAIT);
	gtmsource_poll_wait.tv_sec = 0;
	gtmsource_poll_wait.tv_usec = GTMSOURCE_POLL_WAIT;
	poll_wait = gtmsource_poll_wait;

	gtmsource_poll_immediate.tv_sec = 0;
	gtmsource_poll_immediate.tv_usec = 0;
	poll_immediate = gtmsource_poll_immediate;

	gtmsource_init_sec_addr(&secondary_addr);
	gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;

	/* Below is a simplistic representation of the state diagram of a source server.
	 *
	 *      ------------------------------
	 *            GTMSOURCE_START
	 *      ------------------------------
	 *                    |
	 *                    | (startup state)
	 *                    v
	 *      ------------------------------
	 *     GTMSOURCE_WAITING_FOR_CONNECTION
	 *      ------------------------------
	 *                    |
	 *                    | (gtmsource_est_conn)
	 *                    v
	 *      ------------------------------
	 *       GTMSOURCE_WAITING_FOR_RESTART
	 *      ------------------------------
	 *                    |
	 *                    | (gtmsource_recv_restart)
	 *                    v
	 *      ------------------------------
	 *     GTMSOURCE_SEARCHING_FOR_RESTART
	 *      ------------------------------
	 *                    |
	 *                    | (gtmsource_srch_restart)
	 *                    v
	 *      ------------------------------
	 *        GTMSOURCE_SENDING_JNLRECS         <---------\
	 *      ------------------------------                |
	 *                    |                               |
	 *                    | (receive REPL_XOFF)           |
	 *                    | (receive REPL_XOFF_ACK_ME)    |
	 *                    v                               |
	 *            ------------------------------          ^
	 *              GTMSOURCE_WAITING_FOR_XON             |
	 *            ------------------------------          |
	 *                    |                               |
	 *                    v (receive REPL_XON)            |
	 *                    |                               |
	 *                    \--------------------->---------/
	 */
	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	while (TRUE)
	{
		assert(!udi->grabbed_ftok_sem);
		gtmsource_stop_heartbeat();
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
		{
			gtmsource_est_conn(&secondary_addr);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			repl_source_data_sent = repl_source_msg_sent = 0;
			repl_source_lastlog_data_sent = 0;
			repl_source_lastlog_msg_sent = 0;

			gtmsource_alloc_msgbuff(MAX_REPL_MSGLEN);
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_RESTART;
			recvd_start_flags = START_FLAG_NONE;
			repl_source_prev_log_time = time(NULL);
		}
		if (GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state &&
		    SS_NORMAL != (status = gtmsource_recv_restart(&recvd_seqno, &recvd_msg_type, &recvd_start_flags)))
		{
			if (EREPL_RECV == repl_errno)
			{
				if (REPL_CONN_RESET(status) || ETIMEDOUT == status)
				{	/* Connection reset */
					repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset while receiving restart SEQNO\n");
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				} else
				{
					SNPRINTF(err_string, sizeof(err_string),
							"Error receiving RESTART SEQNO. Error in recv : %s", STRERROR(status));
					rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
				}
			} else if (EREPL_SEND == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE,
					       "Connection reset while sending XOFF_ACK due to possible update process shutdown\n");
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				}
				SNPRINTF(err_string, sizeof(err_string), "Error sending XOFF_ACK_ME message. Error in send : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
			} else if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(err_string, sizeof(err_string), "Error receiving RESTART SEQNO/sending XOFF_ACK_ME.  "
						"Error in select : %s", STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
			}
		}
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			return (SS_NORMAL);
		/* Connection might have been closed if "gtmsource_recv_restart" got an unexpected message. In that case
		 * re-establish the same by continuing to the beginning of this loop. */
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
			continue;
		assert(REPL_PROTO_VER_UNINITIALIZED < gtmsource_local->remote_proto_ver);
		secondary_is_dualsite = (REPL_PROTO_VER_DUALSITE == gtmsource_local->remote_proto_ver) ? 1 : 0;
		if (jctl->upd_disabled)
		{
			if (secondary_is_dualsite)
			{	/* This instance is a Propagating Primary and is trying to connect to a dual-site tertiary.
				 * Do not allow such a connection.
				 */
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Connecting to a dual-site secondary is not allowed "
					"when the current instance [%s] is a propagating primary instance.\n",
					jnlpool.repl_inst_filehdr->this_instname);
				gtm_putmsg(VARLSTCNT(4) ERR_REPLUPGRADESEC, 2,
					LEN_AND_STR((char *)gtmsource_local->secondary_instname));
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset due to above error\n");
				repl_close(&gtmsource_sock_fd);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
				gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
				continue;
			}
		}
		assert(gtmsource_state == GTMSOURCE_SEARCHING_FOR_RESTART || gtmsource_state == GTMSOURCE_WAITING_FOR_RESTART);
		rollback_first = FALSE;
		secondary_ahead = FALSE;
		local_jnl_seqno = jctl->jnl_seqno;
		dualsite_resync_seqno = jctl->max_dualsite_resync_seqno;
		/* Take care to set the flush parameter in repl_log calls below to FALSE until at least the first message
		 * gets sent back. This is so the fetchresync rollback on the other side does not timeout before receiving
		 * a response. */
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Current Journal Seqno of the instance is %llu [0x%llx]\n",
			local_jnl_seqno, local_jnl_seqno);
		if (recvd_seqno > local_jnl_seqno)
		{	/* Secondary journal seqno is greater than that of the Primary. We know it is ahead of the primary. */
			secondary_ahead = TRUE;
			repl_log(gtmsource_log_fp, TRUE, FALSE,
				"Secondary instance journal seqno %llu [0x%llx] is greater than Primary "
				"instance journal seqno %llu [0x%llx]\n",
				recvd_seqno, recvd_seqno, local_jnl_seqno, local_jnl_seqno);
			/* If the secondary is dual-site, the secondary has to roll back to EXACTLY "local_jnl_seqno".
			 * But if the secondary is multi-site, the determination of the rollback seqno involves comparing
			 * the triples between the primary and secondary starting down from "local_jnl_seqno-1" (done below).
			 * In either case, the secondary has to roll back to at most "local_jnl_seqno". Reset "recvd_seqno"
			 * to this number given that we have already recorded that the secondary is ahead of the primary.
			 */
			recvd_seqno = local_jnl_seqno;
		}
		if (secondary_is_dualsite)
		{
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Secondary does NOT support multisite functionality.\n");
			if (START_FLAG_UPDATERESYNC & recvd_start_flags)
			{	/* -updateresync was specified in the receiver server. */
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Secondary receiver specified -UPDATERESYNC.\n");
				local_jnl_seqno = recvd_seqno;
			} else
			{
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Secondary receiver did not specify -UPDATERESYNC.\n");
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Using Resync Seqno instead of Journal Seqno\n");
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Current Resync Seqno of the instance is %llu [0x%llx]\n",
					dualsite_resync_seqno, dualsite_resync_seqno);
				local_jnl_seqno = dualsite_resync_seqno;
				if (recvd_seqno > local_jnl_seqno)
				{
					secondary_ahead = TRUE;
					repl_log(gtmsource_log_fp, TRUE, FALSE,
						"Secondary instance resync seqno %llu [0x%llx] is greater than Primary "
						"instance resync seqno %llu [0x%llx]\n",
						recvd_seqno, recvd_seqno, local_jnl_seqno, local_jnl_seqno);
				}
			}
		} else
		{	/* Receiver runs on a version of GT.M that supports multi-site capability */
			/* If gtmsource_state == GTMSOURCE_SEARCHING_FOR_RESTART, we have already communicated with the
			 * receiver and hence checked the instance info so no need to do it again.
			 */
			if (GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state)
			{	/* Get replication instance info */
				DEBUG_ONLY(secondary_was_rootprimary = -1;)
				if (!gtmsource_get_instance_info(&secondary_was_rootprimary))
				{
					if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						return (SS_NORMAL);
					else if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
						continue;
					else
					{	/* Got a REPL_XOFF_ACK_ME from the receiver. Restart the initial handshake */
						assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state);
						continue;
					}
				}
				assert((FALSE == secondary_was_rootprimary) || (TRUE == secondary_was_rootprimary));
			}
			/* Now get the latest triple information from the secondary. There are three exceptions though.
			 * 	1) If we came here because of a BAD_TRANS message from the receiver server.
			 *		In this case, we have already been communicating with the receiver so no need to
			 *		compare the triple information between primary and secondary.
			 *	2) If receiver server was started with -UPDATERESYNC. In this case there is no triple
			 *		history on the secondary to compare.
			 *	3) If receiver server is at seqno 1, there is no history that can exist. We can safely
			 *		start sending journal records from seqno 1.
			 */
			assert(0 != recvd_seqno);
			if ((1 < recvd_seqno) && ((GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state) || !is_badtrans)
				&& (!(START_FLAG_UPDATERESYNC & recvd_start_flags)))
			{
				if (1 < recvd_seqno)
				{	/* Find the triple in the local instance file corresponding to seqno "recvd_seqno-1" */
					if (!gtmsource_get_triple_info(recvd_seqno, &remote_triple, &remote_triple_num))
					{
						if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
							return (SS_NORMAL);
						else if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
							continue;
						else
						{	/* Got a REPL_XOFF_ACK_ME from receiver. Restart the initial handshake */
							assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state);
							continue;
						}
					}
					assert(remote_triple.start_seqno < recvd_seqno);
					repl_inst_ftok_sem_lock();
					assert(recvd_seqno <= local_jnl_seqno);
					assert(recvd_seqno <= jctl->jnl_seqno);
					status = repl_inst_wrapper_triple_find_seqno(recvd_seqno, &local_triple, &local_triple_num);
					repl_inst_ftok_sem_release();
					if (0 != status)
					{	/* Close the connection. The function call above would have issued the error. */
						assert(ERR_REPLINSTNOHIST == status);
						/* Send this error status to the receiver server before closing the connection.
						 * This way the receiver will know to shut down rather than loop back trying to
						 * reconnect. This avoids an infinite loop of connection open and closes
						 * between the source server and receiver server.
						 */
						instnohist_msg.type = REPL_INST_NOHIST;
						instnohist_msg.len = MIN_REPL_MSGLEN;
						memset(&instnohist_msg.msg[0], 0, sizeof(instnohist_msg.msg));
						gtmsource_repl_send((repl_msg_ptr_t)&instnohist_msg, "REPL_INST_NOHIST", MAX_SEQNO);
						repl_log(gtmsource_log_fp, TRUE, TRUE,
						       "Connection reset due to above REPLINSTNOHIST error\n");
						repl_close(&gtmsource_sock_fd);
						SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
						gtmsource_state = gtmsource_local->gtmsource_state
							= GTMSOURCE_WAITING_FOR_CONNECTION;
						continue;
					}
					/* Check if primary and secondary have same triple information for "recvd_seqno-1" */
					rollback_first = !gtmsource_is_triple_identical(&remote_triple, &local_triple, recvd_seqno);
				}
			}
		}
		QWASSIGN(sav_read_jnl_seqno, gtmsource_local->read_jnl_seqno);
		reply_msgp = (repl_start_reply_msg_ptr_t)gtmsource_msgp;
		memset(reply_msgp, 0, sizeof(*reply_msgp)); /* to identify older releases in the future */
		reply_msgp->len = MIN_REPL_MSGLEN;
		reply_msgp->proto_ver = REPL_PROTO_VER_THIS;
		reply_msgp->node_endianness = NODE_ENDIANNESS;
		assert((1 != recvd_seqno) || !rollback_first);
		if (GTMSOURCE_SEARCHING_FOR_RESTART == gtmsource_state || REPL_START_JNL_SEQNO == recvd_msg_type)
		{
			rollback_first = (rollback_first || secondary_ahead);
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SEARCHING_FOR_RESTART;
			if (!rollback_first)
			{
				resync_seqno = recvd_seqno;
				srch_status = gtmsource_srch_restart(resync_seqno, recvd_start_flags);
				assert(resync_seqno == gtmsource_local->read_jnl_seqno);
				assert(SS_NORMAL == srch_status);
			} else
			{	/* If the last triples in both instances are NOT the same ("rollback_first" is TRUE)
				 * (possible only if the secondary is multi-site), or if secondary is ahead of the primary
				 * ("secondary_ahead" is TRUE) (possible whether secondary is dual-site or multi-site) we
				 * do want the secondary to rollback first. Issue message to do rollback fetchresync.
				 */
				repl_log(gtmsource_log_fp, TRUE, FALSE,
					"Secondary instance needs to first do MUPIP JOURNAL ROLLBACK FETCHRESYNC\n");
				resync_seqno = local_jnl_seqno;
			}
			QWASSIGN(*(seq_num *)&reply_msgp->start_seqno[0], resync_seqno);
			if (!rollback_first)
			{
				reply_msgp->type = REPL_WILL_RESTART_WITH_INFO;
				reply_msgp->jnl_ver = jnl_ver;
				temp_ulong = (0 == primary_side_std_null_coll) ?  START_FLAG_NONE : START_FLAG_COLL_M;
				PUT_ULONG(reply_msgp->start_flags, temp_ulong);
				recvd_start_flags = START_FLAG_NONE;
				gtmsource_repl_send((repl_msg_ptr_t)reply_msgp, "REPL_WILL_RESTART_WITH_INFO", resync_seqno);
			} else
			{	/* Secondary needs to first do FETCHRESYNC rollback to synchronize with primary */
				reply_msgp->type = REPL_ROLLBACK_FIRST;
				gtmsource_repl_send((repl_msg_ptr_t)reply_msgp, "REPL_ROLLBACK_FIRST", resync_seqno);
			}
		} else
		{	/* REPL_FETCH_RESYNC received and state is WAITING_FOR_RESTART */
			if (rollback_first || secondary_ahead)
			{	/* Primary and Secondary are currently not in sync */
				if (!rollback_first)
				{	/* In the case of a multisite secondary, we know the secondary is ahead of the primary
					 * in terms of journal seqno but the last triples are identical. This means that the
					 * secondary is in sync with the primary until the primary's journal seqno
					 * ("local_jnl_seqno") which should be the new resync seqno. In the case of a dualsite
					 * secondary, we know that the secondary's jnl seqno is ahead of the primary's resync
					 * seqno which should be the seqno for the secondary to rollback to. In this case,
					 * "local_jnl_seqno" would have been reset already to "jctl->max_dualsite_resync_seqno"
					 * above so we can use that.
					 */
					resync_seqno = local_jnl_seqno;
				} else
				{	/* Determine the resync seqno between this primary and secondary by comparing
					 * local and remote triples from the tail of the instance file until we reach
					 * one seqno whose triple information is identical in both.
					 */
					assert(!secondary_is_dualsite);
					assert(1 != recvd_seqno);
					resync_seqno = gtmsource_find_resync_seqno(&local_triple, local_triple_num,
											&remote_triple, remote_triple_num);
					assert((MAX_SEQNO != resync_seqno) || (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						|| (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state));
				}
			} else
			{	/* Primary and Secondary are in sync upto "recvd_seqno". Send it back as the new resync seqno. */
				resync_seqno = recvd_seqno;
			}
			if (MAX_SEQNO != resync_seqno)
			{
				assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state && REPL_FETCH_RESYNC == recvd_msg_type);
				reply_msgp->type = REPL_RESYNC_SEQNO;
				QWASSIGN(*(seq_num *)&reply_msgp->start_seqno[0], resync_seqno);
				gtmsource_repl_send((repl_msg_ptr_t)reply_msgp, "REPL_RESYNC_SEQNO", resync_seqno);
			}
		}
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			return (SS_NORMAL);	/* "gtmsource_repl_send" or "gtmsource_find_resync_seqno" did not complete */
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
			continue;	/* "gtmsource_repl_send" or "gtmsource_find_resync_seqno" did not complete */
		assert(MAX_SEQNO != resync_seqno);
		/* Now that the initial communication with the secondary has happened, do additional checks if the secondary
		 * is found to be dualsite. If so, update fields in the journal pool to reflect that. Get the appropriate
		 * lock before updating those fields.
		 */
		if (secondary_is_dualsite)
		{	/* Successfully connected to a dual-site secondary. Check that there are no other source
			 * servers running. If yes, issue REPLUPGRADESEC error.
			 */
			repl_inst_ftok_sem_lock();
			semval = get_sem_info(SOURCE, SRC_SERV_COUNT_SEM, SEM_INFO_VAL);
			if (-1 == semval)
			{
				repl_inst_ftok_sem_release();
				repl_log(gtmsource_log_fp, TRUE, FALSE,
					"Error fetching source server count semaphore value : %s\n", REPL_SEM_ERROR);
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset due to above error\n");
				repl_close(&gtmsource_sock_fd);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
				gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
				continue;
			} else if (1 < semval)
			{	/* There are source servers running on this instance other than us. Cannot connect to a
				 * dual-site secondary in this case as well.
				 */
				repl_inst_ftok_sem_release();
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Multiple source servers cannot be running while "
					"connecting to dual-site secondary.\n");
				gtm_putmsg(VARLSTCNT(4) ERR_REPLUPGRADESEC, 2,
					LEN_AND_STR((char *)gtmsource_local->secondary_instname));
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset due to above error\n");
				repl_close(&gtmsource_sock_fd);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
				gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
				continue;
			}
			/* At this point, we are the only source server running (guaranteed by the ftok lock we are holding)
			 * and are connected to a dual-site secondary. Update the "jctl" structure to reflect this.
			 */
			jctl->secondary_is_dualsite = TRUE;
			repl_inst_ftok_sem_release();
		}
		/* After having established connection, initialize a few fields in the gtmsource_local
		 * structure and flush those changes to the instance file on disk.
		 */
		grab_lock(jnlpool.jnlpool_dummy_reg);
		gtmsource_local->connect_jnl_seqno = jctl->jnl_seqno;
		gtmsource_local->send_losttn_complete = jctl->send_losttn_complete;
		rel_lock(jnlpool.jnlpool_dummy_reg);
		/* Now that "connect_jnl_seqno" has been updated, flush it to corresponding gtmsrc_lcl on disk */
		repl_inst_ftok_sem_lock();
		repl_inst_flush_gtmsrc_lcl();	/* this requires the ftok semaphore to be held */
		repl_inst_ftok_sem_release();
		if (REPL_WILL_RESTART_WITH_INFO != reply_msgp->type)
		{
			assert(reply_msgp->type == REPL_RESYNC_SEQNO || reply_msgp->type == REPL_ROLLBACK_FIRST);
			if ((REPL_RESYNC_SEQNO == reply_msgp->type) && (secondary_is_dualsite || secondary_was_rootprimary))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Sent REPL_RESYNC_SEQNO message with SEQNO %llu\n",
					 (*(seq_num *)&reply_msgp->start_seqno[0]));
				region_top = gd_header->regions + gd_header->n_regions;
				assert(NULL != jnlpool.jnlpool_dummy_reg);
				repl_inst_ftok_sem_lock();
				zqgblmod_seqno = jctl->max_zqgblmod_seqno;
				if (0 == zqgblmod_seqno)
				{	/* If zqgblmod_seqno in all file headers is 0, it implies that this is the first
					 * FETCHRESYNC rollback after the most recent MUPIP REPLIC -LOSTTNCOMPLETE command.
					 * Therefore reset zqgblmod_seqno to the rollback seqno. If not the first rollback,
					 * then zqgblmod_seqno will be reset only if the new rollback seqno is lesser
					 * than the current value.
					 */
					zqgblmod_seqno = MAX_SEQNO;	/* actually 0xFFFFFFFFFFFFFFFF (max possible seqno) */
					/* Reset any pending MUPIP REPLIC -SOURCE -LOSTTNCOMPLETE */
					grab_lock(jnlpool.jnlpool_dummy_reg);
					jctl->send_losttn_complete = FALSE;
					gtmsource_local->send_losttn_complete = jctl->send_losttn_complete;
					rel_lock(jnlpool.jnlpool_dummy_reg);
				}
				REPL_DPRINT2("BEFORE FINDING RESYNC - zqgblmod_seqno is %llu", zqgblmod_seqno);
				REPL_DPRINT2(", curr_seqno is %llu\n", jctl->jnl_seqno);
				if (zqgblmod_seqno > resync_seqno)
				{	/* reset "zqgblmod_seqno" and "zqgblmod_tn" in all fileheaders to "resync_seqno" */
					gtmsource_update_zqgblmod_seqno_and_tn(resync_seqno);
				}
				repl_inst_ftok_sem_release();
			}
			/* Could send a REPL_CLOSE_CONN message here */
			/* It is expected that on receiving this msg, the Receiver Server will break the connection and exit. */
		 	repl_close(&gtmsource_sock_fd);
		 	LONG_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT); /* may not be needed after REPL_CLOSE_CONN is sent */
		 	gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
		 	continue;
		}
		if (QWLT(gtmsource_local->read_jnl_seqno, sav_read_jnl_seqno) && (NULL != repl_ctl_list))
		{	/* The journal files may have been positioned ahead of the read_jnl_seqno for the next read.
			 * Indicate that they have to be repositioned into the past.
			 */
			assert(READ_FILE == gtmsource_local->read_state);
			gtmsource_set_lookback();
		}
		poll_time = poll_immediate;
		gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
		assert(1 <= gtmsource_local->read_jnl_seqno);
		/* Now that "gtmsource_local->read_jnl_seqno" is initialized, ensure the first send gets logged. */
		gtmsource_reinit_logseqno();
		/* Before setting "next_triple_seqno", check if we have at least one triple in the replication instance file.
		 * The only case when there can be no triples is if this instance is a propagating primary. Assert that.
		 * In this case, wait for this instance's primary to send the first triple before setting the next_triple_seqno.
		 * Note that we are fetching the value of "num_triples" without holding a lock on the instance file but that is
		 * ok since all we care about is if it is 0 or not. We do not rely on the actual value until we get the ftok lock.
		 */
		num_triples = jnlpool.repl_inst_filehdr->num_triples;
		assert(0 <= num_triples);
		assert(num_triples || jctl->upd_disabled);
		gtmsource_local->next_triple_num = -1;	/* Initial value. Reset by the call to "gtmsource_set_next_triple_seqno"
							 * invoked in turn by "gtmsource_send_new_triple" down below */
		if (jctl->upd_disabled && !num_triples)
		{	/* Wait for corresponding primary to send a new triple and the receiver server on this instance
			 * to write that to the replication instance file.
			 */
			assert(-1 == gtmsource_local->next_triple_num);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server waiting for first triple to be written by "
				"update process\n");
			do
			{
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_FIRSTTRIPLE);
				gtmsource_poll_actions(FALSE);
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
				num_triples = jnlpool.repl_inst_filehdr->num_triples;
				if (num_triples)	/* Number of triples is non-zero */
					break;
			} while (TRUE);
			repl_log(gtmsource_log_fp, TRUE, TRUE,
				"First triple written by update process. Source server proceeding.\n");
		}
		gtmsource_init_heartbeat();
		if (jnl_ver > remote_jnl_ver && IF_NONE != repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
									       [remote_jnl_ver - JNL_VER_EARLIEST_REPL])
		{
			assert(IF_INVALID != repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
								 [remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
			assert(IF_INVALID != repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
								 [jnl_ver - JNL_VER_EARLIEST_REPL]);
			/* reverse transformation should exist */
			assert(IF_NONE != repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
							      [jnl_ver - JNL_VER_EARLIEST_REPL]);
			if (FALSE != (null_subs_xform = (primary_side_std_null_coll   && !secondary_side_std_null_coll ||
					secondary_side_std_null_coll && !primary_side_std_null_coll)))
				null_subs_xform = (primary_side_std_null_coll ?
							STDNULL_TO_GTMNULL_COLL : GTMNULL_TO_STDNULL_COLL);
			/* note that if jnl_ver == remote_jnl_ver and jnl_ver > V15_JNL_VER, the two sides may be running
			 * different null collation. However, we leave the overhead of null collation transformation to
			 * the receiver as source server is generally more loaded than the receiver.
			 */
			gtmsource_filter |= INTERNAL_FILTER;
			gtmsource_alloc_filter_buff(gtmsource_msgbufsiz);
		} else
		{
			gtmsource_filter &= ~INTERNAL_FILTER;
			if (NO_FILTER == gtmsource_filter)
				gtmsource_free_filter_buff();
		}
		catchup = FALSE;
		force_recv_check = TRUE;
		xon_wait_logged = FALSE;
		if (secondary_is_dualsite)
		{
			gtmsource_upd_reg = gtmsource_mru_reg = NULL;
			for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions;
				reg < region_top; reg++)
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
		}
		/* Flush "gtmsource_local->read_jnl_seqno" and db file header "dualsite_resync_seqnos" to disk right now.
		 * This will serve as a reference point for next timed flush to occur.
		 */
		gtmsource_flush_fh(gtmsource_local->read_jnl_seqno);
		if (secondary_is_dualsite)
			gtmsource_last_flush_reg_seq = jctl->jnl_seqno;
		gtmsource_local->send_new_triple = TRUE;	/* Send new triple unconditionally at start of connection */
		gtmsource_local->next_triple_seqno = MAX_SEQNO;	/* Initial value. Reset by "gtmsource_send_new_triple" below */
		while (TRUE)
		{
			gtmsource_poll_actions(TRUE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;
			if (gtmsource_local->send_losttn_complete)
			{	/* Send LOSTTNCOMPLETE across to the secondary and reset flag to FALSE */
				losttncomplete_msg.type = REPL_LOSTTNCOMPLETE;
				losttncomplete_msg.len = MIN_REPL_MSGLEN;
				gtmsource_repl_send((repl_msg_ptr_t)&losttncomplete_msg, "REPL_LOSTTNCOMPLETE", MAX_SEQNO);
				grab_lock(jnlpool.jnlpool_dummy_reg);
				gtmsource_local->send_losttn_complete = FALSE;
				rel_lock(jnlpool.jnlpool_dummy_reg);
			}
			if (gtmsource_local->send_new_triple)
			{	/* We are at the beginning of a new triple boundary. Send a REPL_NEW_TRIPLE message
				 * before sending journal records for seqnos corresponding to this triple.
				 */
				if (REPL_PROTO_VER_MULTISITE <= gtmsource_local->remote_proto_ver)
				{	/* Remote version supports multi-site functionality. Send REPL_NEW_TRIPLE and friends */
					gtmsource_send_new_triple();
					if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						return (SS_NORMAL);
					if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
						break;
					assert(FALSE == gtmsource_local->send_new_triple);
				} else
					gtmsource_local->send_new_triple = FALSE;	/* Simulate a false new triple send */
			}
			/* If the backlog is high, we want to avoid communication overhead as much as possible. We switch
			 * our communication mode to *catchup* mode, wherein we don't wait for the pipe to become ready to
			 * send. Rather, we assume the pipe is ready for sending. The risk is that the send may block if
			 * the pipe is not ready for sending. In the user's perspective, the risk is that the source server
			 * may not respond to administrative actions such as "change log", "shutdown" (although mupip stop
			 * would work).
			 */
			pre_read_seqno = gtmsource_local->read_jnl_seqno;
			prev_catchup = catchup;
			assert(jctl->write_addr >= gtmsource_local->read_addr);
			backlog_bytes = jctl->write_addr - gtmsource_local->read_addr;
			jnl_seqno = jctl->jnl_seqno;
			assert(jnl_seqno >= pre_read_seqno - 1); /* jnl_seqno >= pre_read_seqno is the most common case;
								      * see gtmsource_readpool() for when the rare case can occur */
			backlog_count = (jnl_seqno >= pre_read_seqno) ? (jnl_seqno - pre_read_seqno) : 0;
			catchup = (BACKLOG_BYTES_THRESHOLD <= backlog_bytes || BACKLOG_COUNT_THRESHOLD <= backlog_count);
			if (!prev_catchup && catchup) /* transition from non catchup to catchup */
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server entering catchup mode at Seqno : %llu "
					 "Current backlog : %llu Backlog size in journal pool : %llu bytes\n", pre_read_seqno,
					 backlog_count, backlog_bytes);
				prev_now = gtmsource_now;
				prev_msg_sent = repl_source_msg_sent;
				force_recv_check = TRUE;
			} else if (prev_catchup && !catchup) /* transition from catchup to non catchup */
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server returning to regular mode from catchup mode "
					 "at Seqno : %llu Current backlog : %llu Backlog size in journal pool : %llu bytes\n",
					 pre_read_seqno, backlog_count, backlog_bytes);
				if (gtmsource_msgbufsiz - MAX_REPL_MSGLEN > 2 * OS_PAGE_SIZE)
				{/* We have expanded the buffer by too much (could have been avoided had we sent one transaction
				  * at a time while reading from journal files); let's revert back to our initial buffer size.
				  * If we don't reduce our buffer, it is possible that the buffer keeps growing (while reading
				  * from journal file) thus making the size of sends while reading from journal pool very
				  * large (> 1 MB). Better to do some house keeping. We will force an expansion if the transaction
				  * size dictates it. Ideally, this must be done while switching reading back from files to
				  * pool, but we can't afford to free the buffer until we sent the transaction out. That apart,
				  * let's wait for some breathing time to do house keeping. In catchup mode, we intend to keep
				  * the send size large. */
					gtmsource_free_filter_buff();
					gtmsource_free_msgbuff();
					gtmsource_alloc_msgbuff(MAX_REPL_MSGLEN); /* will also allocate filter buffer */
				}
			}
			/* Check if receiver sent us any control message.
			 * Typically, the traffic from receiver to source is very low compared to traffic in the other direction.
			 * More often than not, there will be nothing on the pipe to receive. Ideally, we should let TCP
			 * notify us when there is data on the pipe (async I/O on Unix and VMS). We are not there yet. Until then,
			 * we use heuristics - attempt receive every GTMSOURCE_SENT_THRESHOLD_FOR_RECV bytes of sent data, and
			 * every heartbeat period, OR whenever we want to force a check
			 */
			if ((GTMSOURCE_SENDING_JNLRECS != gtmsource_state) || !catchup || prev_now != (save_now = gtmsource_now)
				|| (GTMSOURCE_SENT_THRESHOLD_FOR_RECV <= (repl_source_msg_sent - prev_msg_sent))
				|| force_recv_check)
			{
				REPL_EXTRA_DPRINT2("gtmsource_process: receiving because : %s\n",
						   (GTMSOURCE_SENDING_JNLRECS != gtmsource_state) ? "state is not SENDING_JNLRECS" :
						   !catchup ? "not in catchup mode" :
						   (prev_now != save_now) ? "heartbeat interval passed" :
						   (GTMSOURCE_SENT_THRESHOLD_FOR_RECV <= repl_source_msg_sent - prev_msg_sent) ?
						   	"sent bytes threshold for recv crossed" :
							"force recv check");
				REPL_EXTRA_DPRINT6("gtmsource_state : %d  prev_now : %ld gtmsource_now : %ld repl_source_msg_sent "
						   ": %ld prev_msg_sent : %ld\n", gtmsource_state, prev_now, save_now,
						   repl_source_msg_sent, prev_msg_sent);
				REPL_RECV_LOOP(gtmsource_sock_fd, gtmsource_msgp, MIN_REPL_MSGLEN, FALSE, &poll_time)
				{
					if (0 == recvd_len) /* nothing received in the first attempt, let's try again later */
						break;
					gtmsource_poll_actions(TRUE);
					if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						return (SS_NORMAL);
					if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
						break;
				}
				REPL_EXTRA_DPRINT3("gtmsource_process: %d received, type is %d\n", recvd_len,
							(0 != recvd_len) ? gtmsource_msgp->type : -1);
				if (GTMSOURCE_SENDING_JNLRECS == gtmsource_state && catchup)
				{
					if (prev_now != save_now)
						prev_now = save_now;
					else if (GTMSOURCE_SENT_THRESHOLD_FOR_RECV <= repl_source_msg_sent - prev_msg_sent)
					{ /* do not set to repl_source_msg_sent; increment by GTMSOURCE_SENT_THRESHOLD_FOR_RECV
					   * instead so that we force recv every GTMSOURCE_SENT_THRESHOLD_FOR_RECV bytes */
						prev_msg_sent += GTMSOURCE_SENT_THRESHOLD_FOR_RECV;
					}
				}
			} else
			{	/* behave as if there was nothing to be read */
				status = SS_NORMAL;
				recvd_len = 0;
			}
			force_recv_check = FALSE;
			if (SS_NORMAL == status && 0 != recvd_len)
			{	/* Process the received control message */
				switch(gtmsource_msgp->type)
				{
					case REPL_XOFF:
					case REPL_XOFF_ACK_ME:
						gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_XON;
						poll_time = poll_wait;
						repl_log(gtmsource_log_fp, TRUE, TRUE,
							 "REPL_XOFF/REPL_XOFF_ACK_ME received. Send stalled...\n");
						xon_wait_logged = FALSE;
						if (REPL_XOFF_ACK_ME == gtmsource_msgp->type)
						{
							xoff_ack.type = REPL_XOFF_ACK;
							QWASSIGN(*(seq_num *)&xoff_ack.msg[0], *(seq_num *)&gtmsource_msgp->msg[0]);
							xoff_ack.len = MIN_REPL_MSGLEN;
							gtmsource_repl_send((repl_msg_ptr_t)&xoff_ack, "REPL_XOFF_ACK", MAX_SEQNO);
							if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
								return (SS_NORMAL);	/* "gtmsource_repl_send" did not complete */
							if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
								break;	/* "gtmsource_repl_send" did not complete */
						}
						break;
					case REPL_XON:
						gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
						poll_time = poll_immediate;
						repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_XON received\n");
						gtmsource_restart_heartbeat; /*Macro*/
						REPL_DPRINT1("Restarting HEARTBEAT\n");
						xon_wait_logged = FALSE;
						/* In catchup mode, we do not receive as often as we would have in non catchup mode.
						 * The consequence of this may be that we do not react to XOFF quickly enough,
						 * making it worse for the replication pipe. We may end up with multiple XOFFs (with
						 * intervening XONs) sitting on the replication pipe that are yet to be received
						 * and processed by the source server. To avoid such a situation, on receipt of
						 * an XON, we immediately force a check on the incoming pipe thereby draining
						 * all pending XOFF/XONs, keeping the pipe smooth. Also, there is less likelihood
						 * of missing a HEARTBEAT response (that is perhaps on the pipe) which may lead to
						 * connection breakage although the pipe is alive and well.
						 *
						 * We will force a check regardless of mode (catchup or non catchup) as we may
						 * be pounding the secondary even in non catchup mode
						 */
						force_recv_check = TRUE;
						break;
					case REPL_BADTRANS:
					case REPL_START_JNL_SEQNO:
						QWASSIGN(recvd_seqno, *(seq_num *)&gtmsource_msgp->msg[0]);
						gtmsource_state = gtmsource_local->gtmsource_state
							= GTMSOURCE_SEARCHING_FOR_RESTART;
						if (REPL_BADTRANS == gtmsource_msgp->type)
						{
							is_badtrans = TRUE;
							recvd_start_flags = START_FLAG_NONE;
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Received REPL_BADTRANS message with SEQNO %llu\n", recvd_seqno);
						} else
						{
							is_badtrans = FALSE;
							recvd_start_flags = ((repl_start_msg_ptr_t)gtmsource_msgp)->start_flags;
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								 "Received REPL_START_JNL_SEQNO message with SEQNO %llu. Possible "
								 "crash of recvr/update process\n", recvd_seqno);
						}
						break;
					case REPL_HEARTBEAT:
						gtmsource_process_heartbeat((repl_heartbeat_msg_ptr_t)gtmsource_msgp);
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
						break;
				}
			} else if (SS_NORMAL != status)
			{
				if (EREPL_RECV == repl_errno)
				{
					if (REPL_CONN_RESET(status) || ETIMEDOUT == status)
					{
						/* Connection reset */
						repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Connection reset while attempting to receive from secondary\n");
						repl_close(&gtmsource_sock_fd);
						SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
						gtmsource_state = gtmsource_local->gtmsource_state
							= GTMSOURCE_WAITING_FOR_CONNECTION;
						break;
					} else
					{
						SNPRINTF(err_string, sizeof(err_string),
								"Error receiving Control message from Receiver. Error in recv : %s",
								STRERROR(status));
						rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
					}
				} else if (EREPL_SELECT == repl_errno)
				{
					SNPRINTF(err_string, sizeof(err_string),
							"Error receiving Control message from Receiver. Error in select : %s",
							STRERROR(status));
					rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
				}
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
				if (GTMSOURCE_FH_FLUSH_INTERVAL <= difftime(gtmsource_now, gtmsource_last_flush_time))
					gtmsource_flush_fh(gtmsource_local->read_jnl_seqno);
				continue;
			}
			if (GTMSOURCE_SEARCHING_FOR_RESTART  == gtmsource_state ||
				GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
			{
				xon_wait_logged = FALSE;
				break;
			}
			assert(gtmsource_state == GTMSOURCE_SENDING_JNLRECS);
			if (force_recv_check) /* we want to poll the incoming pipe for possible XOFF */
				continue;
			tot_tr_len = gtmsource_get_jnlrecs(&gtmsource_msgp->msg[0], &data_len,
							   gtmsource_msgbufsiz - REPL_MSG_HDRLEN, NO_FILTER == gtmsource_filter);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;
			if (GTMSOURCE_SEND_NEW_TRIPLE == gtmsource_state)
			{	/* This is a signal from "gtmsource_get_jnlrecs" that a REPL_NEW_TRIPLE message has to be sent
				 * first before sending any more seqnos across. Set "gtmsource_local->send_new_triple" to TRUE.
				 */
				assert(0 == tot_tr_len);
				gtmsource_local->send_new_triple = TRUE;	/* Will cause a new triple to be sent first */
				gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
				continue;	/* Send a REPL_NEW_TRIPLE message first and then send journal records */
			}
			post_read_seqno = gtmsource_local->read_jnl_seqno;
			if (0 <= tot_tr_len)
			{
				if (0 < data_len)
				{
					send_msgp = gtmsource_msgp;
					if (gtmsource_filter & EXTERNAL_FILTER)
					{
						assert(tot_tr_len == data_len + REPL_MSG_HDRLEN); /* only ONE transaction read */
						QWSUBDW(filter_seqno, post_read_seqno, 1);
						if (SS_NORMAL != (status = repl_filter(filter_seqno, gtmsource_msgp->msg, &data_len,
									     	       gtmsource_msgbufsiz)))
							repl_filter_error(filter_seqno, status);
						tot_tr_len = data_len + REPL_MSG_HDRLEN;
					}
					if (gtmsource_filter & INTERNAL_FILTER)
					{
						assert(tot_tr_len == data_len + REPL_MSG_HDRLEN); /* only ONE transaction read */
						pre_intlfilter_datalen = data_len;
						in_buff = gtmsource_msgp->msg;
						in_size = pre_intlfilter_datalen;
						out_buff = repl_filter_buff + REPL_MSG_HDRLEN;
						out_bufsiz = repl_filter_bufsiz - REPL_MSG_HDRLEN;
						tot_out_size = 0;
					     	while ((status =
							repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
									    [remote_jnl_ver - JNL_VER_EARLIEST_REPL](
								in_buff, &in_size, out_buff, &out_size, out_bufsiz)) != SS_NORMAL &&
						       EREPL_INTLFILTER_NOSPC == repl_errno)
						{
							save_filter_buff = repl_filter_buff;
							gtmsource_alloc_filter_buff(repl_filter_bufsiz + (repl_filter_bufsiz >> 1));
							in_buff += in_size;
							in_size = (uint4)(pre_intlfilter_datalen - (in_buff - gtmsource_msgp->msg));
							out_bufsiz = (uint4)(repl_filter_bufsiz -
									     (out_buff - save_filter_buff) - out_size);
							out_buff = repl_filter_buff + (out_buff - save_filter_buff) + out_size;
							tot_out_size += out_size;
						}
						if (SS_NORMAL == status)
						{
							data_len = tot_out_size + out_size;
							tot_tr_len = data_len + REPL_MSG_HDRLEN;
							send_msgp = (repl_msg_ptr_t)repl_filter_buff;
						} else
						{
							if (EREPL_INTLFILTER_BADREC == repl_errno)
								rts_error(VARLSTCNT(1) ERR_REPLRECFMT);
							else if (EREPL_INTLFILTER_DATA2LONG == repl_errno)
								rts_error(VARLSTCNT(4) ERR_JNLSETDATA2LONG, 2, jnl_source_datalen,
								  	  jnl_dest_maxdatalen);
							else if (EREPL_INTLFILTER_NEWREC == repl_errno)
								rts_error(VARLSTCNT(4) ERR_JNLNEWREC, 2,
									  (unsigned int)jnl_source_rectype,
								  	  (unsigned int)jnl_dest_maxrectype);
							else if (EREPL_INTLFILTER_REPLGBL2LONG == repl_errno)
								rts_error(VARLSTCNT(1) ERR_REPLGBL2LONG);
							else /* (EREPL_INTLFILTER_INCMPLREC == repl_errno) */
								GTMASSERT;
						}
					}
					send_msgp->type = REPL_TR_JNL_RECS;
					send_msgp->len = data_len + REPL_MSG_HDRLEN;
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
					REPL_SEND_LOOP(gtmsource_sock_fd, send_msgp, tot_tr_len, UNIX_ONLY(catchup) VMS_ONLY(TRUE),
							&poll_immediate)
					{
						gtmsource_poll_actions(FALSE);
						if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						{
							gtmsource_flush_fh(post_read_seqno);
							return (SS_NORMAL);
						}
					}
					if (SS_NORMAL != status)
					{
						gtmsource_flush_fh(post_read_seqno);
						if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Connection reset while sending seqno data from "
								"%llu [0x%llx] to %llu [0x%llx]\n", pre_read_seqno,
								pre_read_seqno, post_read_seqno, post_read_seqno);
							repl_close(&gtmsource_sock_fd);
							SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
							gtmsource_state = gtmsource_local->gtmsource_state
								= GTMSOURCE_WAITING_FOR_CONNECTION;
							break;
						}
						if (EREPL_SEND == repl_errno)
						{
							SNPRINTF(err_string, sizeof(err_string),
								"Error sending DATA. Error in send : %s", STRERROR(status));
							rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  RTS_ERROR_STRING(err_string));
						}
						if (EREPL_SELECT == repl_errno)
						{
							SNPRINTF(err_string, sizeof(err_string),
								"Error sending DATA. Error in select : %s", STRERROR(status));
							rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  RTS_ERROR_STRING(err_string));
						}
					}
					jnlpool.gtmsource_local->read_jnl_seqno = post_read_seqno;
					if (secondary_is_dualsite)
					{	/* Record the "last sent seqno" in file header of most recently updated region
						 * and one region picked in round robin order. Updating one region is sufficient
						 * since the system's resync_seqno is computed to be the maximum of file header
						 * resync_seqno across all regions. We choose to update multiple regions to
						 * increase the odds of not losing information in case of a system crash.
						 */
						UPDATE_DUALSITE_RESYNC_SEQNO(gtmsource_mru_reg, pre_read_seqno, post_read_seqno);
						if (gtmsource_mru_reg != gtmsource_upd_reg)
							UPDATE_DUALSITE_RESYNC_SEQNO(gtmsource_upd_reg, pre_read_seqno,
								post_read_seqno);
						old_upd_reg = gtmsource_upd_reg;
						do
						{	/* select next region in round robin order */
							gtmsource_upd_reg++;
							if (gtmsource_upd_reg >= gd_header->regions + gd_header->n_regions)
								gtmsource_upd_reg = gd_header->regions;
									/* wrap back to first region */
						} while (gtmsource_upd_reg != old_upd_reg && /* back to the original region? */
								!REPL_ALLOWED(FILE_INFO(gtmsource_upd_reg)->s_addrs.hdr));
						/* Also update "max_dualsite_resync_seqno" in the journal pool. This needs to
						 * be maintained uptodate since this is what gets checked whenever the source
						 * server receives a "go back" request (REPL_BADTRANS or REPL_START_JNL_SEQNO).
						 * No need to hold a lock on the jnlpool to do this update as we are guaranteed
						 * to be the ONLY source server running (due to secondary being dual-site).
						 */
						jctl->max_dualsite_resync_seqno = post_read_seqno;
					}
					if (GTMSOURCE_FH_FLUSH_INTERVAL <= difftime(gtmsource_now, gtmsource_last_flush_time))
						gtmsource_flush_fh(post_read_seqno);
					repl_source_msg_sent += (qw_num)tot_tr_len;
					repl_source_data_sent += (qw_num)(tot_tr_len) -
								(post_read_seqno - pre_read_seqno) * REPL_MSG_HDRLEN;
					log_seqno = post_read_seqno - 1; /* post_read_seqno is the "next" seqno to be sent,
									      * not the last one we sent */
					if (gtmsource_logstats || (log_seqno - lastlog_seqno >= log_interval));
					{	/* print always when STATSLOG is ON, or when the log interval has passed */
						trans_sent_cnt += (log_seqno - lastlog_seqno);
						/* jctl->jnl_seqno >= post_read_seqno is the most common case;
						 * see gtmsource_readpool() for when the rare case can occur */
						jnl_seqno = jctl->jnl_seqno;
						assert(jnl_seqno >= post_read_seqno - 1);
						diff_seqno = (jnl_seqno >= post_read_seqno) ?
								(jnl_seqno - post_read_seqno) : 0;
						repl_log(gtmsource_log_fp, FALSE, FALSE, "REPL INFO - Tr num : %llu", log_seqno);
						repl_log(gtmsource_log_fp, FALSE, FALSE, "  Tr Total : %llu  Msg Total : %llu  ",
							 repl_source_data_sent, repl_source_msg_sent);
						repl_log(gtmsource_log_fp, FALSE, TRUE, "Current backlog : %llu\n", diff_seqno);
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
						time_elapsed = difftime(repl_source_this_log_time,
										repl_source_prev_log_time);
						if ((double)GTMSOURCE_LOGSTATS_INTERVAL <= time_elapsed)
						{
							delta_sent_cnt = trans_sent_cnt - last_log_tr_sent_cnt;
							delta_data_sent = repl_source_data_sent
										- repl_source_lastlog_data_sent;
							delta_msg_sent = repl_source_msg_sent
										- repl_source_lastlog_msg_sent;
							repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL INFO since last log : "
								"Time elapsed : %00.f  Tr sent : %llu  Tr bytes : %llu  "
								"Msg bytes : %llu\n", time_elapsed,
								delta_sent_cnt, delta_data_sent, delta_msg_sent);
							repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO since last log : "
								"Time elapsed : %00.f  Tr sent/s : %f  Tr bytes/s : %f  "
								"Msg bytes/s : %f\n", time_elapsed,
								(float)delta_sent_cnt / time_elapsed,
								(float)delta_data_sent / time_elapsed,
								(float)delta_msg_sent / time_elapsed);
							repl_source_lastlog_data_sent = repl_source_data_sent;
							repl_source_lastlog_msg_sent = repl_source_msg_sent;
							last_log_tr_sent_cnt = trans_sent_cnt;
							repl_source_prev_log_time = repl_source_this_log_time;
						}
						lastlog_seqno = log_seqno;
					}
					poll_time = poll_immediate;
				} else /* data_len == 0 */
				{	/* nothing to send */
					gtmsource_flush_fh(post_read_seqno);
					poll_time = poll_wait;
					rel_quant(); /* give up processor and let other processes run */
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
