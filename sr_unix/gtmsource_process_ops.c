/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#if defined(__MVS__) && !defined(_ISOC99_SOURCE)
#define _ISOC99_SOURCE
#endif

#include "mdef.h"

#include "gtm_stdio.h"	/* for FILE * in repl_comm.h */
#include "gtm_socket.h"
#include "gtm_inet.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_string.h"
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
#include "repl_shutdcode.h"
#include "jnl.h"
#include "hashtab_mname.h"	  /* needed for muprec.h */
#include "hashtab_int4.h"	  /* needed for muprec.h */
#include "hashtab_int8.h"	  /* needed for muprec.h */
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
#include "repl_instance.h"
#include "ftok_sems.h"
#include "gtmmsg.h"
#include "wbox_test_init.h"
#include "have_crit.h"			/* needed for ZLIB_COMPRESS */
#include "deferred_signal_handler.h"	/* needed for ZLIB_COMPRESS */
#include "gtm_zlib.h"
#include "replgbl.h"

GBLREF	gd_addr			*gd_header;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	int			gtmsource_sock_fd;
GBLREF	seq_num			gtmsource_save_read_jnl_seqno;
GBLREF	struct timeval		gtmsource_poll_wait, gtmsource_poll_immediate;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	repl_msg_ptr_t		gtmsource_cmpmsgp;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	int			gtmsource_cmpmsgbufsiz;
GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	unsigned char		*gtmsource_tcombuffp;
GBLREF	int			gtmsource_log_fd;
GBLREF	int			gtmsource_statslog_fd;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	FILE			*gtmsource_statslog_fp;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_filter;
GBLREF	seq_num			seq_num_zero;
GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	boolean_t		gtmsource_pool2file_transition;
GBLREF	repl_ctl_element	*repl_ctl_list;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	boolean_t		secondary_side_std_null_coll;
GBLREF	boolean_t		secondary_side_trigger_support;
GBLREF	uint4			process_id;
GBLREF	boolean_t		gtmsource_received_cmp2uncmp_msg;

error_def(ERR_REPLCOMM);
error_def(ERR_REPLFTOKSEM);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_REPLINSTSECMTCH);
error_def(ERR_REPLNOXENDIAN);
error_def(ERR_REPLWARN);
error_def(ERR_TEXT);

static	unsigned char		*tcombuff, *msgbuff, *cmpmsgbuff, *filterbuff;

void gtmsource_init_sec_addr(struct sockaddr_in *secondary_addr)
{
	gtmsource_local_ptr_t	gtmsource_local;

	gtmsource_local = jnlpool.gtmsource_local;
	memset((char *)secondary_addr, 0, SIZEOF(*secondary_addr));
	(*secondary_addr).sin_family = AF_INET;
	(*secondary_addr).sin_addr.s_addr = gtmsource_local->secondary_inet_addr;
	(*secondary_addr).sin_port = htons(gtmsource_local->secondary_port);
}

