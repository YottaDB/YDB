/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_netdb.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
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
#include "hashtab_mname.h"     /* needed for muprec.h */
#include "hashtab_int4.h"     /* needed for muprec.h */
#include "hashtab_int8.h"     /* needed for muprec.h */
#include "buddy_list.h"
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "gtm_event_log.h"
#include "gt_timer.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "repl_filter.h"
#include "repl_log.h"
#include "sgtm_putmsg.h"
#include "min_max.h"
#include "error.h"

GBLREF	gd_addr			*gd_header;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	int			gtmsource_sock_fd;
GBLREF  seq_num                 gtmsource_save_read_jnl_seqno;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	unsigned char		*gtmsource_tcombuffp;
GBLREF	int			gtmsource_log_fd;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_filter;
GBLREF	seq_num			seq_num_zero;
GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	boolean_t		gtmsource_pool2file_transition;
GBLREF  repl_ctl_element        *repl_ctl_list;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	boolean_t		secondary_side_std_null_coll;
GBLREF	boolean_t		secondary_side_trigger_support;

error_def(ERR_REPLCOMM);
error_def(ERR_REPLWARN);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);

static	unsigned char		*tcombuff, *msgbuff, *filterbuff;

int gtmsource_est_conn()
{
	int			connection_attempts, alert_attempts, save_errno, status;
	char			print_msg[1024], msg_str[1024];
	gtmsource_local_ptr_t	gtmsource_local;
	int			send_buffsize, recv_buffsize, tcp_s_bufsize;
	int 			logging_period, logging_interval; /* logging period = soft_tries_period*logging_interval */
	int 			logging_attempts;
	sockaddr_ptr		secondary_sa;
	int			secondary_addrlen;

	gtmsource_local = jnlpool.gtmsource_local;
	/* Connect to the secondary - use hard tries, soft tries ... */
	connection_attempts = 0;
	gtmsource_comm_init(); /* set up gtmsource_loal.secondary_ai */
	secondary_sa = (sockaddr_ptr)(&gtmsource_local->secondary_inet_addr);
	secondary_addrlen = gtmsource_local->secondary_addrlen;
	repl_log(gtmsource_log_fp, TRUE, TRUE, "Connect hard tries count = %d, Connect hard tries period = %d\n",
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT],
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD]);
	do
	{
		status = gtm_connect(gtmsource_sock_fd, secondary_sa, secondary_addrlen);
		if (0 == status)
			break;
		repl_log(gtmsource_log_fp, FALSE, FALSE, "%d hard connection attempt failed : %s\n", connection_attempts + 1,
			 STRERROR(ERRNO));
		repl_close(&gtmsource_sock_fd);
		if (REPL_MAX_CONN_HARD_TRIES_PERIOD > jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD])
			SHORT_SLEEP(gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD])
		else
			LONG_SLEEP(gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD] % MILLISECS_IN_SEC);
		gtmsource_poll_actions(FALSE);
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			return (SS_NORMAL);
		gtmsource_comm_init();
	} while (++connection_attempts < gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT]);

	gtmsource_poll_actions(FALSE);
	if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
		return (SS_NORMAL);

	if (gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT] <= connection_attempts)
	{	/*Initialize logging period related variables*/
		logging_period = gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD];
		logging_interval = 1;
		logging_attempts = 0;

		alert_attempts = DIVIDE_ROUND_DOWN(gtmsource_local->connect_parms[GTMSOURCE_CONN_ALERT_PERIOD],
						   gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Soft tries period = %d, Alert period = %d\n",
			 gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD],
			 alert_attempts * gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
		connection_attempts = 0;
		do
		{
			status = gtm_connect(gtmsource_sock_fd, secondary_sa, secondary_addrlen);
			if (0 == status)
				break;
			repl_close(&gtmsource_sock_fd);
			if (0 == (connection_attempts + 1) % logging_interval)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "%d soft connection attempt failed : %s\n",
					 connection_attempts + 1, STRERROR(ERRNO));
				logging_attempts++;
			}
			LONG_SLEEP(gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			gtmsource_comm_init();
			connection_attempts++;
			if (0 == (connection_attempts % logging_interval) && 0 == (logging_attempts % alert_attempts))
			{	/* Log ALERT message */
				SNPRINTF(msg_str, SIZEOF(msg_str),
					 "GTM Replication Source Server : Could not connect to secondary in %d seconds\n",
					connection_attempts *
					gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
				sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(msg_str));
				repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
				gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLWARN", print_msg);
			}
			if (logging_period <= REPL_MAX_LOG_PERIOD)
			{	/*the maximum real_period can reach 2*REPL_MAX_LOG_PERIOD)*/
				if (0 == connection_attempts % logging_interval)
				{	/* Double the logging period after every logging attempt*/
					logging_interval = logging_interval << 1;
					logging_period = logging_period << 1;
				}
			}
		} while (TRUE);
	}
	if (0 != (status = get_send_sock_buff_size(gtmsource_sock_fd, &send_buffsize)))
	{
		SNPRINTF(msg_str, SIZEOF(msg_str), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
	}
	if (send_buffsize < GTMSOURCE_TCP_SEND_BUFSIZE)
	{
		for (tcp_s_bufsize = GTMSOURCE_TCP_SEND_BUFSIZE;
		     tcp_s_bufsize >= MAX(send_buffsize, GTMSOURCE_MIN_TCP_SEND_BUFSIZE)
		     &&  0 != (status = set_send_sock_buff_size(gtmsource_sock_fd, tcp_s_bufsize));
		     tcp_s_bufsize -= GTMSOURCE_TCP_SEND_BUFSIZE_INCR)
			;
		if (tcp_s_bufsize < GTMSOURCE_MIN_TCP_SEND_BUFSIZE)
		{
			SNPRINTF(msg_str, SIZEOF(msg_str), "Could not set TCP send buffer size in range [%d, %d], last "
					"known error : %s", GTMSOURCE_MIN_TCP_SEND_BUFSIZE, GTMSOURCE_TCP_SEND_BUFSIZE,
					STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
		}
	}
	if (0 != (status = get_send_sock_buff_size(gtmsource_sock_fd, &repl_max_send_buffsize))) /* may have changed */
	{
		SNPRINTF(msg_str, SIZEOF(msg_str), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
	}
	if (0 != (status = get_recv_sock_buff_size(gtmsource_sock_fd, &recv_buffsize)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Error getting socket recv buffsize"),
				ERR_TEXT, 2, LEN_AND_STR(STRERROR(status)));
	if (recv_buffsize < GTMSOURCE_TCP_RECV_BUFSIZE)
	{
		if (0 != (status = set_recv_sock_buff_size(gtmsource_sock_fd, GTMSOURCE_TCP_RECV_BUFSIZE)))
		{
			if (recv_buffsize < GTMSOURCE_MIN_TCP_RECV_BUFSIZE)
			{
				SNPRINTF(msg_str, SIZEOF(msg_str), "Could not set TCP recv buffer size to %d : %s",
						GTMSOURCE_MIN_TCP_RECV_BUFSIZE, STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2,
						LEN_AND_STR(msg_str));
			}
		}
	}
	if (0 != (status = get_recv_sock_buff_size(gtmsource_sock_fd, &repl_max_recv_buffsize))) /* may have changed */
	{
		SNPRINTF(msg_str, SIZEOF(msg_str), "Error getting socket recv buffsize : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
	}
	return (SS_NORMAL);
}

int gtmsource_alloc_tcombuff(void)
{ /* Allocate buffer for TCOM, ZTCOM records */

	if (NULL == tcombuff)
	{
		assert(NULL == gtmsource_tcombuff_start);
		tcombuff = (unsigned char *)malloc(gd_header->n_regions * TCOM_RECLEN + OS_PAGE_SIZE);
		gtmsource_tcombuff_start = (unsigned char *)ROUND_UP2((unsigned long)tcombuff, OS_PAGE_SIZE);
	}
	return (SS_NORMAL);
}

void gtmsource_free_tcombuff(void)
{
	if (NULL != tcombuff)
	{
		free(tcombuff);
		tcombuff = gtmsource_tcombuff_start = NULL;
	}
	return;
}

int gtmsource_alloc_filter_buff(int bufsiz)
{
	unsigned char	*old_filter_buff, *free_filter_buff;

	bufsiz = ROUND_UP2(bufsiz, OS_PAGE_SIZE);
	if (gtmsource_filter != NO_FILTER && bufsiz > repl_filter_bufsiz)
	{
		REPL_DPRINT3("Expanding filter buff from %d to %d\n", repl_filter_bufsiz, bufsiz);
		free_filter_buff = filterbuff;
		old_filter_buff = repl_filter_buff;
		filterbuff = (unsigned char *)malloc(bufsiz + OS_PAGE_SIZE);
		repl_filter_buff = (unsigned char *)ROUND_UP2((unsigned long)filterbuff, OS_PAGE_SIZE);
		if (NULL != free_filter_buff)
		{
			assert(NULL != old_filter_buff);
			memcpy(repl_filter_buff, old_filter_buff, repl_filter_bufsiz);
			free(free_filter_buff);
		}
		repl_filter_bufsiz = bufsiz;
	}
	return (SS_NORMAL);
}

void gtmsource_free_filter_buff(void)
{
	if (NULL != filterbuff)
	{
		assert(NULL != repl_filter_buff);
		free(filterbuff);
		filterbuff = repl_filter_buff = NULL;
		repl_filter_bufsiz = 0;
	}
}

int gtmsource_alloc_msgbuff(int maxbuffsize)
{ /* Allocate message buffer */

	repl_msg_ptr_t	oldmsgp;
	unsigned char	*free_msgp;

	assert(MIN_REPL_MSGLEN < maxbuffsize);
	maxbuffsize = ROUND_UP2(maxbuffsize, OS_PAGE_SIZE);
	if (maxbuffsize > gtmsource_msgbufsiz || NULL == gtmsource_msgp)
	{
		REPL_DPRINT3("Expanding message buff from %d to %d\n", gtmsource_msgbufsiz, maxbuffsize);
		free_msgp = msgbuff;
		oldmsgp = gtmsource_msgp;
		msgbuff = (unsigned char *)malloc(maxbuffsize + OS_PAGE_SIZE);
		gtmsource_msgp = (repl_msg_ptr_t)ROUND_UP2((unsigned long)msgbuff, OS_PAGE_SIZE);
		if (NULL != free_msgp)
		{ /* Copy existing data */
			assert(NULL != oldmsgp);
			memcpy((unsigned char *)gtmsource_msgp, (unsigned char *)oldmsgp, gtmsource_msgbufsiz);
			free(free_msgp);
		}
		gtmsource_msgbufsiz = maxbuffsize;
		gtmsource_alloc_filter_buff(gtmsource_msgbufsiz);
	}
	return (SS_NORMAL);
}

void gtmsource_free_msgbuff(void)
{
	if (NULL != msgbuff)
	{
		assert(NULL != gtmsource_msgp);
		free(msgbuff);
		msgbuff = NULL;
		gtmsource_msgp = NULL;
		gtmsource_msgbufsiz = 0;
	}
}

int gtmsource_recv_restart(seq_num *recvd_jnl_seqno, int *msg_type, int *start_flags)
{
	/* Receive jnl_seqno for (re)starting transmission */

	fd_set		input_fds;
	repl_msg_t	msg;
	unsigned char	*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int		tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int		torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int		status;					/* needed for REPL_{SEND,RECV}_LOOP */
	unsigned char	seq_num_str[32], *seq_num_ptr;
	repl_msg_t	xoff_ack;
	boolean_t	rcv_node_same_endianness = FALSE;

	status = SS_NORMAL;
	for (; SS_NORMAL == status;)
	{
		repl_log(gtmsource_log_fp, FALSE, FALSE, "Waiting for (re)start JNL_SEQNO/FETCH RESYSNC msg\n");
		REPL_RECV_LOOP(gtmsource_sock_fd, &msg, MIN_REPL_MSGLEN, REPL_POLL_WAIT)
		{
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
		}
		if (SS_NORMAL == status)
		{
			/* Determine endianness of other system by seeing if the msg.len is greater than
			 * expected. If it is, convert it and see if it is now what we expect. If it is,
			 * then the other system is of opposite endianness.
			 * Note: We would normally use msg.type since is is effectively an enum and we
			 * control by adding new messages. But, REPL_START_JNL_SEQNO is lucky number zero
			 * which means it is identical on systems of either endianness.
			 */
			if (((unsigned)MIN_REPL_MSGLEN < (unsigned)msg.len)
					&& ((unsigned)MIN_REPL_MSGLEN == GTM_BYTESWAP_32((unsigned)msg.len)))
				rcv_node_same_endianness = FALSE;
			else
				rcv_node_same_endianness = TRUE;
			if (!rcv_node_same_endianness)
			{
				msg.type = GTM_BYTESWAP_32(msg.type);
				msg.len = GTM_BYTESWAP_32(msg.len);
			}
			assert(msg.type == REPL_START_JNL_SEQNO || msg.type == REPL_FETCH_RESYNC || msg.type == REPL_XOFF_ACK_ME);
			assert(msg.len == MIN_REPL_MSGLEN);
			*msg_type = msg.type;
			*start_flags = START_FLAG_NONE;
			QWASSIGN(*recvd_jnl_seqno, *(seq_num *)&msg.msg[0]);
			if (REPL_START_JNL_SEQNO == msg.type)
			{
				if (!rcv_node_same_endianness)
					*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
				repl_log(gtmsource_log_fp, FALSE, FALSE, "Received (re)start JNL_SEQNO msg %d bytes. seq no "
									 INT8_FMT"\n", recvd_len, INT8_PRINT(*recvd_jnl_seqno));
				if (!rcv_node_same_endianness)
					((repl_start_msg_ptr_t)&msg)->start_flags =
						GTM_BYTESWAP_32(((repl_start_msg_ptr_t)&msg)->start_flags);
				*start_flags = ((repl_start_msg_ptr_t)&msg)->start_flags;
				if (*start_flags & START_FLAG_STOPSRCFILTER)
				{
					repl_log(gtmsource_log_fp, FALSE, FALSE,
						 "Start JNL_SEQNO msg tagged with STOP SOURCE FILTER\n");
					if (gtmsource_filter & EXTERNAL_FILTER)
					{
						repl_stop_filter();
						gtmsource_filter &= ~EXTERNAL_FILTER;
					} else
						repl_log(gtmsource_log_fp, FALSE, FALSE,
							 "Filter is not active, ignoring STOP SOURCE FILTER msg\n");
					*msg_type = REPL_START_JNL_SEQNO;
				}
				assert(*start_flags & START_FLAG_HASINFO); /* V4.2+ versions have jnl ver in the start msg */
				remote_jnl_ver = ((repl_start_msg_ptr_t)&msg)->jnl_ver;
				REPL_DPRINT3("Local jnl ver is octal %o, remote jnl ver is octal %o\n", jnl_ver, remote_jnl_ver);
				repl_check_jnlver_compat();
				assert(remote_jnl_ver > V15_JNL_VER || 0 == (*start_flags & START_FLAG_COLL_M));
				if (remote_jnl_ver <= V15_JNL_VER)
					*start_flags &= ~START_FLAG_COLL_M; /* zap it for pro, just in case */
				secondary_side_std_null_coll = (*start_flags & START_FLAG_COLL_M) ? TRUE : FALSE;
				assert((remote_jnl_ver >= V19_JNL_VER) || (0 == (*start_flags & START_FLAG_TRIGGER_SUPPORT)));
				if (remote_jnl_ver < V19_JNL_VER)
					*start_flags &= ~START_FLAG_TRIGGER_SUPPORT; /* zap it for pro, just in case */
				secondary_side_trigger_support = (*start_flags & START_FLAG_TRIGGER_SUPPORT) ? TRUE : FALSE;
				return (SS_NORMAL);
			} else if (REPL_FETCH_RESYNC == msg.type)
			{
				if (!rcv_node_same_endianness)
					*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
				repl_log(gtmsource_log_fp, TRUE, TRUE, "FETCH RESYNC msg received with SEQNO "INT8_FMT"\n",
					 INT8_PRINT(*(seq_num *)&msg.msg[0]));
				return (SS_NORMAL);
			} else if (REPL_XOFF_ACK_ME == msg.type)
			{
				repl_log(gtmsource_log_fp, FALSE, FALSE, "XOFF received when waiting for (re)start JNL_SEQNO/FETCH "
									"RESYSNC msg. Possible crash/shutdown of update process\n");
				/* Send XOFF_ACK */
				xoff_ack.type = REPL_XOFF_ACK;
				if (!rcv_node_same_endianness)
					*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
				QWASSIGN(*(seq_num *)&xoff_ack.msg[0], *recvd_jnl_seqno);
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
				}
			} else
			{
				repl_log(gtmsource_log_fp, FALSE, FALSE, "UNKNOWN msg received when waiting for (re)start "
									 "JNL_SEQNO/FETCH RESYSNC msg. Ignoring msg\n");
				GTMASSERT;
			}
		}
	}
	return (status);
}

int gtmsource_srch_restart(seq_num recvd_jnl_seqno, int recvd_start_flags)
{
	seq_num			tmp_read_jnl_seqno;
	qw_off_t		tmp_read_addr;
	uint4			tmp_read, prev_tmp_read, prev_tr_size, jnlpool_size;
	int			save_lastwrite_len;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	gd_region		*reg, *region_top;
	sgmnt_addrs		*csa;
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;

	jctl = jnlpool.jnlpool_ctl;
	jnlpool_size = jctl->jnlpool_size;
	gtmsource_local = jnlpool.gtmsource_local;
	if (recvd_start_flags & START_FLAG_UPDATERESYNC)
	{
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		QWASSIGN(gtmsource_local->read_jnl_seqno, jctl->jnl_seqno);
		QWASSIGN(gtmsource_local->read_addr, jctl->write_addr);
		gtmsource_local->read = jctl->write;
		rel_lock(jnlpool.jnlpool_dummy_reg);
		gtmsource_local->read_state = READ_POOL;
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Update resync received, source server now reading from journal pool\n");
		gtmsource_ctl_close();
		REPL_DPRINT1("Received START_FLAG_UPDATERESYNC\n");
	}

	if (QWGT(recvd_jnl_seqno, gtmsource_local->read_jnl_seqno))
	{
		/*
		 * The Receiver is ahead of me though I haven't yet
		 * sent the transactions read_jnl_seqno thru
		 * recvd_jnl_seqno across.
		 */
		/* Log Warning Message */
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Receiver ahead of Source. Source at JNL_SEQNO "INT8_FMT,
			 INT8_PRINT(gtmsource_local->read_jnl_seqno));
		repl_log(gtmsource_log_fp, FALSE, TRUE, ", receiver at "INT8_FMT"\n", INT8_PRINT(recvd_jnl_seqno));
		return (EREPL_SEC_AHEAD);
	}

	QWASSIGN(tmp_read_jnl_seqno, gtmsource_local->read_jnl_seqno);

	if (READ_POOL == gtmsource_local->read_state)
	{
		/* Follow the back-chain in the Journal Pool to find whether
	         * or not recvd_jnl_seqno is in the Pool */

		/* The implementation for searching through the back chain has several inefficiences. We are deferring addressing
		 * them to keep code changes for V4.4-003 to a minimum. We should address these in an upcoming release.
		 * Vinaya 2003, Oct 02 */

		QWASSIGN(tmp_read_addr, gtmsource_local->read_addr);
		QWASSIGN(tmp_read_jnl_seqno, gtmsource_local->read_jnl_seqno);
		tmp_read = gtmsource_local->read;

		if (jnlpool_hasnt_overflowed(jctl, jnlpool_size, tmp_read_addr) &&
		    QWGT(tmp_read_jnl_seqno, recvd_jnl_seqno) &&
		    QWGT(tmp_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			if (QWGE(jctl->early_write_addr, tmp_read_addr))
			{
				/* If there is no more input to be read, the previous transaction size should not be read from the
				 * journal pool since the read pointers point to the next read. In such a case, we can find the
				 * size of the transcation tmp_read_jnl_seqno from jctl->lastwrite_len. We should access
				 * lastwrite_len after a memory barrier to avoid reading a stale value. We rely on the memory
				 * barrier done in jnlpool_hasnt_overflowed */
				save_lastwrite_len = jctl->lastwrite_len;
				if (QWEQ(jctl->early_write_addr, tmp_read_addr))
				{ /* GT.M is not writing any transaction, safe to rely on jctl->lastwrite_len. Note, GT.M could not
				   * have been writing transaction tmp_read_jnl_seqno if we are here. Also, lastwrite_len cannot be
				   * in the future w.r.t early_write_addr because of the memory barriers we do in t{p}_{t}end.c
				   * It can be behind by atmost one transaction (tmp_read_jnl_seqno). Well, we want the length of
				   * transaction tmp_read_jnl_seqno, not tmp_read_jnl_seqno + 1. */
					QWDECRBYDW(tmp_read_addr, save_lastwrite_len);
					QWDECRBYDW(tmp_read_jnl_seqno, 1);
					prev_tmp_read = tmp_read;
					tmp_read -= save_lastwrite_len;
					if (tmp_read >= prev_tmp_read)
						tmp_read += jnlpool_size;
					assert(tmp_read == QWMODDW(tmp_read_addr, jnlpool_size));
					REPL_DPRINT2("Srch restart : No more input in jnlpool, backing off to read_jnl_seqno : "
						     INT8_FMT, INT8_PRINT(tmp_read_jnl_seqno));
					REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n", INT8_PRINT(tmp_read_addr), tmp_read);
				}
			}
			if (QWEQ(tmp_read_addr, jctl->write_addr))
			{ /* we caught a GTM process writing tmp_read_jnl_seqno + 1, we cannot rely on lastwrite_len as it
			   * may or may not have changed. Wait until the GTM process finishes writing this transaction */
				repl_log(gtmsource_log_fp, TRUE, TRUE, "SEARCHING RESYNC POINT IN POOL : Waiting for GTM process "
								       "to finish writing journal records to the pool\n");
				while (QWEQ(tmp_read_addr, jctl->write_addr))
				{
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
					gtmsource_poll_actions(FALSE);
				}
				repl_log(gtmsource_log_fp, TRUE, TRUE, "SEARCHING RESYNC POINT IN POOL : GTM process finished "
								       "writing journal records to the pool\n");
			}
		}

		while (jnlpool_hasnt_overflowed(jctl, jnlpool_size, tmp_read_addr) &&
		       QWGT(tmp_read_jnl_seqno, recvd_jnl_seqno) &&
	               QWGT(tmp_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			assert(tmp_read + SIZEOF(jnldata_hdr_struct) <= jnlpool_size);
			prev_tr_size = ((jnldata_hdr_ptr_t)(jnlpool.jnldata_base + tmp_read))->prev_jnldata_len;
			if ((prev_tr_size <= tmp_read_addr) &&
				jnlpool_hasnt_overflowed(jctl, jnlpool_size, tmp_read_addr - prev_tr_size))
			{
				QWDECRBYDW(tmp_read_addr, prev_tr_size);
				prev_tmp_read = tmp_read;
				tmp_read -= prev_tr_size;
				if (tmp_read >= prev_tmp_read)
					tmp_read += jnlpool_size;
				assert(tmp_read == QWMODDW(tmp_read_addr, jnlpool_size));
				QWDECRBYDW(tmp_read_jnl_seqno, 1);
				REPL_DPRINT2("Srch restart : No overflow yet, backing off to read_jnl_seqno : "INT8_FMT,
					     INT8_PRINT(tmp_read_jnl_seqno));
				REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n", INT8_PRINT(tmp_read_addr), tmp_read);
				continue;
			}
			break;
		}

		QWASSIGN(gtmsource_local->read_addr, tmp_read_addr);
		gtmsource_local->read = tmp_read;

		if (jnlpool_hasnt_overflowed(jctl, jnlpool_size, tmp_read_addr) &&
	            QWEQ(tmp_read_jnl_seqno, recvd_jnl_seqno) &&
	    	    QWGE(tmp_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			REPL_DPRINT2("Srch restart : Now in READ_POOL state read_jnl_seqno : "INT8_FMT,
				     INT8_PRINT(tmp_read_jnl_seqno));
			REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n",INT8_PRINT(tmp_read_addr), tmp_read);
		} else
		{
			/* Overflow, or requested seqno too far back to be in pool */
			REPL_DPRINT2("Srch restart : Now in READ_FILE state. Changing sync point to read_jnl_seqno : "INT8_FMT,
				     INT8_PRINT(tmp_read_jnl_seqno));
			REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d ", INT8_PRINT(tmp_read_addr), tmp_read);
			REPL_DPRINT2("save_read_jnl_seqno : "INT8_FMT"\n", INT8_PRINT(gtmsource_save_read_jnl_seqno));

			QWASSIGN(gtmsource_save_read_jnl_seqno, tmp_read_jnl_seqno);
			if (QWLT(gtmsource_save_read_jnl_seqno, jctl->start_jnl_seqno))
			{
				QWASSIGN(gtmsource_save_read_jnl_seqno, jctl->start_jnl_seqno);
				assert(QWEQ(gtmsource_local->read_addr, seq_num_zero));
				assert(gtmsource_local->read == 0);
				/* For pro version, force zero assignment */
				QWASSIGN(gtmsource_local->read_addr, seq_num_zero);
				gtmsource_local->read = 0;
				REPL_DPRINT2("Srch restart : Sync point "INT8_FMT, INT8_PRINT(gtmsource_save_read_jnl_seqno));
				REPL_DPRINT2(" beyond start_seqno : "INT8_FMT, INT8_PRINT(jctl->start_jnl_seqno));
				REPL_DPRINT3(", sync point set to read_addr : "INT8_FMT" read : %d\n",
					     INT8_PRINT(gtmsource_local->read_addr), gtmsource_local->read);
			}
			gtmsource_local->read_state = READ_FILE;

			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal files; journal pool "
					"does not contain transaction %llu\n", recvd_jnl_seqno);
			gtmsource_pool2file_transition = TRUE;
		}
	} else /* read_state is READ_FILE and requesting a sequence number
	        * less than or equal to read_jnl_seqno */
	{
		if (QWGT(tmp_read_jnl_seqno,  gtmsource_save_read_jnl_seqno))
			QWASSIGN(gtmsource_save_read_jnl_seqno, tmp_read_jnl_seqno);
		REPL_DPRINT2("Srch restart : Continuing in READ_FILE state. Retaining sync point for read_jnl_seqno : "INT8_FMT,
			     INT8_PRINT(tmp_read_jnl_seqno));
		REPL_DPRINT2(" at read_addr : "INT8_FMT, INT8_PRINT(gtmsource_local->read_addr));
		REPL_DPRINT3(" read : %d corresponding to save_read_jnl_seqno : "INT8_FMT"\n", gtmsource_local->read,
			     INT8_PRINT(gtmsource_save_read_jnl_seqno));
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server continuing to read from journal files at transaction %llu\n",
				recvd_jnl_seqno);
	}

	QWASSIGN(gtmsource_local->read_jnl_seqno, recvd_jnl_seqno);

	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		csa = &FILE_INFO(reg)->s_addrs;
		if (REPL_ALLOWED(csa->hdr))
		{
#ifndef INT8_SUPPORTED
			grab_crit(reg); /* File-header sync is done in crit, and so grab_crit here */
#endif
			QWASSIGN(FILE_INFO(reg)->s_addrs.hdr->resync_seqno, recvd_jnl_seqno);
			REPL_DPRINT3("Setting resync_seqno of %s to "INT8_FMT"\n", reg->rname, INT8_PRINT(recvd_jnl_seqno));
#ifndef INT8_SUPPORTED
			rel_crit(reg);
#endif
		}
	}

	return (SS_NORMAL);
}

int gtmsource_get_jnlrecs(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multpile)
{
	int 			total_tr_len;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;
	seq_num			jnl_seqno, read_jnl_seqno;
	qw_num			write_addr, read_addr;

	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	write_addr = jctl->write_addr;
	jnl_seqno = jctl->jnl_seqno;
	read_jnl_seqno = gtmsource_local->read_jnl_seqno;
	read_addr = gtmsource_local->read_addr;
	assert(read_addr <= write_addr);
	assert((0 != write_addr) || (read_jnl_seqno <= jctl->start_jnl_seqno));

#ifdef GTMSOURCE_ALWAYS_READ_FILES
	gtmsource_local->read_state = READ_FILE;
#endif
	switch(gtmsource_local->read_state)
	{
		case READ_POOL:
#ifndef GTMSOURCE_ALWAYS_READ_FILES_STRESS
			if (read_addr == write_addr)
			{/* Nothing to read. While reading pool, the comparison of read_addr against write_addr is the only
			  * reliable indicator if there are any transactions to be read. This is due to the placement of
			  * memory barriers in t_end/tp_tend.c. Also, since we do not issue memory barrier here, we may be reading
			  * a stale value of write_addr in which case we may conclude that there is nothing to read. But, this will
			  * not continue forever as the source server eventually (decided by architecture's implementation) will see
			  * the change to write_addr.
			  */
				*data_len = 0;
				return (0);
			}
			if (0 < (total_tr_len = gtmsource_readpool(buff, data_len, maxbufflen, read_multpile, write_addr)))
				return (total_tr_len);
			if (0 < *data_len)
				return (-1);
#endif /* for GTMSOURCE_ALWAYS_READ_FILES_STRESS, we let the source server switch back and forth between pool read and file read */
			/* Overflow, switch to READ_FILE */
			gtmsource_local->read_state = READ_FILE;
			QWASSIGN(gtmsource_save_read_jnl_seqno, read_jnl_seqno);
			gtmsource_pool2file_transition = TRUE; /* so that we read the latest gener jnl files */
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal files; journal pool "
					"overflow detected at transaction %llu\n", gtmsource_save_read_jnl_seqno);

			/* CAUTION : FALL THROUGH */

		case READ_FILE:
			if (read_jnl_seqno >= jnl_seqno)
			{ /* Nothing to read. While reading from files, source server does not use write_addr to decide
			   * how much to read. Also, it is possible that read_addr and write_addr are the same if the
			   * source server came up after a crash and syncs with the latest state in jnlpool (see
			   * gtmsource()). These reasons preclude the comparison of read_addr and write_addr (as we did for
			   * READ_POOL case) to decide whether there are any unsent transactions. We use jnl_seqno instead.
			   * Note though, the source server's view of jnl_seqno may be stale, and we may conclude that
			   * we don't have anything to read as we do not do memory barrier here to fetch the latest
			   * value of jnl_seqno. But, this will not continue forever as the source server eventually
			   * (decided by architecture's implementation) will see the change to jnl_seqno.
			   *
			   * On systems that allow unordered memory access, it is possible that the value of jnl_seqno
			   * as seen by source server is in the past compared to read_jnl_seqno - source server in
			   * keeping up with updaters reads (from pool) and sends upto write_addr, the last transaction
			   * of which could be jnl_seqno + 1. To cover the case, we use >= in the above comparison.
			   * Given this, we may return with "nothing to read" even though we fell through from the READ_POOL case.
			   */
				*data_len = 0;
				return 0;
			}
			if (gtmsource_pool2file_transition /* read_pool -> read_file transition */
			    || NULL == repl_ctl_list) /* files not opened */
			{
				/* Close all the file read related structures
				 * and start afresh. The idea here is that
				 * most of the file read info might be stale
				 * 'cos there is usually a long gap between
				 * pool to file read transitions (in
				 * production environments). So, start afresh
				 * with the latest generation journal files.
				 * This will also prevent opening previous
				 * generations that may not be required.
				 */
				REPL_DPRINT1("Pool to File read transition. Closing all the stale file read info and starting "
					     "afresh\n");
				gtmsource_ctl_close();
				gtmsource_ctl_init();
				gtmsource_pool2file_transition = FALSE;
			}
			if (0 < (total_tr_len = gtmsource_readfiles(buff, data_len, maxbufflen, read_multpile)))
				return (total_tr_len);
			if (0 < *data_len)
				return (-1);
			GTMASSERT;
	}
}