int gtmsource_est_conn(struct sockaddr_in *secondary_addr)
{
	int			connection_attempts, alert_attempts, save_errno, status;
	char			print_msg[1024], msg_str[1024], *errmsg;
	gtmsource_local_ptr_t	gtmsource_local;
	int			send_buffsize, recv_buffsize, tcp_s_bufsize;

	gtmsource_local = jnlpool.gtmsource_local;
	gtmsource_local->remote_proto_ver = REPL_PROTO_VER_UNINITIALIZED;
	/* Connect to the secondary - use hard tries, soft tries ... */
	connection_attempts = 0;
	gtmsource_comm_init();
	repl_log(gtmsource_log_fp, TRUE, TRUE, "Connect hard tries count = %d, Connect hard tries period = %d\n",
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT],
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD]);
	do
	{
		status = gtm_connect(gtmsource_sock_fd, (struct sockaddr *)secondary_addr, SIZEOF(*secondary_addr));
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
	{
		alert_attempts = DIVIDE_ROUND_DOWN(gtmsource_local->connect_parms[GTMSOURCE_CONN_ALERT_PERIOD],
							gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Soft tries period = %d, Alert period = %d\n",
			 gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD],
			 alert_attempts * gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
		connection_attempts = 0;
		do
		{
			status = gtm_connect(gtmsource_sock_fd, (struct sockaddr *)secondary_addr, SIZEOF(*secondary_addr));
			if (0 == status)
				break;
			repl_close(&gtmsource_sock_fd);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "%d soft connection attempt failed : %s\n",
				 connection_attempts + 1, STRERROR(ERRNO));
			LONG_SLEEP(gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			gtmsource_comm_init();
			connection_attempts++;
			if (0 == connection_attempts % alert_attempts)
			{ /* Log ALERT message */
				SNPRINTF(msg_str, SIZEOF(msg_str),
					 "GTM Replication Source Server : Could not connect to secondary in %d seconds\n",
					connection_attempts *
					gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
				sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(msg_str));
				repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
				gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLWARN", print_msg);
			}
		} while (TRUE);
	}
	if (0 != (status = get_send_sock_buff_size(gtmsource_sock_fd, &send_buffsize)))
	{
		SNPRINTF(msg_str, SIZEOF(msg_str), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
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
			rts_error(VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
		}
	}
	if (0 != (status = get_send_sock_buff_size(gtmsource_sock_fd, &repl_max_send_buffsize))) /* may have changed */
	{
		SNPRINTF(msg_str, SIZEOF(msg_str), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
	}
	if (0 != (status = get_recv_sock_buff_size(gtmsource_sock_fd, &recv_buffsize)))
	{
		errmsg = STRERROR(status);
		rts_error(VARLSTCNT(10) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_LIT("Error getting socket recv buffsize"),
			ERR_TEXT, 2, RTS_ERROR_STRING(errmsg));
	}
	if (recv_buffsize < GTMSOURCE_TCP_RECV_BUFSIZE)
	{
		if (0 != (status = set_recv_sock_buff_size(gtmsource_sock_fd, GTMSOURCE_TCP_RECV_BUFSIZE)))
		{
			if (recv_buffsize < GTMSOURCE_MIN_TCP_RECV_BUFSIZE)
			{
				SNPRINTF(msg_str, SIZEOF(msg_str), "Could not set TCP recv buffer size to %d : %s",
						GTMSOURCE_MIN_TCP_RECV_BUFSIZE, STRERROR(status));
				rts_error(VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
			}
		}
	}
	if (0 != (status = get_recv_sock_buff_size(gtmsource_sock_fd, &repl_max_recv_buffsize))) /* may have changed */
	{
		SNPRINTF(msg_str, SIZEOF(msg_str), "Error getting socket recv buffsize : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(msg_str));
	}
	repl_log(gtmsource_log_fp, TRUE, TRUE, "Connected to secondary, using TCP send buffer size %d receive buffer size %d\n",
			repl_max_send_buffsize, repl_max_recv_buffsize);
	repl_log_conn_info(gtmsource_sock_fd, gtmsource_log_fp);
	/* re-determine compression level on the replication pipe after every connection establishment */
	gtmsource_local->repl_zlib_cmp_level = repl_zlib_cmp_level = ZLIB_CMPLVL_NONE;
	/* reset any CMP2UNCMP messages received in prior connections. Once a connection encounters a REPL_CMP2UNCMP message
	 * all further replication on that connection will be uncompressed.
	 */
	gtmsource_received_cmp2uncmp_msg = FALSE;
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
	if ((maxbuffsize > gtmsource_msgbufsiz) || (NULL == gtmsource_msgp))
	{
		REPL_DPRINT3("Expanding message buff from %d to %d\n", gtmsource_msgbufsiz, maxbuffsize);
		free_msgp = msgbuff;
		oldmsgp = gtmsource_msgp;
		msgbuff = (unsigned char *)malloc(maxbuffsize + OS_PAGE_SIZE);
		gtmsource_msgp = (repl_msg_ptr_t)ROUND_UP2((unsigned long)msgbuff, OS_PAGE_SIZE);
		if (NULL != free_msgp)
		{	/* Copy existing data */
			assert(NULL != oldmsgp);
			memcpy((unsigned char *)gtmsource_msgp, (unsigned char *)oldmsgp, gtmsource_msgbufsiz);
			free(free_msgp);
		}
		gtmsource_msgbufsiz = maxbuffsize;
		if (ZLIB_CMPLVL_NONE != gtm_zlib_cmp_level)
		{	/* Compression is enabled. Allocate parallel buffers to hold compressed journal records.
			 * Allocate extra space just in case compression actually expands the data (needed only in rare cases).
			 */
			free_msgp = cmpmsgbuff;
			cmpmsgbuff = (unsigned char *)malloc((maxbuffsize * MAX_CMP_EXPAND_FACTOR) + OS_PAGE_SIZE);
			gtmsource_cmpmsgp = (repl_msg_ptr_t)ROUND_UP2((unsigned long)cmpmsgbuff, OS_PAGE_SIZE);
			if (NULL != free_msgp)
				free(free_msgp);
			gtmsource_cmpmsgbufsiz = (maxbuffsize * MAX_CMP_EXPAND_FACTOR);
		}
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
		if (ZLIB_CMPLVL_NONE != gtm_zlib_cmp_level)
		{	/* Compression is enabled. Free up compression buffer as well. */
			assert(NULL != gtmsource_cmpmsgp);
			free(cmpmsgbuff);
			cmpmsgbuff = NULL;
			gtmsource_cmpmsgp = NULL;
			gtmsource_cmpmsgbufsiz = 0;
		}
	}
}

int gtmsource_recv_restart(seq_num *recvd_jnl_seqno, int *msg_type, int *start_flags, boolean_t *rcvr_same_endianness)
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
	boolean_t	lcl_endianness;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	status = SS_NORMAL;
	jnlpool.gtmsource_local->remote_proto_ver = REPL_PROTO_VER_UNINITIALIZED;
	*rcvr_same_endianness = FALSE;
	for (; SS_NORMAL == status;)
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Waiting for REPL_START_JNL_SEQNO or REPL_FETCH_RESYNC message\n");
		REPL_RECV_LOOP(gtmsource_sock_fd, &msg, MIN_REPL_MSGLEN, FALSE, &gtmsource_poll_wait)
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
			{
				lcl_endianness = *rcvr_same_endianness = FALSE;
				msg.type = GTM_BYTESWAP_32(msg.type);
				msg.len = GTM_BYTESWAP_32(msg.len);
			}
			else
				lcl_endianness = *rcvr_same_endianness = TRUE;
			assert(msg.type == REPL_START_JNL_SEQNO || msg.type == REPL_FETCH_RESYNC || msg.type == REPL_XOFF_ACK_ME);
			assert(msg.len == MIN_REPL_MSGLEN);
			*msg_type = msg.type;
			*start_flags = START_FLAG_NONE;
			memcpy((uchar_ptr_t)recvd_jnl_seqno, (uchar_ptr_t)&msg.msg[0], SIZEOF(seq_num));
			if (REPL_START_JNL_SEQNO == msg.type)
			{
				if (!lcl_endianness)
					*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Received REPL_START_JNL_SEQNO message with SEQNO "
					INT8_FMT"\n", INT8_PRINT(*recvd_jnl_seqno));
				if (!lcl_endianness)
					((repl_start_msg_ptr_t)&msg)->start_flags =
						GTM_BYTESWAP_32(((repl_start_msg_ptr_t)&msg)->start_flags);
				*start_flags = ((repl_start_msg_ptr_t)&msg)->start_flags;
				assert(lcl_endianness
					|| (NODE_ENDIANNESS != ((repl_start_msg_ptr_t)&msg)->node_endianness));
				if (*start_flags & START_FLAG_STOPSRCFILTER)
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE,
						 "Start JNL_SEQNO msg tagged with STOP SOURCE FILTER\n");
					if (gtmsource_filter & EXTERNAL_FILTER)
					{
						repl_stop_filter();
						gtmsource_filter &= ~EXTERNAL_FILTER;
					} else
						repl_log(gtmsource_log_fp, TRUE, TRUE,
							 "Filter is not active, ignoring STOP SOURCE FILTER msg\n");
					*msg_type = REPL_START_JNL_SEQNO;
				}
				/* Determine the protocol version of the receiver side. That information is encoded in the
				 * "proto_ver" field of the message from V51 onwards but to differentiate V50 vs V51 we need
				 * to check if the START_FLAG_VERSION_INFO bit is set in start_flags. If not the protocol is V50.
				 * V51 implies support for multi-site while V50 implies dual-site configuration only.
				 */
				if (*start_flags & START_FLAG_VERSION_INFO)
				{
					assert(REPL_PROTO_VER_DUALSITE != ((repl_start_msg_ptr_t)&msg)->proto_ver);
					jnlpool.gtmsource_local->remote_proto_ver = ((repl_start_msg_ptr_t)&msg)->proto_ver;
				} else
					jnlpool.gtmsource_local->remote_proto_ver = REPL_PROTO_VER_DUALSITE;
				assert(*start_flags & START_FLAG_HASINFO); /* V4.2+ versions have jnl ver in the start msg */
				remote_jnl_ver = ((repl_start_msg_ptr_t)&msg)->jnl_ver;
				REPL_DPRINT3("Local jnl ver is octal %o, remote jnl ver is octal %o\n", jnl_ver, remote_jnl_ver);
				repl_check_jnlver_compat(lcl_endianness);
				assert(remote_jnl_ver > V15_JNL_VER || 0 == (*start_flags & START_FLAG_COLL_M));
				if (remote_jnl_ver <= V15_JNL_VER)
					*start_flags &= ~START_FLAG_COLL_M; /* zap it for pro, just in case */
				secondary_side_std_null_coll = (*start_flags & START_FLAG_COLL_M) ? TRUE : FALSE;
				assert((remote_jnl_ver >= V19_JNL_VER) || (0 == (*start_flags & START_FLAG_TRIGGER_SUPPORT)));
				if (remote_jnl_ver < V19_JNL_VER)
					*start_flags &= ~START_FLAG_TRIGGER_SUPPORT; /* zap it for pro, just in case */
				secondary_side_trigger_support = (*start_flags & START_FLAG_TRIGGER_SUPPORT) ? TRUE : FALSE;
#				ifdef GTM_TRIGGER
				if (!secondary_side_trigger_support)
					repl_log(gtmsource_log_fp, TRUE, TRUE, "Warning : Secondary does not support GT.M "
						"database triggers. #t updates on primary will not be replicated\n");
#				endif
				(TREF(replgbl)).trig_replic_warning_issued = FALSE;
				return (SS_NORMAL);
			} else if (REPL_FETCH_RESYNC == msg.type)
			{	/* Determine the protocol version of the receiver side.
				 * Take care to set the flush parameter in repl_log calls below to FALSE until at least the
				 * first message gets sent back. This is so the fetchresync rollback on the other side does
				 * not timeout before receiving a response. */
				jnlpool.gtmsource_local->remote_proto_ver = (RECAST(repl_resync_msg_ptr_t)&msg)->proto_ver;
				if (!lcl_endianness)
					*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Received REPL_FETCH_RESYNC message with SEQNO "
					INT8_FMT"\n", INT8_PRINT(*recvd_jnl_seqno));
				return (SS_NORMAL);
			} else if (REPL_XOFF_ACK_ME == msg.type)
			{
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Received REPL_XOFF_ACK_ME message. "
									"Possible crash/shutdown of update process\n");
				/* Send XOFF_ACK */
				xoff_ack.type = REPL_XOFF_ACK;
				if (!lcl_endianness)
					*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
				memcpy((uchar_ptr_t)&xoff_ack.msg[0], (uchar_ptr_t)recvd_jnl_seqno, SIZEOF(seq_num));
				xoff_ack.len = MIN_REPL_MSGLEN;
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Sending REPL_XOFF_ACK message\n");
				REPL_SEND_LOOP(gtmsource_sock_fd, &xoff_ack, xoff_ack.len, FALSE, &gtmsource_poll_immediate)
				{
					gtmsource_poll_actions(FALSE);
					if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						return (SS_NORMAL);
				}
			} else
			{	/* If unknown message is received, close connection. Caller will reopen the same. */
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Received UNKNOWN message (type = %d). "
					"Closing connection.\n", msg.type);
				assert(FALSE);
				repl_close(&gtmsource_sock_fd);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
				gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
				return (SS_NORMAL);
			}
		}
	}
	return (status);
}

int gtmsource_srch_restart(seq_num recvd_jnl_seqno, int recvd_start_flags)
{
	seq_num			cur_read_jnl_seqno;
	qw_off_t		cur_read_addr;
	uint4			cur_read, prev_read, prev_tr_size, jnlpool_size;
	int			save_lastwrite_len;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;
	gd_region		*reg, *region_top;
	sgmnt_addrs		*csa, *repl_csa;

	jctl = jnlpool.jnlpool_ctl;
	jnlpool_size = jctl->jnlpool_size;
	gtmsource_local = jnlpool.gtmsource_local;
	if (recvd_start_flags & START_FLAG_UPDATERESYNC)
	{
		DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
		assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
		grab_lock(jnlpool.jnlpool_dummy_reg);
		QWASSIGN(gtmsource_local->read_jnl_seqno, jctl->jnl_seqno);
		QWASSIGN(gtmsource_local->read_addr, jctl->write_addr);
		gtmsource_local->read = jctl->write;
		rel_lock(jnlpool.jnlpool_dummy_reg);
		gtmsource_local->read_state = READ_POOL;
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Update resync received, source server now reading from journal pool\n");
		gtmsource_ctl_close();
		REPL_DPRINT1("Received START_FLAG_UPDATERESYNC\n");
	}
	assert(recvd_jnl_seqno <= jctl->jnl_seqno);
	cur_read_jnl_seqno = gtmsource_local->read_jnl_seqno;
	if (recvd_jnl_seqno > cur_read_jnl_seqno)
	{	/* The secondary is requesting a seqno higher than what we last remember having sent but yet it is in
		 * sync with us upto seqno "recvd_jnl_seqno" as otherwise the caller would have determined it is out
		 * of sync and not even call us. This means that we are going to bump "gtmsource_local->read_jnl_seqno"
		 * up to the received seqno (in the later call to "gtmsource_flush_fh") without knowing how many bytes of
		 * transaction data that meant (to correspondingly bump up "gtmsource_local->read_addr"). The only case
		 * where we dont care to maintain "read_addr" is if we are in READ_FILE mode AND the current read_jnl_seqno
		 * and the received seqno is both lesser than or equal to "gtmsource_save_read_jnl_seqno". Except for
		 * that case, we need to reset "gtmsource_save_read_jnl_seqno" to correspond to the current jnl seqno.
		 */
		if ((READ_FILE != gtmsource_local->read_state) || (recvd_jnl_seqno > gtmsource_save_read_jnl_seqno))
		{
			grab_lock(jnlpool.jnlpool_dummy_reg);
			gtmsource_local->read_state = READ_FILE;
			gtmsource_save_read_jnl_seqno = jctl->jnl_seqno;
			gtmsource_local->read_addr = jnlpool.jnlpool_ctl->write_addr;
			gtmsource_local->read = jnlpool.jnlpool_ctl->write;
			rel_lock(jnlpool.jnlpool_dummy_reg);
		}
	} else if (READ_POOL == gtmsource_local->read_state)
	{	/* Follow the back-chain in the Journal Pool to find whether or not recvd_jnl_seqno is in the Pool */

		/* The implementation for searching through the back chain has several inefficiences. We are deferring addressing
		 * them to keep code changes for V4.4-003 to a minimum. We should address these in an upcoming release.
		 * Vinaya 2003, Oct 02 */

		QWASSIGN(cur_read_addr, gtmsource_local->read_addr);
		cur_read = gtmsource_local->read;

		if (jnlpool_hasnt_overflowed(jctl, jnlpool_size, cur_read_addr) &&
			 QWGT(cur_read_jnl_seqno, recvd_jnl_seqno) &&
			 QWGT(cur_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			if (QWGE(jctl->early_write_addr, cur_read_addr))
			{	/* If there is no more input to be read, the previous transaction size should not be read from the
				 * journal pool since the read pointers point to the next read. In such a case, we can find the
				 * size of the transcation cur_read_jnl_seqno from jctl->lastwrite_len. We should access
				 * lastwrite_len after a memory barrier to avoid reading a stale value. We rely on the memory
				 * barrier done in jnlpool_hasnt_overflowed */
				save_lastwrite_len = jctl->lastwrite_len;
				if (QWEQ(jctl->early_write_addr, cur_read_addr))
				{	/* GT.M is not writing any transaction, safe to rely on jctl->lastwrite_len. Note,
					 * GT.M could not have been writing transaction cur_read_jnl_seqno if we are here. Also,
					 * lastwrite_len cannot be in the future w.r.t early_write_addr because of the memory
					 * barriers we do in t{p}_{t}end.c. It can be behind by atmost one transaction
					 * (cur_read_jnl_seqno). Well, we want the length of transaction cur_read_jnl_seqno,
					 * not cur_read_jnl_seqno + 1.
					 */
					QWDECRBYDW(cur_read_addr, save_lastwrite_len);
					QWDECRBYDW(cur_read_jnl_seqno, 1);
					prev_read = cur_read;
					if (cur_read > save_lastwrite_len)
						cur_read -= save_lastwrite_len;
					else
					{
						cur_read = cur_read - (save_lastwrite_len % jnlpool_size);
						if (cur_read >= prev_read)
							cur_read += jnlpool_size;
					}
					assert(cur_read == QWMODDW(cur_read_addr, jnlpool_size));
					REPL_DPRINT2("Srch restart : No more input in jnlpool, backing off to read_jnl_seqno : "
							  INT8_FMT, INT8_PRINT(cur_read_jnl_seqno));
					REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n", INT8_PRINT(cur_read_addr), cur_read);
				}
			}
			if (QWEQ(cur_read_addr, jctl->write_addr))
			{ /* we caught a GTM process writing cur_read_jnl_seqno + 1, we cannot rely on lastwrite_len as it
				* may or may not have changed. Wait until the GTM process finishes writing this transaction */
				repl_log(gtmsource_log_fp, TRUE, FALSE, "SEARCHING RESYNC POINT IN POOL : Waiting for GTM process "
										 "to finish writing journal records to the pool\n");
				while (QWEQ(cur_read_addr, jctl->write_addr))
				{
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
					gtmsource_poll_actions(FALSE);
				}
				repl_log(gtmsource_log_fp, TRUE, FALSE, "SEARCHING RESYNC POINT IN POOL : GTM process finished "
										 "writing journal records to the pool\n");
			}
		}
		while (jnlpool_hasnt_overflowed(jctl, jnlpool_size, cur_read_addr) &&
				 QWGT(cur_read_jnl_seqno, recvd_jnl_seqno) &&
						QWGT(cur_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			assert(cur_read + SIZEOF(jnldata_hdr_struct) <= jnlpool_size);
			prev_tr_size = ((jnldata_hdr_ptr_t)(jnlpool.jnldata_base + cur_read))->prev_jnldata_len;
			if (jnlpool_hasnt_overflowed(jctl, jnlpool_size, cur_read_addr))
			{
				QWDECRBYDW(cur_read_addr, prev_tr_size);
				prev_read = cur_read;
				cur_read -= prev_tr_size;
				if (cur_read >= prev_read)
					cur_read += jnlpool_size;
				assert(cur_read == QWMODDW(cur_read_addr, jnlpool_size));
				QWDECRBYDW(cur_read_jnl_seqno, 1);
				REPL_DPRINT2("Srch restart : No overflow yet, backing off to read_jnl_seqno : "INT8_FMT,
						  INT8_PRINT(cur_read_jnl_seqno));
				REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n", INT8_PRINT(cur_read_addr), cur_read);
				continue;
			}
			break;
		}
		QWASSIGN(gtmsource_local->read_addr, cur_read_addr);
		gtmsource_local->read = cur_read;

		if (jnlpool_hasnt_overflowed(jctl, jnlpool_size, cur_read_addr) &&
					QWEQ(cur_read_jnl_seqno, recvd_jnl_seqno) &&
				 QWGE(cur_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			REPL_DPRINT2("Srch restart : Now in READ_POOL state read_jnl_seqno : "INT8_FMT,
					  INT8_PRINT(cur_read_jnl_seqno));
			REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n",INT8_PRINT(cur_read_addr), cur_read);
		} else
		{
			/* Overflow, or requested seqno too far back to be in pool */
			REPL_DPRINT2("Srch restart : Now in READ_FILE state. Changing sync point to read_jnl_seqno : "INT8_FMT,
					  INT8_PRINT(cur_read_jnl_seqno));
			REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d ", INT8_PRINT(cur_read_addr), cur_read);
			REPL_DPRINT2("save_read_jnl_seqno : "INT8_FMT"\n", INT8_PRINT(gtmsource_save_read_jnl_seqno));

			QWASSIGN(gtmsource_save_read_jnl_seqno, cur_read_jnl_seqno);
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
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Source server now reading from journal files; journal pool "
					"does not contain transaction %llu\n", recvd_jnl_seqno);
			gtmsource_pool2file_transition = TRUE;
		}
	} else /* read_state is READ_FILE and requesting a sequence number less than or equal to read_jnl_seqno */
	{
		QWASSIGN(cur_read_jnl_seqno, gtmsource_local->read_jnl_seqno);
		if (QWGT(cur_read_jnl_seqno,  gtmsource_save_read_jnl_seqno))
			QWASSIGN(gtmsource_save_read_jnl_seqno, cur_read_jnl_seqno);
		REPL_DPRINT2("Srch restart : Continuing in READ_FILE state. Retaining sync point for read_jnl_seqno : "INT8_FMT,
				  INT8_PRINT(cur_read_jnl_seqno));
		REPL_DPRINT2(" at read_addr : "INT8_FMT, INT8_PRINT(gtmsource_local->read_addr));
		REPL_DPRINT3(" read : %d corresponding to save_read_jnl_seqno : "INT8_FMT"\n", gtmsource_local->read,
				  INT8_PRINT(gtmsource_save_read_jnl_seqno));
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Source server continuing to read from journal files at seqno "
			"%llu [0x%llx]\n", recvd_jnl_seqno, recvd_jnl_seqno);
	}
	REPL_DPRINT2("Setting resync_seqno to "INT8_FMT"\n", INT8_PRINT(recvd_jnl_seqno));
	repl_log(gtmsource_log_fp, TRUE, FALSE, "Source server last sent seqno %llu [0x%llx]\n",
		cur_read_jnl_seqno, cur_read_jnl_seqno);
	repl_log(gtmsource_log_fp, TRUE, FALSE, "Source server will start sending from seqno %llu [0x%llx]\n",
		recvd_jnl_seqno, recvd_jnl_seqno);
	if (READ_POOL == gtmsource_local->read_state)
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from the journal POOL\n");
	else
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from the journal FILES\n");
	/* Now that we are going to send seqnos from "recvd_jnl_seqno" onwards, reset "csd->dualsite_resync_seqno"
	 * to the next seqno that will be sent across. Do this only if the secondary is still dualsite.
	 */
	assert(REPL_PROTO_VER_UNINITIALIZED != jnlpool.gtmsource_local->remote_proto_ver);
	if (REPL_PROTO_VER_DUALSITE == jnlpool.gtmsource_local->remote_proto_ver)
	{
		region_top = gd_header->regions + gd_header->n_regions;
		for (reg = gd_header->regions; reg < region_top; reg++)
		{
			csa = &FILE_INFO(reg)->s_addrs;
			if (REPL_ALLOWED(csa->hdr))
			{
#				ifndef INT8_SUPPORTED
				assert(!csa->hold_onto_crit);	/* so it is ok to invoke "grab_crit" unconditionally */
				grab_crit(reg); /* File-header sync is done in crit, and so grab_crit here */
#				endif
				QWASSIGN(FILE_INFO(reg)->s_addrs.hdr->dualsite_resync_seqno, recvd_jnl_seqno);
				REPL_DPRINT3("Setting dualsite_resync_seqno of %s to "INT8_FMT"\n", reg->rname,
					INT8_PRINT(recvd_jnl_seqno));
#				ifndef INT8_SUPPORTED
				assert(!csa->hold_onto_crit);	/* so it is ok to invoke "rel_crit" unconditionally */
				rel_crit(reg);
#				endif
			}
		}
	}
	/* Finally set "gtmsource_local->read_jnl_seqno" to be "recvd_jnl_seqno" and flush changes to instance file on disk */
	gtmsource_flush_fh(recvd_jnl_seqno);
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
			{	/* Nothing to read. While reading pool, the comparison of read_addr against write_addr is
				 * the only reliable indicator if there are any transactions to be read. This is due to
				 * the placement of memory barriers in t_end/tp_tend.c. Also, since we do not issue memory
				 * barrier here, we may be reading a stale value of write_addr in which case we may conclude
				 * that there is nothing to read. But, this will not continue forever as the source server
				 * eventually (decided by architecture's implementation) will see the change to write_addr.
				 */
				*data_len = 0;
				return (0);
			}
			total_tr_len = gtmsource_readpool(buff, data_len, maxbufflen, read_multpile, write_addr);
			if (GTMSOURCE_SEND_NEW_TRIPLE == gtmsource_state)
				return (0); /* REPL_NEW_TRIPLE message has to be sent across first before sending any more seqnos */
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				return (0);	/* Connection got reset in call to "gtmsource_readpool" */
			if (0 < total_tr_len)
				return (total_tr_len);
			if (0 < *data_len)
				return (-1);
#endif /* for GTMSOURCE_ALWAYS_READ_FILES_STRESS, we let the source server switch back and forth between pool read and file read */
			/* Overflow, switch to READ_FILE */
			gtmsource_local->read_state = READ_FILE;
			QWASSIGN(gtmsource_save_read_jnl_seqno, read_jnl_seqno);
			gtmsource_pool2file_transition = TRUE; /* so that we read the latest gener jnl files */
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal files; journal pool "
					"overflow detected at seqno %llu [0x%llx]\n",
					gtmsource_save_read_jnl_seqno, gtmsource_save_read_jnl_seqno);
			/* CAUTION : FALL THROUGH */

		case READ_FILE:
			if (read_jnl_seqno >= jnl_seqno)
			{	/* Nothing to read. While reading from files, source server does not use write_addr to decide
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
				 * Given this, we may return with "nothing to read" even though we fell through from the
				 * READ_POOL case.
				 */
				*data_len = 0;
				return 0;
			}
			if (gtmsource_pool2file_transition /* read_pool -> read_file transition */
				 || NULL == repl_ctl_list) /* files not opened */
			{
				/* Close all the file read related structures and start afresh. The idea here is that most of the
				 * file read info might be stale 'cos there is usually a long gap between pool to file read
				 * transitions (in production environments). So, start afresh with the latest generation journal
				 * files. This will also prevent opening previous generations that may not be required.
				 */
				REPL_DPRINT1("Pool to File read transition. Closing all the stale file read info and starting "
						  "afresh\n");
				gtmsource_ctl_close();
				gtmsource_ctl_init();
				gtmsource_pool2file_transition = FALSE;
			}
			total_tr_len = gtmsource_readfiles(buff, data_len, maxbufflen, read_multpile);
			if (GTMSOURCE_SEND_NEW_TRIPLE == gtmsource_state)
				return (0); /* REPL_NEW_TRIPLE message has to be sent across first before sending any more seqnos */
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				return (0);	/* Connection got reset in call to "gtmsource_readfiles" */
			if (0 < total_tr_len)
				return (total_tr_len);
			if (0 < *data_len)
				return (-1);
			GTMASSERT;
	}
	return (-1); /* This should never get executed, added to make compiler happy */
}

/* This function can be used to only send fixed-size message types across the replication pipe.
 * This in turn uses REPL_SEND* macros but also does error checks and sets the global variable "gtmsource_state" accordingly.
 *
 *	msg		  = Pointer to the message buffer to send
 *	msgtypestr = Message name as a string to display meaningful error messages
 *	optional_seqno = Optional seqno that needs to be printed along with the message name
 */
void	gtmsource_repl_send(repl_msg_ptr_t msg, char *msgtypestr, seq_num optional_seqno)
{
	unsigned char		*msg_ptr;				/* needed for REPL_SEND_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			status;					/* needed for REPL_SEND_LOOP */
	char			err_string[1024];

	assert((REPL_MULTISITE_MSG_START > msg->type) || (REPL_PROTO_VER_MULTISITE <= jnlpool.gtmsource_local->remote_proto_ver));
	if (MAX_SEQNO != optional_seqno)
	{
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Sending %s message with seqno %llu [0x%llx]\n", msgtypestr,
			optional_seqno, optional_seqno);
	} else
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Sending %s message\n", msgtypestr);
	REPL_SEND_LOOP(gtmsource_sock_fd, msg, msg->len, FALSE, &gtmsource_poll_immediate)
	{
		gtmsource_poll_actions(FALSE);
		if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
			return;
	}
	/* Check for error status from the REPL_SEND */
	if (SS_NORMAL != status)
	{
		if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
		{
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset while sending %s. Status = %d ; %s\n",
					msgtypestr, status, STRERROR(status));
			repl_close(&gtmsource_sock_fd);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
			gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
			return;
		}
		if (EREPL_SEND == repl_errno)
		{
			SNPRINTF(err_string, SIZEOF(err_string), "Error sending %s message. "
				"Error in send : %s", msgtypestr, STRERROR(status));
			rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
		}
		if (EREPL_SELECT == repl_errno)
		{
			SNPRINTF(err_string, SIZEOF(err_string), "Error sending %s message. "
				"Error in select : %s", msgtypestr, STRERROR(status));
			rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
		}
	}
}

/* This function can be used to only receive fixed-size message types across the replication pipe.
 * This in turn uses REPL_RECV* macros but also does error checks and sets the global variable "gtmsource_state" accordingly.
 * While waiting for the input message type, this function also recognizes
 *	a) a REPL_XOFF_ACK_ME message in which case invokes "gtmsource_repl_send" to send a REPL_XOFF_ACK message type.
 *	b) a REPL_XOFF or REPL_XON message in which case ignores them as we are still in the initial handshake stage
 *		and these messages from the receiver are not relevant.
 *
 *	msg	   = Pointer to the message buffer where received message will be stored
 *	msglen	   = Length of the message to receive
 *	msgtype	   = Expected type of the message  (if received message is not of this type, the connection is closed)
 *	msgtypestr = Message name as a string to display meaningful error messages
 */
static	boolean_t	gtmsource_repl_recv(repl_msg_ptr_t msg, int4 msglen, int4 msgtype, char *msgtypestr)
{
	unsigned char		*msg_ptr;				/* needed for REPL_RECV_LOOP */
	int			torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int			status;					/* needed for REPL_RECV_LOOP */
	repl_msg_t		xoff_ack;
	char			err_string[1024];

	repl_log(gtmsource_log_fp, TRUE, FALSE, "Waiting for %s message\n", msgtypestr);
	assert((REPL_XOFF != msgtype) && (REPL_XON != msgtype) && (REPL_XOFF_ACK_ME != msgtype));
	do
	{
		REPL_RECV_LOOP(gtmsource_sock_fd, msg, msglen, FALSE, &gtmsource_poll_wait)
		{
			gtmsource_poll_actions(TRUE);
			if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
				return FALSE;
		}
		if (SS_NORMAL != status)
		{
			if (EREPL_RECV == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{	/* Connection reset */
					repl_log(gtmsource_log_fp, TRUE, TRUE,
						"Connection reset while attempting to receive %s message. Status = %d ; %s\n",
						msgtypestr, status, STRERROR(status));
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = jnlpool.gtmsource_local->gtmsource_state =
						GTMSOURCE_WAITING_FOR_CONNECTION;
					return FALSE;
				} else
				{
					SNPRINTF(err_string, SIZEOF(err_string),
							"Error receiving %s message from Receiver. Error in recv : %s",
							msgtypestr, STRERROR(status));
					rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
				}
			} else if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(err_string, SIZEOF(err_string),
						"Error receiving %s message from Receiver. Error in select : %s",
						msgtypestr, STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_string));
			}
		}
		assert(SS_NORMAL == status);
		if ((REPL_XON != msg->type) && (REPL_XOFF != msg->type))
			break;
	} while (TRUE);
	/* Check if message received is indeed of expected type */
	if (REPL_XOFF_ACK_ME == msg->type)
	{	/* Send XOFF_ACK. Anything sent before this in the replication pipe will be drained and therefore
		 * return to the caller so it can reissue the message exchange sequence right from the beginning.
		 */
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Received REPL_XOFF_ACK_ME message\n", msgtypestr);
		xoff_ack.type = REPL_XOFF_ACK;
		memcpy((uchar_ptr_t)&xoff_ack.msg[0], (uchar_ptr_t)&gtmsource_msgp->msg[0], SIZEOF(seq_num));
		xoff_ack.len = MIN_REPL_MSGLEN;
		gtmsource_repl_send((repl_msg_ptr_t)&xoff_ack, "REPL_XOFF_ACK", MAX_SEQNO);
		if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
			return FALSE;	/* "gtmsource_repl_send" did not complete */
		gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_RESTART;
		return FALSE;
	} else if (msgtype != msg->type)
	{
		assert(FALSE);
		repl_log(gtmsource_log_fp, TRUE, FALSE, "UNKNOWN msg (type = %d) received when waiting for msg (type = %d)"
							 ". Closing connection.\n", msg->type, msgtype);
		repl_close(&gtmsource_sock_fd);
		SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
		gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
		return FALSE;
	} else
	{
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Received %s message\n", msgtypestr);
		return TRUE;
	}
}

/* Given that the source server was started with compression enabled, this function checks if the receiver server was
 * also started with the decompression enabled and if so sends a compressed test message. The receiver server responds back
 * whether it is successfully able to decompress that or not. If yes, compression is enabled on the replication pipe and
 * the input parameter "*repl_zlib_cmp_level_ptr" is set to the compression level used.
 */
boolean_t	gtmsource_get_cmp_info(int4 *repl_zlib_cmp_level_ptr)
{
	repl_cmpinfo_msg_t	test_msg, solve_msg;
	char			inputdata[REPL_MSG_CMPDATALEN], cmpbuf[REPL_MSG_CMPEXPDATALEN];
	int			index, cmpret, start;
	boolean_t		cmpfail;
	uLongf			cmplen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gtm_zlib_cmp_level);
	/*************** Send REPL_CMP_TEST message ***************/
	memset(&test_msg, 0, SIZEOF(test_msg));
	test_msg.type = REPL_CMP_TEST;
	test_msg.len = REPL_MSG_CMPINFOLEN;
	test_msg.proto_ver = REPL_PROTO_VER_THIS;
	/* Fill in test data with random data. The data will be a sequence of bytes from 0 to 255. The start point though
	 * is randomly chosen using the process_id. If it is 253, the resulting sequence would be 253, 254, 255, 0, 1, 2, ...
	 */
	for (start = (process_id & REPL_MSG_CMPDATAMASK), index = 0; index < REPL_MSG_CMPDATALEN; index++)
		inputdata[index] = (start + index) % REPL_MSG_CMPDATALEN;
	/* Compress the data */
	cmplen = SIZEOF(cmpbuf);	/* initialize it to the available compressed buffer space */
	ZLIB_COMPRESS(&cmpbuf[0], cmplen, inputdata, REPL_MSG_CMPDATALEN, gtm_zlib_cmp_level, cmpret);
	switch(cmpret)
	{
		case Z_MEM_ERROR:
			assert(FALSE);
			repl_log(gtmsource_log_fp, TRUE, FALSE,
				"Out-of-memory; Error from zlib compress function before sending REPL_CMP_TEST message\n");
			break;
		case Z_BUF_ERROR:
			assert(FALSE);
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Insufficient output buffer; Error from zlib compress "
				"function before sending REPL_CMP_TEST message\n");
			break;
		case Z_STREAM_ERROR:
			/* level was incorrectly specified. Default to NO compression. */
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Compression level %d invalid; Error from compress function"
				" before sending REPL_CMP_TEST message\n", gtm_zlib_cmp_level);
			break;
	}
	if (Z_OK != cmpret)
	{
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Defaulting to NO compression\n");
		*repl_zlib_cmp_level_ptr = ZLIB_CMPLVL_NONE;	/* no compression */
		return TRUE;
	}
	if (REPL_MSG_CMPEXPDATALEN < cmplen)
	{	/* The zlib compression library expanded data more than we had allocated for. But handle code for pro */
		assert(FALSE);
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Compression of %d bytes of test data resulted in %d bytes which is"
			" more than allocated space of %d bytes\n", REPL_MSG_CMPDATALEN, cmplen, REPL_MSG_CMPEXPDATALEN);
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Defaulting to NO compression\n");
		*repl_zlib_cmp_level_ptr = ZLIB_CMPLVL_NONE;	/* no compression */
		return TRUE;
	}
	test_msg.datalen = (int4)cmplen;
	memcpy(test_msg.data, cmpbuf, test_msg.datalen);
	gtmsource_repl_send((repl_msg_ptr_t)&test_msg, "REPL_CMP_TEST", MAX_SEQNO);
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return FALSE; /* send did not succeed */
	/* for in-house testing, set up a timer that would cause assert failure if REPL_CMP_SOLVE is not received
	 * within 1 or 15 minutes, depending on whether this is a white-box test or not */
	if (TREF(gtm_environment_init))
#ifdef DEBUG
		if (gtm_white_box_test_case_enabled && (WBTEST_CMP_SOLVE_TIMEOUT == gtm_white_box_test_case_number))
			start_timer((TID)repl_cmp_solve_src_timeout, 1 * 60 * 1000, repl_cmp_solve_src_timeout, 0, NULL);
		else
#endif
			start_timer((TID)repl_cmp_solve_src_timeout, 15 * 60 * 1000, repl_cmp_solve_src_timeout, 0, NULL);
	/*************** Receive REPL_CMP_SOLVE message ***************/
	if (!gtmsource_repl_recv((repl_msg_ptr_t)&solve_msg, REPL_MSG_CMPINFOLEN, REPL_CMP_SOLVE, "REPL_CMP_SOLVE"))
	{
		if (TREF(gtm_environment_init))
			cancel_timer((TID)repl_cmp_solve_src_timeout);
		return FALSE; /* recv did not succeed */
	}
	if (TREF(gtm_environment_init))
		cancel_timer((TID)repl_cmp_solve_src_timeout);
	assert(REPL_CMP_SOLVE == solve_msg.type);
	cmpfail = FALSE;
	if (REPL_MSG_CMPDATALEN != solve_msg.datalen)
	{
		assert(REPL_RCVR_CMP_TEST_FAIL == solve_msg.datalen);
		cmpfail = TRUE;
	} else
	{	/* Check that receiver side decompression was correct */
		for (index = 0; index < REPL_MSG_CMPDATALEN; index++)
		{
			if (inputdata[index] != solve_msg.data[index])
			{
				cmpfail = FALSE;
				break;
			}
		}
	}
	if (!cmpfail)
	{
		assert(solve_msg.proto_ver == jnlpool.gtmsource_local->remote_proto_ver);
		assert(REPL_PROTO_VER_MULTISITE_CMP <= solve_msg.proto_ver);
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Receiver server was able to decompress successfully\n");
		*repl_zlib_cmp_level_ptr = gtm_zlib_cmp_level;
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Using zlib compression level %d for replication\n", gtm_zlib_cmp_level);
	} else
	{
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Receiver server could not decompress successfully\n");
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Defaulting to NO compression\n");
		*repl_zlib_cmp_level_ptr = ZLIB_CMPLVL_NONE;
	}
	return TRUE;
}

void 		repl_cmp_solve_src_timeout(void)
{
	GTMASSERT;
}

boolean_t	gtmsource_get_instance_info(boolean_t *secondary_was_rootprimary)
{
	repl_needinst_msg_t	needinst_msg;
	repl_instinfo_msg_t	instinfo_msg;
	char			print_msg[1024];
	int			status;

	/*************** Send REPL_NEED_INSTANCE_INFO message ***************/
	memset(&needinst_msg, 0, SIZEOF(needinst_msg));
	needinst_msg.type = REPL_NEED_INSTANCE_INFO;
	needinst_msg.len = MIN_REPL_MSGLEN;
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	memcpy(needinst_msg.instname, jnlpool.repl_inst_filehdr->this_instname, MAX_INSTNAME_LEN - 1);
	needinst_msg.proto_ver = REPL_PROTO_VER_THIS;
	needinst_msg.node_endianness = NODE_ENDIANNESS;
	needinst_msg.is_rootprimary = !(jnlpool.jnlpool_ctl->upd_disabled);
	gtmsource_repl_send((repl_msg_ptr_t)&needinst_msg, "REPL_NEED_INSTANCE_INFO", MAX_SEQNO);
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return FALSE; /* send did not succeed */

	/*************** Receive REPL_INSTANCE_INFO message ***************/
	if (!gtmsource_repl_recv((repl_msg_ptr_t)&instinfo_msg, MIN_REPL_MSGLEN, REPL_INSTANCE_INFO, "REPL_INSTANCE_INFO"))
		return FALSE; /* recv did not succeed */
	assert(REPL_INSTANCE_INFO == instinfo_msg.type);
	repl_log(gtmsource_log_fp, TRUE, FALSE, "Received secondary instance name is [%s]\n", instinfo_msg.instname);
	/* Check if instance name in the REPL_INSTANCE_INFO message matches that in the source server command line */
	if (STRCMP(instinfo_msg.instname, jnlpool.gtmsource_local->secondary_instname))
	{	/* Instance name obtained from the receiver does not match what was specified in the
		 * source server command line. Issue error.
		 */
		sgtm_putmsg(print_msg, VARLSTCNT(6) ERR_REPLINSTSECMTCH, 4,
			LEN_AND_STR(instinfo_msg.instname), LEN_AND_STR(jnlpool.gtmsource_local->secondary_instname));
		repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
		status = gtmsource_shutdown(TRUE, NORMAL_SHUTDOWN) - NORMAL_SHUTDOWN;
		gtmsource_exit(status);
	}
	*secondary_was_rootprimary = (boolean_t)instinfo_msg.was_rootprimary;
	assert(NULL != jnlpool.repl_inst_filehdr);
	return TRUE;
}

/* Given an input "seqno", this function locates the triple from the receiver that corresponds to "seqno - 1".
 * In addition this function also returns the index of the received triple in the secondary's replication instance file.
 * When this becomes 0, the source server knows the receiver has run out of triples.
 *
 *	seqno		--> The journal seqno that is to be searched in the instance file triple history.
 *	triple 		--> Pointer to the triple structure that is filled in with what was received.
 *	triple_num	--> Number of the triple (in the secondary's instance file) that was received
 */
boolean_t	gtmsource_get_triple_info(seq_num seqno, repl_triple *triple, int4 *triple_num)
{
	repl_needtriple_msg_t	needtriple_msg;
	repl_tripinfo1_msg_t	tripinfo1_msg;
	repl_tripinfo2_msg_t	tripinfo2_msg;
	char			err_string[1024];

	/*************** Send REPL_NEED_TRIPLE_INFO message ***************/
	memset(&needtriple_msg, 0, SIZEOF(needtriple_msg));
	needtriple_msg.type = REPL_NEED_TRIPLE_INFO;
	needtriple_msg.len = MIN_REPL_MSGLEN;
	needtriple_msg.seqno = seqno;
	gtmsource_repl_send((repl_msg_ptr_t)&needtriple_msg, "REPL_NEED_TRIPLE_INFO", seqno);
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return FALSE; /* send did not succeed */

	/*************** Receive REPL_TRIPLE_INFO1 message ***************/
	if (!gtmsource_repl_recv((repl_msg_ptr_t)&tripinfo1_msg, MIN_REPL_MSGLEN, REPL_TRIPLE_INFO1, "REPL_TRIPLE_INFO1"))
		return FALSE; /* recv did not succeed */
	assert(REPL_TRIPLE_INFO1 == tripinfo1_msg.type);

	/*************** Receive REPL_TRIPLE_INFO2 message ***************/
	if (!gtmsource_repl_recv((repl_msg_ptr_t)&tripinfo2_msg, MIN_REPL_MSGLEN, REPL_TRIPLE_INFO2, "REPL_TRIPLE_INFO2"))
		return FALSE; /* recv did not succeed */
	assert(REPL_TRIPLE_INFO2 == tripinfo2_msg.type);

	/* Check if start_seqno in TRIPLE_INFO1 and TRIPLE_INFO2 message is the same. If not something is wrong */
	if (tripinfo1_msg.start_seqno != tripinfo2_msg.start_seqno)
	{
		assert(FALSE);
		repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL_TRIPLE_INFO1 msg has start_seqno %llu [0x%llx] while "
			"corresponding REPL_TRIPLE_INFO2 msg has a different start_seqno %llu [0x%llx]. "
			"Closing connection.\n", tripinfo1_msg.start_seqno, tripinfo1_msg.start_seqno,
			tripinfo2_msg.start_seqno, tripinfo2_msg.start_seqno);
		repl_close(&gtmsource_sock_fd);
		SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
		gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
		return FALSE;
	}
	memset(triple, 0, SIZEOF(*triple));
	memcpy(triple->root_primary_instname, tripinfo1_msg.instname, MAX_INSTNAME_LEN - 1);
	triple->start_seqno = tripinfo1_msg.start_seqno;
	triple->root_primary_cycle = tripinfo2_msg.cycle;
	*triple_num = tripinfo2_msg.triple_num;
	assert(0 <= *triple_num);
	return TRUE;
}

/* This function finds the 'n'th triple in the replication instance file of this instance.
 * This is a wrapper on top of "repl_inst_triple_get" which additionally does error checking.
 * This closes the connection if it detects a REPLINSTNOHIST error.
 */
void	gtmsource_triple_get(int4 index, repl_triple *triple)
{
	unix_db_info	*udi;
	char		histdetail[256];
	int4		status;
	repl_msg_t	instnohist_msg;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	status = repl_inst_triple_get(index, triple);
	assert(0 == status);
	if (0 != status)
	{
		assert(ERR_REPLINSTNOHIST == status);	/* currently the only error returned by "repl_inst_triple_get" */
		SPRINTF(histdetail, "record number %d [0x%x]", index, index);
		gtm_putmsg(VARLSTCNT(6) ERR_REPLINSTNOHIST, 4, LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
		/* Send this error status to the receiver server before closing the connection. This way the receiver
		 * will know to shut down rather than loop back trying to reconnect. This avoids an infinite loop of
		 * connection open and closes between the source server and receiver server.
		 */
		instnohist_msg.type = REPL_INST_NOHIST;
		instnohist_msg.len = MIN_REPL_MSGLEN;
		memset(&instnohist_msg.msg[0], 0, SIZEOF(instnohist_msg.msg));
		gtmsource_repl_send((repl_msg_ptr_t)&instnohist_msg, "REPL_INST_NOHIST", MAX_SEQNO);
		/* Close the connection */
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset due to above REPLINSTNOHIST error\n");
		repl_close(&gtmsource_sock_fd);
		SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
		gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
	}
}

/* Given two triples (one each from the primary and secondary) corresponding to the seqno "jnl_seqno-1", this function compares
 * if the "root_primary_instname" and "root_primary_cycle" information in both triples are the same.
 * If so, it returns TRUE else it returns FALSE.
 */
boolean_t	gtmsource_is_triple_identical(repl_triple *remote_triple, repl_triple *local_triple, seq_num jnl_seqno)
{
	repl_log(gtmsource_log_fp, FALSE, FALSE, "On Secondary, seqno %llu [0x%llx] generated by instance name [%s] "
		"with cycle [0x%x] (%d) \n", jnl_seqno - 1, jnl_seqno - 1, remote_triple->root_primary_instname,
		remote_triple->root_primary_cycle, remote_triple->root_primary_cycle);
	repl_log(gtmsource_log_fp, FALSE, FALSE, "On Primary, seqno %llu [0x%llx] generated by instance name [%s] "
		"with cycle [0x%x] (%d)\n", jnl_seqno - 1, jnl_seqno - 1, local_triple->root_primary_instname,
		local_triple->root_primary_cycle, local_triple->root_primary_cycle);
	if (STRCMP(local_triple->root_primary_instname, remote_triple->root_primary_instname)
		|| (local_triple->root_primary_cycle != remote_triple->root_primary_cycle))
	{	/* either the root primary instance name or the cycle did not match */
		repl_log(gtmsource_log_fp, FALSE, FALSE, "Primary and Secondary have DIFFERING history records for "
			"seqno %llu [0x%llx]\n", jnl_seqno - 1, jnl_seqno - 1);
		return FALSE;
	} else
	{
		repl_log(gtmsource_log_fp, FALSE, FALSE, "Primary and Secondary have IDENTICAL history records for "
			"seqno %llu [0x%llx]\n", jnl_seqno - 1, jnl_seqno - 1);
		return TRUE;
	}
}

/* Determine the resync seqno between primary and secondary by comparing local and remote triples from the tail of the
 * respective instance files until we reach a seqno whose triple information is identical in both. The resync seqno
 * is the first seqno whose triple information was NOT identical in both. The triple history on the secondary is
 * obtained through successive calls to the function "gtmsource_get_triple_info".
 *
 * If the connection gets reset while exchanging triple information with secondary, this function returns a seqno of MAX_SEQNO.
 * The global variable "gtmsource_state" will be set to GTMSOURCE_CHANGING_MODE or GTMSOURCE_WAITING_FOR_CONNECTION and the
 * caller of this function should accordingly check for that immediately on return.
 */
seq_num	gtmsource_find_resync_seqno(
		repl_triple	*local_triple,
		int4 		local_triple_num,
		repl_triple	*remote_triple,
		int4		remote_triple_num)
{
	seq_num		max_start_seqno, local_start_seqno, remote_start_seqno;
	int4		prev_remote_triple_num = remote_triple_num;

	do
	{
		local_start_seqno = local_triple->start_seqno;
		remote_start_seqno = remote_triple->start_seqno;
		assert(local_start_seqno);
		assert(remote_start_seqno);
		max_start_seqno = MAX(local_start_seqno, remote_start_seqno);
		/* "max_start_seqno" is the largest yet known seqno whose triple info does NOT match in primary and secondary.
		 * Therefore determine the triple(s) for "max_start_seqno-1" in primary and/or secondary and compare them.
		 */
		if (1 == max_start_seqno)
		{	/* The earliest possible seqno in the primary is out of sync with that of the secondary. Stop the triple
			 * search right away and return with 1 (the smallest possible seqno) as the resync seqno.
			 */
			assert(local_start_seqno == max_start_seqno);
			assert(remote_start_seqno == max_start_seqno);
			assert(0 == local_triple_num);
			assert(0 == remote_triple_num);
			break;
		}
		if (local_start_seqno == max_start_seqno)
		{	/* Need to get the previous triple on the primary */
			local_triple_num--;
			assert(0 <= local_triple_num);
			repl_inst_ftok_sem_lock();
			gtmsource_triple_get(local_triple_num, local_triple);
			repl_inst_ftok_sem_release();
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				return MAX_SEQNO;	/* Connection got reset in "gtmsource_triple_get" due to REPLINSTNOHIST */
		}
		if (remote_start_seqno == max_start_seqno)
		{	/* Need to get the previous triple on the secondary */
			assert(1 <= prev_remote_triple_num);
			if (!gtmsource_get_triple_info(remote_start_seqno, remote_triple, &remote_triple_num))
			{
				assert((GTMSOURCE_CHANGING_MODE == gtmsource_state)
					|| (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state));
				/* Connection got reset while exchanging triple history information */
				return MAX_SEQNO;
			}
			assert(remote_triple_num == (prev_remote_triple_num - 1));
			DEBUG_ONLY(prev_remote_triple_num = remote_triple_num);
		}
		assert(local_triple->start_seqno < max_start_seqno);
		assert(remote_triple->start_seqno < max_start_seqno);
	} while (!gtmsource_is_triple_identical(remote_triple, local_triple, max_start_seqno - 1));
	repl_log(gtmsource_log_fp, TRUE, FALSE, "Resync Seqno determined is %llu [0x%llx]\n",  max_start_seqno, max_start_seqno);
	/* "max_start_seqno-1" has same triple info in both primary and secondary. Hence "max_start_seqno" is the resync seqno. */
	return max_start_seqno;
}

/* This routine sends a REPL_NEW_TRIPLE message followed by REPL_TRIPLE_INFO1 and REPL_TRIPLE_INFO2 messages containing the
 * information from the triple corresponding to the seqno "gtmsource_local->read_jnl_seqno".
 *
 * It positions the send to that triple which corresponds to "gtmsource_local->read_jnl_seqno". This is done by a call to the
 * function "gtmsource_set_next_triple_seqno".
 *
 * On return from this routine, the caller should check the value of the global variable "gtmsource_state" to see if it is
 * either of GTMSOURCE_CHANGING_MODE or GTMSOURCE_WAITING_FOR_CONNECTION and if so take appropriate action.
 */
void	gtmsource_send_new_triple(boolean_t rcvr_same_endianness)
{
	repl_triple_msg_t	newtriple_msg;
	repl_triple		triple;
	gtmsource_local_ptr_t	gtmsource_local;

	gtmsource_local = jnlpool.gtmsource_local;
	assert(gtmsource_local->send_new_triple);
	assert(gtmsource_local->read_jnl_seqno <= gtmsource_local->next_triple_seqno);
	gtmsource_set_next_triple_seqno(FALSE);
	if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
		return;	/* Connection got reset in "gtmsource_set_next_triple_seqno" due to REPLINSTNOHIST */
	/*************** Read triple (to send) from instance file first ***************/
	repl_inst_ftok_sem_lock();
	assert(1 <= gtmsource_local->next_triple_num);
	assert(gtmsource_local->read_jnl_seqno < gtmsource_local->next_triple_seqno);
	gtmsource_triple_get(gtmsource_local->next_triple_num - 1, &triple);
	repl_inst_ftok_sem_release();
	if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
	{
		assert(FALSE);
		return;	/* Connection got reset in "gtmsource_triple_get" due to REPLINSTNOHIST */
	}
	assert(gtmsource_local->read_jnl_seqno >= triple.start_seqno);
	/*************** Send REPL_NEW_TRIPLE message ***************/
	memset(&newtriple_msg, 0, SIZEOF(newtriple_msg));
	newtriple_msg.type = REPL_NEW_TRIPLE;
	newtriple_msg.len = SIZEOF(newtriple_msg);
	newtriple_msg.triplecontent.jrec_type = JRT_TRIPLE;
	if (!rcvr_same_endianness && (jnl_ver < remote_jnl_ver))
	{
		newtriple_msg.triplecontent.forwptr = GTM_BYTESWAP_24(SIZEOF(repl_triple_jnl_t));
		newtriple_msg.triplecontent.start_seqno = GTM_BYTESWAP_64(gtmsource_local->read_jnl_seqno);
		newtriple_msg.triplecontent.cycle = GTM_BYTESWAP_32(triple.root_primary_cycle);
	} else
	{
		newtriple_msg.triplecontent.forwptr = SIZEOF(repl_triple_jnl_t);
		newtriple_msg.triplecontent.start_seqno = gtmsource_local->read_jnl_seqno;
		newtriple_msg.triplecontent.cycle = triple.root_primary_cycle;
	}
	memcpy(newtriple_msg.triplecontent.instname, triple.root_primary_instname, MAX_INSTNAME_LEN - 1);
	memcpy(newtriple_msg.triplecontent.rcvd_from_instname, jnlpool.repl_inst_filehdr->this_instname, MAX_INSTNAME_LEN - 1);
	gtmsource_repl_send((repl_msg_ptr_t)&newtriple_msg, "REPL_NEW_TRIPLE", gtmsource_local->read_jnl_seqno);
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return; /* send did not succeed */
	repl_log(gtmsource_log_fp, TRUE, TRUE, "New Triple Content : Send Seqno = %llu [0x%llx] : Start Seqno = %llu [0x%llx] :"
		" Root Primary = [%s] : Cycle = [%d]\n", gtmsource_local->read_jnl_seqno, gtmsource_local->read_jnl_seqno,
		triple.start_seqno, triple.start_seqno, triple.root_primary_instname, triple.root_primary_cycle);
	gtmsource_local->send_new_triple = FALSE;
}

/* This function is invoked once for each triple that the source server goes past while sending journal records across.
 * This function sets the boundary seqno field "next_triple_seqno" to be the "start_seqno" of the next triple so the
 * source server will not send any seqnos corresponding to the next triple before sending a REPL_NEW_TRIPLE message.
 * It will set "gtmsource_local->next_triple_seqno" and "gtmsource_local->next_triple_num" to correspond to the next triple and
 * set the private copy "gtmsource_local->num_triples" to a copy of what is currently present in "repl_inst_filehdr->num_triples".
 *
 * The input variable "detect_new_triple" is set to TRUE if this function is called from "gtmsource_readfiles" or
 * "gtmsource_readpool" the moment they detect that the instance file has had a new triple since the last time this
 * source server took a copy of it in its private "gtmsource_local->num_triples". In this case, the only objective
 * is to find the start_seqno of the next triple and note that down as "gtmsource_local->next_triple_seqno".
 *
 * If the input variable "detect_new_triple" is set to FALSE, "next_triple_seqno" is set to the starting seqno of that
 * triple immediately after that triple corresponding to "gtmsource_local->read_jnl_seqno".
 *
 * This can end up closing the connection if the call to "gtmsource_triple_get" or "repl_inst_wrapper_triple_find_seqno" fails.
 */
void	gtmsource_set_next_triple_seqno(boolean_t detect_new_triple)
{
	unix_db_info		*udi;
	int4			status, next_triple_num, num_triples;
	repl_triple		next_triple, local_triple;
	gtmsource_local_ptr_t	gtmsource_local;
	repl_msg_t		instnohist_msg;

	repl_inst_ftok_sem_lock();
	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	gtmsource_local = jnlpool.gtmsource_local;
	assert((-1 == gtmsource_local->next_triple_num) || (gtmsource_local->next_triple_seqno >= gtmsource_local->read_jnl_seqno));
	next_triple_num = gtmsource_local->next_triple_num;
	num_triples = jnlpool.repl_inst_filehdr->num_triples;
	if (!detect_new_triple)
	{	/* Find the triple in the local instance file corresponding to the next seqno to be sent across
		 * i.e. "gtmsource_local->read_jnl_seqno". To do that, invoke the function "repl_inst_wrapper_triple_find_seqno"
		 * but pass one seqno more as input_seqno since the routine finds the triple for "input_seqno-1".
		 */
		assert(gtmsource_local->read_jnl_seqno <= jnlpool.jnlpool_ctl->jnl_seqno);
		status = repl_inst_wrapper_triple_find_seqno(gtmsource_local->read_jnl_seqno + 1, &local_triple, &next_triple_num);
		if (0 != status)
		{	/* Close the connection. The function call above would already have issued the error. */
			assert(ERR_REPLINSTNOHIST == status);
			/* Send this error status to the receiver server before closing the connection. This way the receiver
			 * will know to shut down rather than loop back trying to reconnect. This avoids an infinite loop of
			 * connection open and closes between the source server and receiver server.
			 */
			instnohist_msg.type = REPL_INST_NOHIST;
			instnohist_msg.len = MIN_REPL_MSGLEN;
			memset(&instnohist_msg.msg[0], 0, SIZEOF(instnohist_msg.msg));
			gtmsource_repl_send((repl_msg_ptr_t)&instnohist_msg, "REPL_INST_NOHIST", MAX_SEQNO);
			repl_inst_ftok_sem_release();
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset due to above REPLINSTNOHIST error\n");
			repl_close(&gtmsource_sock_fd);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
			return;
		}
		assert(0 <= next_triple_num);
		assert(local_triple.start_seqno <= gtmsource_local->read_jnl_seqno);
		assert(next_triple_num < num_triples);
		assert((gtmsource_local->next_triple_num != next_triple_num)
			|| (gtmsource_local->read_jnl_seqno == gtmsource_local->next_triple_seqno)
			|| (MAX_SEQNO == gtmsource_local->next_triple_seqno));
		next_triple_num++;
	} else
	{	/* A new triple got added to the instance file since we knew last. Set "next_triple_seqno" for our current
		 * triple down from its current value of MAX_SEQNO.
		 */
		assert(gtmsource_local->next_triple_seqno == MAX_SEQNO);
		assert(next_triple_num < num_triples);
		if (READ_FILE == gtmsource_local->read_state)
		{	/* It is possible that we have already read the journal records for the next
			 * read_jnl_seqno before detecting that a triple has to be sent first. In that case,
			 * the journal files may have been positioned ahead of the read_jnl_seqno for the
			 * next read. Indicate that they have to be repositioned into the past.
			 */
			gtmsource_set_lookback();
		}
	}
	if (num_triples > next_triple_num)
	{	/* Read the next triple to determine its "start_seqno" */
		gtmsource_triple_get(next_triple_num, &next_triple);
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
		{
			assert(FALSE);
			repl_inst_ftok_sem_release();
			return;	/* Connection got reset in "gtmsource_triple_get" due to REPLINSTNOHIST */
		}
		assert(next_triple.start_seqno >= gtmsource_local->read_jnl_seqno);
		gtmsource_local->next_triple_seqno = next_triple.start_seqno;
	} else
		gtmsource_local->next_triple_seqno = MAX_SEQNO;
	gtmsource_local->next_triple_num = next_triple_num;
	gtmsource_local->num_triples = num_triples;
	repl_inst_ftok_sem_release();
	assert(!udi->grabbed_ftok_sem);
}
