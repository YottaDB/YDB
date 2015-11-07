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

#if defined(__MVS__) && !defined(_ISOC99_SOURCE)
#define _ISOC99_SOURCE
#endif

#include "mdef.h"

#include "gtm_stdio.h"	/* for FILE * in repl_comm.h */
#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_inet.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_string.h"

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
#include "gtmio.h"		/* for REPL_DPRINT* macros */
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
#include "repl_inst_dump.h"		/* for "repl_dump_histinfo" prototype */
#include "gtmdbgflags.h"

#define SEND_REPL_LOGFILE_INFO(LOGFILE, LOGFILE_MSG)							\
{													\
	int		len;										\
													\
	len = repl_logfileinfo_get(LOGFILE, &LOGFILE_MSG, FALSE, gtmsource_log_fp);			\
	REPL_SEND_LOOP(gtmsource_sock_fd, &LOGFILE_MSG, len, REPL_POLL_NOWAIT)				\
	{												\
		gtmsource_poll_actions(FALSE);								\
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)						\
			return SS_NORMAL;								\
	}												\
}

GBLREF	boolean_t		gtmsource_logstats;
GBLREF	boolean_t		gtmsource_pool2file_transition;
GBLREF	boolean_t		gtmsource_received_cmp2uncmp_msg;
GBLREF	boolean_t		secondary_side_std_null_coll;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	gd_addr			*gd_header;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	int4			strm_index;
GBLREF	int			gtmsource_cmpmsgbufsiz;
GBLREF	int			gtmsource_filter;
GBLREF	int			gtmsource_log_fd;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	int			gtmsource_sock_fd;
GBLREF	int			repl_filter_bufsiz;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	repl_conn_info_t	*this_side, *remote_side;
GBLREF	repl_ctl_element	*repl_ctl_list;
GBLREF	repl_msg_ptr_t		gtmsource_cmpmsgp;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	seq_num			gtmsource_save_read_jnl_seqno;
GBLREF	seq_num			seq_num_zero;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	uint4			process_id;
GBLREF	unsigned char		*gtmsource_tcombuffp;
GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	gtmsource_options_t	gtmsource_options;

error_def(ERR_REPL2OLD);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLFTOKSEM);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_REPLINSTSECMTCH);
error_def(ERR_REPLNOXENDIAN);
error_def(ERR_REPLWARN);
error_def(ERR_SECNOTSUPPLEMENTARY);
error_def(ERR_STRMNUMMISMTCH1);
error_def(ERR_STRMNUMMISMTCH2);
error_def(ERR_TEXT);

static	unsigned char		*tcombuff, *msgbuff, *cmpmsgbuff, *filterbuff;

int gtmsource_est_conn()
{
	int			connection_attempts, alert_attempts, save_errno, status;
	char			print_msg[1024], msg_str[1024], *errmsg;
	gtmsource_local_ptr_t	gtmsource_local;
	int			send_buffsize, recv_buffsize, tcp_s_bufsize;
	int 			logging_period, logging_interval; /* logging period = soft_tries_period*logging_interval */
	int 			logging_attempts;
	sockaddr_ptr		secondary_sa;
	int			secondary_addrlen;

	gtmsource_local = jnlpool.gtmsource_local;
	assert(remote_side == &gtmsource_local->remote_side);
	remote_side->proto_ver = REPL_PROTO_VER_UNINITIALIZED;
	remote_side->endianness_known = FALSE;
	/* Connect to the secondary - use hard tries, soft tries ... */
	connection_attempts = 0;
	gtmsource_comm_init(); /* set up gtmsource_local.secondary_ai */
	repl_log(gtmsource_log_fp, TRUE, TRUE, "Connect hard tries count = %d, Connect hard tries period = %d\n",
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT],
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD]);
	do
	{
		secondary_sa = (sockaddr_ptr)(&gtmsource_local->secondary_inet_addr);
		secondary_addrlen = gtmsource_local->secondary_addrlen;
		status = gtm_connect(gtmsource_sock_fd, secondary_sa, secondary_addrlen);
		if (0 == status)
			break;
		repl_log(gtmsource_log_fp, FALSE, FALSE, "%d hard connection attempt failed : %s\n", connection_attempts + 1,
			 STRERROR(errno));
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
			secondary_sa = (sockaddr_ptr)(&gtmsource_local->secondary_inet_addr);
			secondary_addrlen = gtmsource_local->secondary_addrlen;
			status = gtm_connect(gtmsource_sock_fd, secondary_sa, secondary_addrlen);
			if (0 == status)
				break;
			repl_close(&gtmsource_sock_fd);
			if (0 == (connection_attempts + 1) % logging_interval)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "%d soft connection attempt failed : %s\n",
					 connection_attempts + 1, STRERROR(errno));
				logging_attempts++;
			}
			LONG_SLEEP(gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			gtmsource_comm_init();
			connection_attempts++;
			if (0 == (connection_attempts % logging_interval) && 0 == (logging_attempts % alert_attempts))
			{ 	/* Log ALERT message */
				SNPRINTF(msg_str, SIZEOF(msg_str),
					 "GTM Replication Source Server : Could not connect to secondary in %d seconds\n",
					connection_attempts *
					gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
				sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(msg_str));
				repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
				gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLWARN", print_msg);
			}
			if (logging_period <= REPL_MAX_LOG_PERIOD)
			{	 /*the maximum real_period can reach 2*REPL_MAX_LOG_PERIOD)*/
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
	{
		errmsg = STRERROR(status);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Error getting socket recv buffsize"),
				ERR_TEXT, 2, LEN_AND_STR(errmsg));
	}
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

int gtmsource_alloc_msgbuff(int maxbuffsize, boolean_t discard_oldbuff)
{	/* Allocate message buffer */
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
			if (!discard_oldbuff)
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

/* Receive starting jnl_seqno for (re)starting transmission */
int gtmsource_recv_restart(seq_num *recvd_jnl_seqno, int *msg_type, int *start_flags)
{
	boolean_t			msg_is_cross_endian, log_waitmsg;
	fd_set				input_fds;
	repl_msg_t			msg;
	repl_logfile_info_msg_t		logfile_msg;
	unsigned char			*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int				tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int				torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int				status;					/* needed for REPL_{SEND,RECV}_LOOP */
	uint4				remaining_len, len;
	unsigned char			seq_num_str[32], *seq_num_ptr, *buffp;
	repl_msg_t			xoff_ack;
#	ifdef DEBUG
	boolean_t			remote_side_endianness_known;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	status = SS_NORMAL;
	assert(remote_side == &jnlpool.gtmsource_local->remote_side);
	DEBUG_ONLY(*msg_type = -1);
	for (log_waitmsg = TRUE; SS_NORMAL == status; )
	{
		if (log_waitmsg)
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Waiting for REPL_START_JNL_SEQNO or REPL_FETCH_RESYNC message\n");
		REPL_RECV_LOOP(gtmsource_sock_fd, &msg, MIN_REPL_MSGLEN, REPL_POLL_WAIT)
		{
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
		}
		DEBUG_ONLY(remote_side_endianness_known = remote_side->endianness_known);
		if (SS_NORMAL != status)
			break;
		if (REPL_LOGFILE_INFO == msg.type) /* No need to endian convert as the receiver converts this to our native fmt */
		{	/* We received a REPL_START_JNL_SEQNO/REPL_FETCH_RESYNC and coming through the loop again
			 * to receive REPL_LOGFILE_INFO. At this point, we should have already established the endianness
			 * of the remote side and even if the remote side is of different endianness, we are going to interpret the
			 * message without endian conversion because the Receiver Server, from REPL_PROTO_VER_SUPPLEMENTARY
			 * onwards, always endian converts the message intended for the Source Server
			 */
			assert(remote_side->endianness_known);
			assert(REPL_PROTO_VER_REMOTE_LOGPATH <= remote_side->proto_ver);
			assert(-1 != *msg_type);
			buffp = (unsigned char *)&logfile_msg;
			/* First copy what we already received */
			memcpy(buffp, &msg, MIN_REPL_MSGLEN);
			assert((logfile_msg.fullpath_len > MIN_REPL_MSGLEN)
					&& logfile_msg.fullpath_len < REPL_LOGFILE_PATH_MAX);
			/* Now receive the rest of the message */
			buffp += MIN_REPL_MSGLEN;
			remaining_len = msg.len - MIN_REPL_MSGLEN;
			REPL_RECV_LOOP(gtmsource_sock_fd, buffp, remaining_len, REPL_POLL_WAIT)
			{
				gtmsource_poll_actions(FALSE);
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return SS_NORMAL;
			}
			if (SS_NORMAL != status)
				return status;
			assert(REPL_PROTO_VER_REMOTE_LOGPATH <= logfile_msg.proto_ver);
			assert(logfile_msg.proto_ver == remote_side->proto_ver);
			assert('\0' == logfile_msg.fullpath[logfile_msg.fullpath_len - 1]);
			if (REPL_FETCH_RESYNC == *msg_type)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Remote side rollback path is %s; Rollback PID = %d\n",
						logfile_msg.fullpath, logfile_msg.pid);
			}
			else
			{
				assert(REPL_START_JNL_SEQNO == *msg_type);
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Remote side receiver log file path is %s; "
						"Receiver Server PID = %d\n", logfile_msg.fullpath, logfile_msg.pid);
			}
			/* Now that we've received REPL_LOGFILE_INFO message from the other side, handshake is complete. */
			return SS_NORMAL;
		}
		/* If endianness of other side is not yet known, determine that now by seeing if the msg.len is
		 * greater than expected. If it is, convert it and see if it is now what we expect. If it is,
		 * then the other system is of opposite endianness. Note: We would normally use msg.type since
		 * it is effectively an enum and we control by adding new messages. But, REPL_START_JNL_SEQNO
		 * is lucky number zero which means it is identical on systems of either endianness.
		 *
		 * If endianness of other side is not yet known, determine that from the message length as we
		 * expect it to be MIN_REPL_MSGLEN. There is one exception though. If a pre-V55000 receiver sends
		 * a REPL_XOFF_ACK_ME message, it could send it in the receiver's native-endian or cross-endian
		 * format (depending on how its global variable "src_node_same_endianness" is initialized). This
		 * bug in the receiver server logic is fixed V55000 onwards (proto_ver is REPL_PROTO_VER_SUPPLEMENTARY).
		 * Therefore, in this case, we cannot use the endianness of the REPL_XOFF_ACK_ME message to determine
		 * the endianness of the connection. In this case, wait for the next non-REPL_XOFF_ACK_ME message
		 * to determine the connection endianness. Handle this exception case correctly.
		 *
		 * If on the other hand, we know the endianness of the other side, we still cannot guarantee which
		 * endianness a REPL_XOFF_ACK_ME message is sent in (in pre-V55000 versions for example in V53004A where
		 * it is sent in receiver native endian format whereas in V54002B it is sent in source native
		 * endian format). So better be safe on the source side and handle those cases like we did when
		 * we did not know the endianness of the remote side.
		 *
		 * The below check works as all messages we expect at this point have a fixed length of MIN_REPL_MSGLEN.
		 */
		msg_is_cross_endian = (((unsigned)MIN_REPL_MSGLEN < (unsigned)msg.len)
					&& ((unsigned)MIN_REPL_MSGLEN == GTM_BYTESWAP_32((unsigned)msg.len)));
		if (msg_is_cross_endian)
		{
			msg.type = GTM_BYTESWAP_32(msg.type);
			msg.len = GTM_BYTESWAP_32(msg.len);
		}
		assert(msg.type == REPL_START_JNL_SEQNO || msg.type == REPL_FETCH_RESYNC || msg.type == REPL_XOFF_ACK_ME);
		assert(MIN_REPL_MSGLEN == msg.len);
		/* If we dont yet know the endianness of the other side and the input message is not a REPL_XOFF_ACK_ME
		 * we can decide the endianness of the receiver side by the endianness of the input message.
		 * REPL_XOFF_ACK_ME is an exception due to its handling by pre-V5500 versions (described in comments above).
		 */
		if (!remote_side->endianness_known && (REPL_XOFF_ACK_ME != msg.type))
		{
			remote_side->endianness_known = TRUE;
			remote_side->cross_endian = msg_is_cross_endian;
			if (remote_side->cross_endian)
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Source and Receiver sides have opposite "
					"endianness\n");
			else
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Source and Receiver sides have same endianness\n");
		}
		/* We only expect REPL_START_JNL_SEQNO, REPL_LOGFILE_INFO and REPL_XOFF_ACK_ME messages to be sent once the
		 * endianness of the remote side has been determined. We dont expect the REPL_FETCH_RESYNC message to be
		 * ever sent in the middle of a handshake (i.e. after the remote side endianness has been determined).
		 * Assert that. The logic that sets "msg_is_cross_endian" relies on this. If this assert fails, the logic
		 * has to change.
		 */
		assert((REPL_FETCH_RESYNC != msg.type) || !remote_side_endianness_known);
		*msg_type = msg.type;
		*start_flags = START_FLAG_NONE;
		memcpy((uchar_ptr_t)recvd_jnl_seqno, (uchar_ptr_t)&msg.msg[0], SIZEOF(seq_num));
		if (REPL_START_JNL_SEQNO == msg.type)
		{
			if (msg_is_cross_endian)
				*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Received REPL_START_JNL_SEQNO message with SEQNO "
				"%llu [0x%llx]\n", INT8_PRINT(*recvd_jnl_seqno), INT8_PRINT(*recvd_jnl_seqno));
			if (msg_is_cross_endian)
				((repl_start_msg_ptr_t)&msg)->start_flags =
					GTM_BYTESWAP_32(((repl_start_msg_ptr_t)&msg)->start_flags);
			*start_flags = ((repl_start_msg_ptr_t)&msg)->start_flags;
			assert(!msg_is_cross_endian || (NODE_ENDIANNESS != ((repl_start_msg_ptr_t)&msg)->node_endianness));
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
			}
			/* Determine the protocol version of the receiver side. That information is encoded in the
			 * "proto_ver" field of the message from V51 onwards but to differentiate V50 vs V51 we need
			 * to check if the START_FLAG_VERSION_INFO bit is set in start_flags. If not the protocol is V50.
			 * V51 implies support for multi-site while V50 implies dual-site configuration only.
			 */
			if (*start_flags & START_FLAG_VERSION_INFO)
			{
				assert(REPL_PROTO_VER_DUALSITE != ((repl_start_msg_ptr_t)&msg)->proto_ver);
				remote_side->is_supplementary = ((repl_start_msg_ptr_t)&msg)->is_supplementary;
				remote_side->proto_ver = ((repl_start_msg_ptr_t)&msg)->proto_ver;
			} else
			{	/* Issue REPL2OLD error because receiver is dual-site */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPL2OLD, 4, LEN_AND_STR(UNKNOWN_INSTNAME),
					LEN_AND_STR(jnlpool.repl_inst_filehdr->inst_info.this_instname));
			}
			assert(*start_flags & START_FLAG_HASINFO); /* V4.2+ versions have jnl ver in the start msg */
			remote_side->jnl_ver = ((repl_start_msg_ptr_t)&msg)->jnl_ver;
			REPL_DPRINT3("Local jnl ver is octal %o, remote jnl ver is octal %o\n",
				this_side->jnl_ver, remote_side->jnl_ver);
			repl_check_jnlver_compat(!remote_side->cross_endian);
			assert(remote_side->jnl_ver > V15_JNL_VER || 0 == (*start_flags & START_FLAG_COLL_M));
			if (remote_side->jnl_ver <= V15_JNL_VER)
				*start_flags &= ~START_FLAG_COLL_M; /* zap it for pro, just in case */
			remote_side->is_std_null_coll = (*start_flags & START_FLAG_COLL_M) ? TRUE : FALSE;
			assert((remote_side->jnl_ver >= V19_JNL_VER) || (0 == (*start_flags & START_FLAG_TRIGGER_SUPPORT)));
			if (remote_side->jnl_ver < V19_JNL_VER)
				*start_flags &= ~START_FLAG_TRIGGER_SUPPORT; /* zap it for pro, just in case */
			remote_side->trigger_supported = (*start_flags & START_FLAG_TRIGGER_SUPPORT) ? TRUE : FALSE;
#				ifdef GTM_TRIGGER
			if (!remote_side->trigger_supported)
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Warning : Secondary does not support GT.M "
					"database triggers. #t updates on primary will not be replicated\n");
#				endif
			/* remote_side->null_subs_xform is initialized later in function "gtmsource_process" */
			(TREF(replgbl)).trig_replic_warning_issued = FALSE;
			if (REPL_PROTO_VER_REMOTE_LOGPATH > remote_side->proto_ver)
				return SS_NORMAL; /* Remote side doesn't support REPL_LOGFILE_INFO message */
			SEND_REPL_LOGFILE_INFO(jnlpool.gtmsource_local->log_file, logfile_msg);
			log_waitmsg = FALSE;
		} else if (REPL_FETCH_RESYNC == msg.type)
		{	/* Determine the protocol version of the receiver side.
			 * Take care to set the flush parameter in repl_log calls below to FALSE until at least the
			 * first message gets sent back. This is so the fetchresync rollback on the other side does
			 * not timeout before receiving a response. */
			remote_side->proto_ver = (RECAST(repl_resync_msg_ptr_t)&msg)->proto_ver;
			remote_side->is_supplementary = (RECAST(repl_resync_msg_ptr_t)&msg)->is_supplementary;
			/* The following fields dont need to be initialized since they are needed (for internal filter
			 * transformations) only if we send journal records across. REPL_FETCH_RESYNC causes only
			 * protocol messages to be exchanged (no journal records).
			 *	remote_side->jnl_ver = ...
			 *	remote_side->is_std_null_coll = ...
			 *	remote_side->trigger_supported = ...
			 *	remote_side->null_subs_xform = ...
			 */
			if (msg_is_cross_endian)
				*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Received REPL_FETCH_RESYNC message with SEQNO "
				"%llu [0x%llx]\n", INT8_PRINT(*recvd_jnl_seqno), INT8_PRINT(*recvd_jnl_seqno));
			if (REPL_PROTO_VER_REMOTE_LOGPATH > remote_side->proto_ver)
				return SS_NORMAL; /* Remote side doesn't support REPL_LOGFILE_INFO message */
			SEND_REPL_LOGFILE_INFO(jnlpool.gtmsource_local->log_file, logfile_msg);
			log_waitmsg = FALSE;
		} else if (REPL_XOFF_ACK_ME == msg.type)
		{
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Received REPL_XOFF_ACK_ME message. "
								"Possible crash/shutdown of update process\n");
			/* Send XOFF_ACK */
			xoff_ack.type = REPL_XOFF_ACK;
			if (msg_is_cross_endian)
				*recvd_jnl_seqno = GTM_BYTESWAP_64(*recvd_jnl_seqno);
			memcpy((uchar_ptr_t)&xoff_ack.msg[0], (uchar_ptr_t)recvd_jnl_seqno, SIZEOF(seq_num));
			xoff_ack.len = MIN_REPL_MSGLEN;
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Sending REPL_XOFF_ACK message\n");
			REPL_SEND_LOOP(gtmsource_sock_fd, &xoff_ack, xoff_ack.len, REPL_POLL_NOWAIT)
			{
				gtmsource_poll_actions(FALSE);
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
			}
			log_waitmsg = TRUE;	/* Wait for REPL_START_JNL_SEQNO or REPL_FETCH_RESYNC */
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
	assert(recvd_jnl_seqno <= jctl->jnl_seqno);
	cur_read_jnl_seqno = gtmsource_local->read_jnl_seqno;
	if (recvd_jnl_seqno > cur_read_jnl_seqno)
	{	/* The secondary is requesting a seqno higher than what we last remember having sent but yet it is in sync with us
		 * upto seqno "recvd_jnl_seqno" as otherwise the caller would have determined it is out of sync and not even call
		 * us. To illustrate an example of how this could happen, consider an INSTA->INSTB replication and INSTA->INSTC
		 * replication going on. Lets say INSTA's journal sequence number is at 100. INSTB is at 60 and INSTC is at 30.
		 * This means, last sequence number sent by INSTA to INSTB is 60 and to INSTC is 30. Now, lets say INSTA is shutdown
		 * and INSTB comes up as the primary to INSTC and starts replicating the 30 updates thereby bringing both INSTB and
		 * INSTC sequence number to 60. Now, if INSTA comes backup again as primary against INSTC, we will have a case where
		 * gtmsource_local->read_jnl_seqno as 30, but recvd_jnl_seqno as 60. This means that we are going to bump
		 * "gtmsource_local->read_jnl_seqno" up to the received seqno (in the later call to "gtmsource_flush_fh") without
		 * knowing how many bytes of transaction data that meant (to correspondingly bump up "gtmsource_local->read_addr").
		 * The only case where we dont care to maintain "read_addr" is if we are in READ_FILE mode AND the current
		 * read_jnl_seqno and the received seqno is both lesser than or equal to "gtmsource_save_read_jnl_seqno". Except
		 * for that case, we need to reset "gtmsource_save_read_jnl_seqno" to correspond to the current jnl seqno.
		 */
		if ((READ_FILE != gtmsource_local->read_state) || (recvd_jnl_seqno > gtmsource_save_read_jnl_seqno))
		{
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
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
			if ((prev_tr_size <= cur_read_addr) &&
				jnlpool_hasnt_overflowed(jctl, jnlpool_size, cur_read_addr - prev_tr_size))
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
					"does not contain transaction %llu [0x%llx]\n", recvd_jnl_seqno, recvd_jnl_seqno);
			gtmsource_pool2file_transition = TRUE;
		}
	} else /* read_state is READ_FILE and requesting a sequence number less than or equal to read_jnl_seqno */
	{
		assert(cur_read_jnl_seqno == gtmsource_local->read_jnl_seqno);
		if (cur_read_jnl_seqno > gtmsource_save_read_jnl_seqno)
			gtmsource_save_read_jnl_seqno = cur_read_jnl_seqno;
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
	/* Finally set "gtmsource_local->read_jnl_seqno" to be "recvd_jnl_seqno" and flush changes to instance file on disk */
	gtmsource_flush_fh(recvd_jnl_seqno);
	assert(GTMSOURCE_HANDLE_ONLN_RLBK != gtmsource_state);
	return (SS_NORMAL);
}

int gtmsource_get_jnlrecs(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple)
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
	GTMDBGFLAGS_NOFREQ_ONLY(GTMSOURCE_FORCE_READ_FILE_MODE, gtmsource_local->read_state = READ_FILE);
	switch(gtmsource_local->read_state)
	{
		case READ_POOL:
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
			total_tr_len = gtmsource_readpool(buff, data_len, maxbufflen, read_multiple, write_addr);
			if (GTMSOURCE_SEND_NEW_HISTINFO == gtmsource_state)
				return (0); /* need to send REPL_HISTREC message before sending any more seqnos */
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				return (0);	/* Connection got reset in call to "gtmsource_readpool" */
			if (0 < total_tr_len)
				return (total_tr_len);
			if (0 < *data_len)
				return (-1);
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
			total_tr_len = gtmsource_readfiles(buff, data_len, maxbufflen, read_multiple);
			if (GTMSOURCE_SEND_NEW_HISTINFO == gtmsource_state)
				return (0); /* need to send REPL_HISTREC message before sending any more seqnos */
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
 *	msg		= Pointer to the message buffer to send
 *	msgtypestr      = Message name as a string to display meaningful error messages
 *	optional_seqno = Optional seqno that needs to be printed along with the message name
 */
void	gtmsource_repl_send(repl_msg_ptr_t msg, char *msgtypestr, seq_num optional_seqno, int4 optional_strm_num)
{
	unsigned char		*msg_ptr;				/* needed for REPL_SEND_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			status;					/* needed for REPL_SEND_LOOP */
	char			err_string[1024];

	assert((REPL_MULTISITE_MSG_START > msg->type) || (REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver));
	if (MAX_SEQNO != optional_seqno)
	{
		if (INVALID_SUPPL_STRM == optional_strm_num)
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Sending %s message with seqno %llu [0x%llx]\n", msgtypestr,
				optional_seqno, optional_seqno);
		else
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Sending %s message with seqno %llu [0x%llx] for Stream # %2d\n",
				msgtypestr, optional_seqno, optional_seqno, optional_strm_num);
	} else
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Sending %s message\n", msgtypestr);
	REPL_SEND_LOOP(gtmsource_sock_fd, msg, msg->len, REPL_POLL_NOWAIT)
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
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
		}
		if (EREPL_SELECT == repl_errno)
		{
			SNPRINTF(err_string, SIZEOF(err_string), "Error sending %s message. "
				"Error in select : %s", msgtypestr, STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(err_string));
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
	unsigned char		*buff;
	int4			bufflen;

	repl_log(gtmsource_log_fp, TRUE, FALSE, "Waiting for %s message\n", msgtypestr);
	assert((REPL_XOFF != msgtype) && (REPL_XON != msgtype) && (REPL_XOFF_ACK_ME != msgtype));
	buff = (unsigned char *)msg;
	bufflen = msglen;
	do
	{	/* Note that "bufflen" could potentially be > 32-byte (MIN_REPL_MSGLEN) the length of most replication
		 * messages, in case we are expecting to receive a REPL_CMP_SOLVE message. In that case, it is possible
		 * that while waiting for the 512 byte or so REPL_CMP_SOLVE message, we get a 32-byte REPL_XOFF_ACK_ME
		 * message. In this case, we should break out of the REPL_RECV_LOOP as otherwise we would be eternally
		 * waiting for a never-to-come REPL_CMP_SOLVE message. This is in fact a general issue with any REPL_RECV_LOOP
		 * code that passes a 3rd parameter > MIN_REPL_MSGLEN. All such usages need a check inside the body of the
		 * loop to account for a REPL_XOFF_ACK_ME and if so break.
		 */
		REPL_RECV_LOOP(gtmsource_sock_fd, buff, bufflen, REPL_POLL_WAIT)
		{
			gtmsource_poll_actions(TRUE);
			if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
				return FALSE;
			/* Check if we received an XOFF_ACK_ME message completely. If yes, we can safely break out of the
			 * loop without receiving the originally intended message (as the receiver is going to drain away all
			 * the stuff in the replication pipe anyways and reinitiate a fresh handshake). This way we dont hang
			 * eternally waiting for a never-arriving originally intended message.
			 */
			if ((MIN_REPL_MSGLEN <= recvd_len) && (REPL_XOFF_ACK_ME == msg->type))
				break;
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
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
							 LEN_AND_STR(err_string));
				}
			} else if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(err_string, SIZEOF(err_string),
						"Error receiving %s message from Receiver. Error in select : %s",
						msgtypestr, STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						 LEN_AND_STR(err_string));
			}
		}
		assert(SS_NORMAL == status);
		if ((REPL_XON != msg->type) && (REPL_XOFF != msg->type))
			break;
		/* Strip the "msg" buffer of the XON or XOFF message and redo the loop until "msglen" bytes of the expected
		 * msgtype are obtained. If "msglen" matches the XON or XOFF message length, then we can discard the ENTIRE message
		 * and redo the loop without having to strip a "partial" msg buffer. If "msglen" is LESSER than a complete XON or
		 * XOFF message, then we have to finish reading the entire XON/XOFF message and then redo the loop. But we
		 * expect all callers to set "msglen" to at least the XON/XOFF message length. Assert accordingly.
		 * Note that the below logic works even if more than one XON/XOFF messages get sent before the expected message.
		 * For every XON/XOFF message, we will do one memmove and go back in the loop to read MIN_REPL_MSGLEN-more bytes.
		 */
		assert(MIN_REPL_MSGLEN == msg->len);
		assert(MIN_REPL_MSGLEN <= msglen);
		bufflen = msg->len;
		if (msglen != bufflen)
		{
			memmove(msg, (uchar_ptr_t)msg + bufflen, msglen - bufflen);
			buff = (uchar_ptr_t)msg + msglen - bufflen;
		}
	} while (TRUE);
	/* Check if message received is indeed of expected type */
	assert(remote_side->endianness_known); /* so we ensure msg->type we read below is accurate and not cross-endian format */
	if (REPL_XOFF_ACK_ME == msg->type)
	{	/* Send XOFF_ACK. Anything sent before this in the replication pipe will be drained and therefore
		 * return to the caller so it can reissue the message exchange sequence right from the beginning.
		 */
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Received REPL_XOFF_ACK_ME message\n", msgtypestr);
		xoff_ack.type = REPL_XOFF_ACK;
		memcpy((uchar_ptr_t)&xoff_ack.msg[0], (uchar_ptr_t)&gtmsource_msgp->msg[0], SIZEOF(seq_num));
		xoff_ack.len = MIN_REPL_MSGLEN;
		gtmsource_repl_send((repl_msg_ptr_t)&xoff_ack, "REPL_XOFF_ACK", MAX_SEQNO, INVALID_SUPPL_STRM);
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
	gtmsource_repl_send((repl_msg_ptr_t)&test_msg, "REPL_CMP_TEST", MAX_SEQNO, INVALID_SUPPL_STRM);
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return FALSE; /* send did not succeed */
	/* for in-house testing, set up a timer that would cause assert failure if REPL_CMP_SOLVE is not received
	 * within 1 or 15 minutes, depending on whether this is a white-box test or not */
#	ifdef REPL_CMP_SOLVE_TESTING
	if (TREF(gtm_environment_init))
			start_timer((TID)repl_cmp_solve_src_timeout, 15 * 60 * 1000, repl_cmp_solve_src_timeout, 0, NULL);
#	endif
	/*************** Receive REPL_CMP_SOLVE message ***************/
	if (!gtmsource_repl_recv((repl_msg_ptr_t)&solve_msg, REPL_MSG_CMPINFOLEN, REPL_CMP_SOLVE, "REPL_CMP_SOLVE"))
	{
#		ifdef REPL_CMP_SOLVE_TESTING
		if (TREF(gtm_environment_init))
			cancel_timer((TID)repl_cmp_solve_src_timeout);
#		endif
		return FALSE; /* recv did not succeed */
	}
#	ifdef REPL_CMP_SOLVE_TESTING
	if (TREF(gtm_environment_init))
		cancel_timer((TID)repl_cmp_solve_src_timeout);
#	endif
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
		assert(solve_msg.proto_ver == remote_side->proto_ver);
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

/* Note: Do NOT reset input parameter "strm_jnl_seqno" unless receiver instance is supplementary and root primary */
boolean_t	gtmsource_get_instance_info(boolean_t *secondary_was_rootprimary, seq_num *strm_jnl_seqno)
{
	char			print_msg[1024];
	gtmsource_local_ptr_t	gtmsource_local;
	int			status;
	repl_instinfo_msg_t	instinfo_msg;
	repl_needinst_msg_t	needinst_msg;
	repl_old_instinfo_msg_t	old_instinfo_msg;
	repl_old_needinst_msg_t	old_needinst_msg;

	gtmsource_local = jnlpool.gtmsource_local;
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
	if (REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver)
	{	/* Use pre-supplementary protocol to communicate */
		/*************** Send REPL_OLD_NEED_INSTANCE_INFO message ***************/
		memset(&old_needinst_msg, 0, SIZEOF(old_needinst_msg));
		old_needinst_msg.type = REPL_OLD_NEED_INSTANCE_INFO;
		old_needinst_msg.len = MIN_REPL_MSGLEN;
		memcpy(old_needinst_msg.instname, jnlpool.repl_inst_filehdr->inst_info.this_instname, MAX_INSTNAME_LEN - 1);
		old_needinst_msg.proto_ver = REPL_PROTO_VER_THIS;
		old_needinst_msg.node_endianness = NODE_ENDIANNESS;
		old_needinst_msg.is_rootprimary = !(jnlpool.jnlpool_ctl->upd_disabled);
		gtmsource_repl_send((repl_msg_ptr_t)&old_needinst_msg, "REPL_OLD_NEED_INSTANCE_INFO",
										MAX_SEQNO, INVALID_SUPPL_STRM);
		if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
			return FALSE; /* send did not succeed */

		/*************** Receive REPL_OLD_INSTANCE_INFO message ***************/
		if (!gtmsource_repl_recv((repl_msg_ptr_t)&old_instinfo_msg, MIN_REPL_MSGLEN,
						REPL_OLD_INSTANCE_INFO, "REPL_OLD_INSTANCE_INFO"))
			return FALSE; /* recv did not succeed */
		assert(REPL_OLD_INSTANCE_INFO == old_instinfo_msg.type);
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Received secondary instance name is [%s]\n", old_instinfo_msg.instname);
		if (jnlpool.repl_inst_filehdr->is_supplementary)
		{	/* Issue REPL2OLD error because this is a supplementary instance and remote side runs
			 * on a GT.M version that does not understand the supplementary protocol */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPL2OLD, 4, LEN_AND_STR(old_instinfo_msg.instname),
				LEN_AND_STR(jnlpool.repl_inst_filehdr->inst_info.this_instname));
		}
		/* Check if instance name in the REPL_OLD_INSTANCE_INFO message matches that in the source server command line */
		if (STRCMP(old_instinfo_msg.instname, jnlpool.gtmsource_local->secondary_instname))
		{	/* Instance name obtained from the receiver does not match what was specified in the
			 * source server command line. Issue error.
			 */
			sgtm_putmsg(print_msg, VARLSTCNT(6) ERR_REPLINSTSECMTCH, 4,
				LEN_AND_STR(old_instinfo_msg.instname), LEN_AND_STR(jnlpool.gtmsource_local->secondary_instname));
			repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
			status = gtmsource_shutdown(TRUE, NORMAL_SHUTDOWN) - NORMAL_SHUTDOWN;
			gtmsource_exit(status);
		}
		*secondary_was_rootprimary = (boolean_t)old_instinfo_msg.was_rootprimary;
	} else
	{	/* Use supplementary protocol to communicate */
		/*************** Send REPL_NEED_INSTINFO message ***************/
		memset(&needinst_msg, 0, SIZEOF(needinst_msg));
		needinst_msg.type = REPL_NEED_INSTINFO;
		needinst_msg.len = SIZEOF(needinst_msg);
		memcpy(needinst_msg.instname, jnlpool.repl_inst_filehdr->inst_info.this_instname, MAX_INSTNAME_LEN - 1);
		needinst_msg.lms_group_info = jnlpool.repl_inst_filehdr->lms_group_info;
		/* Need to byteswap a few multi-byte fields to take into account the receiver endianness */
		assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
		if (remote_side->cross_endian && (this_side->jnl_ver < remote_side->jnl_ver))
			ENDIAN_CONVERT_REPL_INST_UUID(&needinst_msg.lms_group_info);
		needinst_msg.proto_ver = REPL_PROTO_VER_THIS;
		needinst_msg.is_rootprimary = !(jnlpool.jnlpool_ctl->upd_disabled);
		needinst_msg.is_supplementary = jnlpool.repl_inst_filehdr->is_supplementary;
		gtmsource_repl_send((repl_msg_ptr_t)&needinst_msg, "REPL_NEED_INSTINFO", MAX_SEQNO, INVALID_SUPPL_STRM);
		if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
			return FALSE; /* send did not succeed */

		/*************** Receive REPL_INSTINFO message ***************/
		if (!gtmsource_repl_recv((repl_msg_ptr_t)&instinfo_msg, SIZEOF(repl_instinfo_msg_t),
										REPL_INSTINFO, "REPL_INSTINFO"))
			return FALSE; /* recv did not succeed */
		assert(REPL_INSTINFO == instinfo_msg.type);
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Received secondary instance name is [%s]\n", instinfo_msg.instname);
		if (!remote_side->is_supplementary)
		{	/* Remote side is non-supplementary */
			if (jnlpool.repl_inst_filehdr->is_supplementary)
			{	/* Issue SECNOTSUPPLEMENTARY error because this is a supplementary primary and secondary
				 * is not a supplementary instance.
				 */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SECNOTSUPPLEMENTARY, 4,
					LEN_AND_STR(jnlpool.repl_inst_filehdr->inst_info.this_instname),
					LEN_AND_STR(instinfo_msg.instname));
			}
		} else if (!jnlpool.repl_inst_filehdr->is_supplementary)
		{	/* Remote side is supplementary and Local side is non-supplementary.
			 * The REPL_INSTINFO message would have a non-zero "strm_jnl_seqno" field.
			 * Pass it back on to the caller.
			 */
			assert(instinfo_msg.strm_jnl_seqno);
			assert(0 == GET_STRM_INDEX(instinfo_msg.strm_jnl_seqno));
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Received Stream Seqno = %llu [0x%llx]\n",
				instinfo_msg.strm_jnl_seqno, instinfo_msg.strm_jnl_seqno);
			*strm_jnl_seqno = instinfo_msg.strm_jnl_seqno;
		}
		/* Check if instance name in the REPL_INSTINFO message matches that in the source server command line */
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
	}
	return TRUE;
}

/* Given an input "seqno", this function locates the histinfo record from the receiver that corresponds to "seqno - 1"
 *
 *	seqno		--> The journal seqno that is to be searched in the instance file history.
 *	histinfo 	--> Pointer to the histinfo structure that is filled in with what was received.
 */
boolean_t	gtmsource_get_remote_histinfo(seq_num seqno, repl_histinfo *histinfo)
{
	char			err_string[1024];
	repl_histinfo1_msg_t	histinfo1_msg;
	repl_histinfo2_msg_t	histinfo2_msg;
	repl_histinfo_msg_t	histinfo_msg;
	repl_needhistinfo_msg_t	needhistinfo_msg;

	/*************** Send REPL_NEED_HISTINFO (formerly REPL_NEED_TRIPLE_INFO) message ***************/
	memset(&needhistinfo_msg, 0, SIZEOF(needhistinfo_msg));
	assert(SIZEOF(needhistinfo_msg) == MIN_REPL_MSGLEN);
	needhistinfo_msg.type = REPL_NEED_HISTINFO;
	needhistinfo_msg.len = MIN_REPL_MSGLEN;
	needhistinfo_msg.seqno = seqno;
	needhistinfo_msg.strm_num = strm_index;
	needhistinfo_msg.histinfo_num = INVALID_HISTINFO_NUM;
	gtmsource_repl_send((repl_msg_ptr_t)&needhistinfo_msg, "REPL_NEED_HISTINFO", seqno, needhistinfo_msg.strm_num);
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return FALSE; /* send did not succeed */
	if (REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver)
	{	/* Remote side does not support supplementary protocol. Use older protocol messages to communicate. */
		assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);

		/*************** Receive REPL_OLD_TRIPLEINFO1 message ***************/
		if (!gtmsource_repl_recv((repl_msg_ptr_t)&histinfo1_msg, MIN_REPL_MSGLEN,
							REPL_OLD_TRIPLEINFO1, "REPL_OLD_TRIPLEINFO1"))
			return FALSE; /* recv did not succeed */
		assert(REPL_OLD_TRIPLEINFO1 == histinfo1_msg.type);

		/*************** Receive REPL_OLD_TRIPLEINFO2 message ***************/
		if (!gtmsource_repl_recv((repl_msg_ptr_t)&histinfo2_msg, MIN_REPL_MSGLEN,
							REPL_OLD_TRIPLEINFO2, "REPL_OLD_TRIPLEINFO2"))
			return FALSE; /* recv did not succeed */
		assert(REPL_OLD_TRIPLEINFO2 == histinfo2_msg.type);

		/* Check if start_seqno in HISTINFO1 and HISTINFO2 message is the same. If not something is wrong */
		if (histinfo1_msg.start_seqno != histinfo2_msg.start_seqno)
		{
			assert(FALSE);
			repl_log(gtmsource_log_fp, TRUE, FALSE, "REPL_OLD_TRIPLEINFO1 msg has start_seqno %llu [0x%llx] while "
				"corresponding REPL_OLD_TRIPLEINFO2 msg has a different start_seqno %llu [0x%llx]. "
				"Closing connection.\n", histinfo1_msg.start_seqno, histinfo1_msg.start_seqno,
				histinfo2_msg.start_seqno, histinfo2_msg.start_seqno);
			repl_close(&gtmsource_sock_fd);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
			gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
			return FALSE;
		}
		memset(histinfo, 0, SIZEOF(*histinfo));
		memcpy(histinfo->root_primary_instname, histinfo1_msg.instname, MAX_INSTNAME_LEN - 1);
		histinfo->start_seqno = histinfo1_msg.start_seqno;
		histinfo->root_primary_cycle = histinfo2_msg.cycle;
		histinfo->histinfo_num = histinfo2_msg.histinfo_num;
	} else
	{	/* Remote side does support supplementary protocol. Use newer protocol messages to communicate. */
		/*************** Receive REPL_HISTINFO message ***************/
		if (!gtmsource_repl_recv((repl_msg_ptr_t)&histinfo_msg, SIZEOF(repl_histinfo_msg_t),
									REPL_HISTINFO, "REPL_HISTINFO"))
			return FALSE; /* recv did not succeed */
		assert(REPL_HISTINFO == histinfo_msg.type);
		*histinfo = histinfo_msg.history;
	}
	assert(0 <= histinfo->histinfo_num);
	return TRUE;
}

/* Given an input "seqno", this function determines how many non-supplementary streams are known to the receiver server
 * as of instance jnl_seqno = "seqno". It then compares if the source side list of known streams are identical. For each
 * such stream, exchange and verify the stream-specific history record corresponding to "seqno" is the same.
 *
 *	seqno		--> The journal seqno that is to be searched in the instance file history.
 *	*rollback_first	--> Set to TRUE if we find some stream history record not matching between the source & receiver side.
 */
boolean_t	gtmsource_check_remote_strm_histinfo(seq_num seqno, boolean_t *rollback_first)
{
	boolean_t		lcl_strm_valid, remote_strm_valid;
	int4			lcl_histinfo_num;
	char			err_string[1024];
	int			idx, status;
	repl_histinfo		local_histinfo;
	repl_histinfo_msg_t	histinfo_msg;
	repl_needhistinfo_msg_t	needhistinfo_msg;
	repl_needstrminfo_msg_t	needstrminfo_msg;
	repl_strminfo_msg_t	strminfo_msg;

	assert(remote_side->is_supplementary);
	assert(REPL_PROTO_VER_SUPPLEMENTARY <= remote_side->proto_ver);
	assert(0 == strm_index);
	assert(FALSE == *rollback_first);
	/*************** Send REPL_NEED_STRMINFO message ***************/
	memset(&needstrminfo_msg, 0, SIZEOF(needstrminfo_msg));
	assert(SIZEOF(needstrminfo_msg) == MIN_REPL_MSGLEN);
	needstrminfo_msg.type = REPL_NEED_STRMINFO;
	needstrminfo_msg.len = MIN_REPL_MSGLEN;
	needstrminfo_msg.seqno = seqno;
	gtmsource_repl_send((repl_msg_ptr_t)&needstrminfo_msg, "REPL_NEED_STRMINFO", seqno, INVALID_SUPPL_STRM);
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return FALSE; /* send did not succeed */
	/*************** Receive REPL_STRMINFO message ***************/
	if (!gtmsource_repl_recv((repl_msg_ptr_t)&strminfo_msg, SIZEOF(repl_strminfo_msg_t), REPL_STRMINFO, "REPL_STRMINFO"))
		return FALSE; /* recv did not succeed */
	assert(REPL_STRMINFO == strminfo_msg.type);
	/* Verify that the list of known streams is identical on both sides */
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
	if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
		return FALSE;	/* concurrent online rollback happened */
	status = repl_inst_histinfo_find_seqno(seqno, INVALID_SUPPL_STRM, &local_histinfo);
	rel_lock(jnlpool.jnlpool_dummy_reg);
	assert(0 == status);	/* we are guaranteed to find this since we have already verified 0th stream matches */
	/* Fix last_histinfo_num[] in local side to include "local_histinfo" too (which could have strm_index > 0) */
	if (0 < local_histinfo.strm_index)
	{
		assert(local_histinfo.last_histinfo_num[local_histinfo.strm_index] < local_histinfo.histinfo_num);
		local_histinfo.last_histinfo_num[local_histinfo.strm_index] = local_histinfo.histinfo_num;
	}
	/* Skip 0th stream as it has already been verified */
	for (idx = 1; idx < MAX_SUPPL_STRMS; idx++)
	{
		lcl_strm_valid = (INVALID_HISTINFO_NUM != local_histinfo.last_histinfo_num[idx]);
		remote_strm_valid = (INVALID_HISTINFO_NUM != strminfo_msg.last_histinfo_num[idx]);
		if (!lcl_strm_valid && remote_strm_valid)
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_STRMNUMMISMTCH1, 1, idx);
		}
		else if (lcl_strm_valid && !remote_strm_valid)
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_STRMNUMMISMTCH2, 1, idx);
		}
	}
	/* Now that we know both sides have the exact set of known streams, verify history record for each stream matches */
	/* Send REPL_NEED_HISTINFO message for each stream. Do common initialization outside loop */
	memset(&needhistinfo_msg, 0, SIZEOF(needhistinfo_msg));
	assert(SIZEOF(needhistinfo_msg) == MIN_REPL_MSGLEN);
	needhistinfo_msg.type = REPL_NEED_HISTINFO;
	needhistinfo_msg.len = MIN_REPL_MSGLEN;
	/* No need to initialize the following as needhistinfo_msg.histinfo_num will override those.
	 *	needhistinfo_msg.seqno = ...
	 */
	for (idx = 1; idx < MAX_SUPPL_STRMS; idx++)
	{
		lcl_histinfo_num = local_histinfo.last_histinfo_num[idx];
		if (INVALID_HISTINFO_NUM == lcl_histinfo_num)
			continue;
		/* Find corresponding history record on REMOTE side */
		assert(INVALID_HISTINFO_NUM != strminfo_msg.last_histinfo_num[idx]);
		needhistinfo_msg.histinfo_num = strminfo_msg.last_histinfo_num[idx];
		needhistinfo_msg.strm_num = idx;
		/*************** Send REPL_NEED_HISTINFO message ***************/
		gtmsource_repl_send((repl_msg_ptr_t)&needhistinfo_msg, "REPL_NEED_HISTINFO", seqno, needhistinfo_msg.strm_num);
		if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
			return FALSE; /* send did not succeed */
		/*************** Receive REPL_HISTINFO message ***************/
		if (!gtmsource_repl_recv((repl_msg_ptr_t)&histinfo_msg, SIZEOF(repl_histinfo_msg_t),
									REPL_HISTINFO, "REPL_HISTINFO"))
			return FALSE; /* recv did not succeed */
		assert(REPL_HISTINFO == histinfo_msg.type);
		/* Find corresponding history record on LOCAL side */
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
			return FALSE;	/* concurrent online rollback happened */
		status = repl_inst_histinfo_get(lcl_histinfo_num, &local_histinfo);
		assert(0 == status); /* Since we pass histinfo_num of 0 which is >=0 and < num_histinfo */
		rel_lock(jnlpool.jnlpool_dummy_reg);
		/* Compare the two history records. If they are not identical for even one stream, signal rollback on receiver */
		if (!gtmsource_is_histinfo_identical(&histinfo_msg.history, &local_histinfo, seqno, OK_TO_LOG_TRUE))
			*rollback_first = TRUE;
	}
	return TRUE;
}

/* This function finds the 'n'th histinfo record in the replication instance file of this instance.
 * This is a wrapper on top of "repl_inst_histinfo_get" which additionally does error checking.
 * This closes the connection if it detects a REPLINSTNOHIST error.
 */
void	gtmsource_histinfo_get(int4 index, repl_histinfo *histinfo)
{
	unix_db_info	*udi;
	char		histdetail[256];
	int4		status;
	repl_msg_t	instnohist_msg;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->s_addrs.now_crit);
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	status = repl_inst_histinfo_get(index, histinfo);
	assert((0 == status) || (INVALID_HISTINFO_NUM == index));
	assert((0 != status) || (index == histinfo->histinfo_num));
	if (0 != status)
	{
		assert(ERR_REPLINSTNOHIST == status);	/* currently the only error returned by "repl_inst_histinfo_get" */
		SPRINTF(histdetail, "record number %d [0x%x]", index, index);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLINSTNOHIST, 4, LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
		/* Send this error status to the receiver server before closing the connection. This way the receiver
		 * will know to shut down rather than loop back trying to reconnect. This avoids an infinite loop of
		 * connection open and closes between the source server and receiver server.
		 */
		instnohist_msg.type = REPL_INST_NOHIST;
		instnohist_msg.len = MIN_REPL_MSGLEN;
		memset(&instnohist_msg.msg[0], 0, SIZEOF(instnohist_msg.msg));
		gtmsource_repl_send((repl_msg_ptr_t)&instnohist_msg, "REPL_INST_NOHIST", MAX_SEQNO, INVALID_SUPPL_STRM);
		/* Close the connection */
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset due to above REPLINSTNOHIST error\n");
		repl_close(&gtmsource_sock_fd);
		SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
		gtmsource_state = jnlpool.gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
	}
}

/* Given two histinfo records (one each from the primary and secondary) corresponding to the seqno "jnl_seqno-1", this function
 * compares if the history information in both histinfo records is the same. If so, it returns TRUE else it returns FALSE.
 */
boolean_t	gtmsource_is_histinfo_identical(repl_histinfo *remote_histinfo, repl_histinfo *local_histinfo,
										seq_num jnl_seqno, boolean_t ok_to_log)
{
	if (ok_to_log)
	{
		repl_log(gtmsource_log_fp, FALSE, FALSE, "On Secondary, seqno %llu [0x%llx] generated by instance name [%s] "
			"with cycle [0x%x] (%d), pid = %d : time = %d [0x%x]\n", jnl_seqno - 1, jnl_seqno - 1,
			remote_histinfo->root_primary_instname, remote_histinfo->root_primary_cycle,
			remote_histinfo->root_primary_cycle, remote_histinfo->creator_pid,
			remote_histinfo->created_time, remote_histinfo->created_time);
		repl_log(gtmsource_log_fp, FALSE, FALSE, "On Primary, seqno %llu [0x%llx] generated by instance name [%s] "
			"with cycle [0x%x] (%d), pid = %d : time = %d [0x%x]\n", jnl_seqno - 1, jnl_seqno - 1,
			local_histinfo->root_primary_instname, local_histinfo->root_primary_cycle,
			local_histinfo->root_primary_cycle, local_histinfo->creator_pid,
			local_histinfo->created_time, local_histinfo->created_time);
	}
	/* Starting with the version of GT.M that supports supplementary instances, we check for a lot more pieces of the history.
	 * But if remote side is an older version that does not support the supplementary protocol, check only those pieces
	 * which were checked previously.
	 */
	if (STRCMP(local_histinfo->root_primary_instname, remote_histinfo->root_primary_instname)
		|| (local_histinfo->root_primary_cycle != remote_histinfo->root_primary_cycle)
		|| ((REPL_PROTO_VER_SUPPLEMENTARY <= remote_side->proto_ver)
			&& ((local_histinfo->creator_pid != remote_histinfo->creator_pid)
				|| (local_histinfo->created_time != remote_histinfo->created_time))))
	{	/* either the root primary instance name or the cycle did not match */
		if (ok_to_log)
			repl_log(gtmsource_log_fp, FALSE, FALSE, "Primary and Secondary have DIFFERING history records for "
				"seqno %llu [0x%llx]\n", jnl_seqno - 1, jnl_seqno - 1);
		return FALSE;
	} else
	{
		if (ok_to_log)
			repl_log(gtmsource_log_fp, FALSE, FALSE, "Primary and Secondary have IDENTICAL history records for "
				"seqno %llu [0x%llx]\n", jnl_seqno - 1, jnl_seqno - 1);
		return TRUE;
	}
}

/* Determine the resync seqno between primary and secondary by comparing local and remote histinfo records from the tail of the
 * respective instance files until we reach a seqno whose histinfo record information is identical in both. The resync seqno
 * is the first seqno whose histinfo record information was NOT identical in both. The histinfo records on the secondary are
 * obtained through successive calls to the function "gtmsource_get_remote_histinfo".
 *
 * If the connection gets reset while exchanging histinfo records with secondary, this function returns a seqno of MAX_SEQNO.
 * The global variable "gtmsource_state" will be set to GTMSOURCE_CHANGING_MODE or GTMSOURCE_WAITING_FOR_CONNECTION and the
 * caller of this function should accordingly check for that immediately on return.
 */
seq_num	gtmsource_find_resync_seqno(repl_histinfo *local_histinfo, repl_histinfo *remote_histinfo)
{
	seq_num			max_start_seqno, local_start_seqno, remote_start_seqno;
	int4			local_histinfo_num;
	DEBUG_ONLY(int4		prev_remote_histinfo_num;)
	DEBUG_ONLY(sgmnt_addrs	*csa;)
	DEBUG_ONLY(seq_num	min_start_seqno;)

	assert((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open);
	DEBUG_ONLY(
		csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		ASSERT_VALID_JNLPOOL(csa);
	)
	DEBUG_ONLY(prev_remote_histinfo_num = remote_histinfo->prev_histinfo_num;)
	do
	{
		local_start_seqno = local_histinfo->start_seqno;
		remote_start_seqno = remote_histinfo->start_seqno;
		assert(local_start_seqno);
		assert(remote_start_seqno);
		max_start_seqno = MAX(local_start_seqno, remote_start_seqno);
		/* "max_start_seqno" is the largest yet known seqno whose histinfo does NOT match between primary and secondary.
		 * Therefore determine the histinfo(s) for "max_start_seqno-1" in primary and/or secondary and compare them.
		 */
		if (1 == max_start_seqno)
		{	/* The earliest possible seqno in the primary is out of sync with that of the secondary. Stop the histinfo
			 * search right away and return with 1 (the smallest possible seqno) as the resync seqno.
			 */
			assert(local_start_seqno == max_start_seqno);
			assert(remote_start_seqno == max_start_seqno);
			assert(INVALID_HISTINFO_NUM == local_histinfo->prev_histinfo_num);
			assert(INVALID_HISTINFO_NUM == remote_histinfo->prev_histinfo_num);
			break;
		}
		DEBUG_ONLY(min_start_seqno = MIN(local_start_seqno, remote_start_seqno);)
		assert(!gtmsource_is_histinfo_identical(remote_histinfo, local_histinfo, min_start_seqno, OK_TO_LOG_FALSE));
		if (local_start_seqno == max_start_seqno)
		{	/* Need to get the previous histinfo record on the primary */
			local_histinfo_num = local_histinfo->prev_histinfo_num;
			assert(0 <= local_histinfo->histinfo_num);
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
			if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
				return MAX_SEQNO;
			gtmsource_histinfo_get(local_histinfo_num, local_histinfo);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				return MAX_SEQNO;	/* Connection got reset in "gtmsource_histinfo_get" due to REPLINSTNOHIST */
		}
		if (remote_start_seqno == max_start_seqno)
		{	/* Need to get the previous histinfo record on the secondary */
			assert(0 <= prev_remote_histinfo_num);
			if (!gtmsource_get_remote_histinfo(remote_start_seqno, remote_histinfo))
			{
				assert((GTMSOURCE_CHANGING_MODE == gtmsource_state)
					|| (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state));
				/* Connection got reset while exchanging histinfo history information */
				return MAX_SEQNO;
			}
			assert(remote_histinfo->histinfo_num == prev_remote_histinfo_num);
			DEBUG_ONLY(prev_remote_histinfo_num = remote_histinfo->prev_histinfo_num);
		}
		assert(local_histinfo->start_seqno < max_start_seqno);
		assert(remote_histinfo->start_seqno < max_start_seqno);
	} while (!gtmsource_is_histinfo_identical(remote_histinfo, local_histinfo, max_start_seqno - 1, OK_TO_LOG_TRUE));
	repl_log(gtmsource_log_fp, TRUE, FALSE, "Resync Seqno determined is %llu [0x%llx]\n",  max_start_seqno, max_start_seqno);
	/* "max_start_seqno-1" has same histinfo info in both primary and secondary. Hence "max_start_seqno" is the resync seqno. */
	return max_start_seqno;
}

/* This routine sends a REPL_HISTREC message for the histinfo record corresponding to seqno "gtmsource_local->read_jnl_seqno".
 * It positions the send to that histinfo record which corresponds to "gtmsource_local->read_jnl_seqno". This is done by
 * a call to the function "gtmsource_set_next_histinfo_seqno".
 * On return from this routine, the caller should check the value of the global variable "gtmsource_state" to see if it is
 * either of GTMSOURCE_CHANGING_MODE or GTMSOURCE_WAITING_FOR_CONNECTION and if so take appropriate action.
 */
void	gtmsource_send_new_histrec()
{
	repl_histinfo		histinfo, zero_histinfo;
	repl_old_triple_msg_t	oldtriple_msg;
	gtmsource_local_ptr_t	gtmsource_local;
	boolean_t		first_histrec_send;
	int4			zero_histinfo_num;
	DEBUG_ONLY(sgmnt_addrs	*csa;)

	assert((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open);
	DEBUG_ONLY(
		csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		ASSERT_VALID_JNLPOOL(csa);
	)
	gtmsource_local = jnlpool.gtmsource_local;
	assert(gtmsource_local->send_new_histrec);
	assert(gtmsource_local->read_jnl_seqno <= gtmsource_local->next_histinfo_seqno);
	first_histrec_send = (-1 == gtmsource_local->next_histinfo_num);
	gtmsource_set_next_histinfo_seqno(FALSE);	/* sets gtmsource_local->next_histinfo_seqno & next_histinfo_num */
	if ((GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state) || (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state))
		return;	/* "gtmsource_set_next_histinfo_seqno" encountered REPLINSTNOHIST or concurrent online rollback occurred */
	/*************** Read histinfo (to send) from instance file first ***************/
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
	if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
		return;
	assert(1 <= gtmsource_local->next_histinfo_num);
	/* With non-supplementary instances, we are guaranteed, consecutive history records increase in start_seqno.
	 * But with supplementary instances, it is possible for two consecutive history records to have the same start_seqno
	 * (if those came from different root primary instances at the same time). Take care of that in the assert below.
	 */
	assert(!this_side->is_supplementary && (gtmsource_local->read_jnl_seqno < gtmsource_local->next_histinfo_seqno)
		|| this_side->is_supplementary && (gtmsource_local->read_jnl_seqno <= gtmsource_local->next_histinfo_seqno));
	gtmsource_histinfo_get(gtmsource_local->next_histinfo_num - 1, &histinfo);
	if ((GTMSOURCE_WAITING_FOR_CONNECTION != gtmsource_state) && this_side->is_supplementary && first_histrec_send
			&& (0 < histinfo.strm_index))
	{	/* Supplementary source side sending to a supplementary receiver. And this is the FIRST history record
		 * being sent for this replication connection. It is possible that the closest history record prior to
		 * "gtmsource_local->read_jnl_seqno" has a non-zero "strm_index". In that case, we might be sending a
		 * non-zero strm_index history record across and it is possible that the secondary has an empty instance
		 * file (in case of an -updateresync startup). This would lead to issues on the receiving supplementary
		 * instance since it will now have a non-zero stream history record as the first history record in its
		 * instance file. This is an out-of-design situation since to establish the resync point between two
		 * supplementary instances, we need to examine the 0th stream and the assumption is that if the instance
		 * file has at least one history record, there is at least one 0th stream history record. Therefore we
		 * need to prevent this out-of-design situation. Towards that, find out the most recent 0th stream
		 * history record in this instance and send that across BEFORE sending the history record corresponding
		 * to "gtmsource_local->read_jnl_seqno".
		 */
		zero_histinfo_num = histinfo.last_histinfo_num[0];
		assert(INVALID_HISTINFO_NUM != zero_histinfo_num);
		assert(0 <= zero_histinfo_num);
		gtmsource_histinfo_get(zero_histinfo_num, &zero_histinfo);
	} else
		zero_histinfo_num = INVALID_HISTINFO_NUM;
	rel_lock(jnlpool.jnlpool_dummy_reg);
	if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
	{
		assert(FALSE);
		return;	/* Connection got reset in "gtmsource_histinfo_get" due to REPLINSTNOHIST */
	}
	assert(gtmsource_local->read_jnl_seqno >= histinfo.start_seqno);
	assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
	if (REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver)
	{	/* Remote side does not support supplementary protocol. Use older protocol messages to communicate. */
		assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
		assert(!this_side->is_supplementary);
		/*************** Send REPL_OLD_TRIPLE message ***************/
		memset(&oldtriple_msg, 0, SIZEOF(oldtriple_msg));
		oldtriple_msg.type = REPL_OLD_TRIPLE;
		oldtriple_msg.len = SIZEOF(oldtriple_msg);
		oldtriple_msg.triplecontent.jrec_type = JRT_TRIPLE;
		if (remote_side->cross_endian && (this_side->jnl_ver < remote_side->jnl_ver))
		{
			oldtriple_msg.triplecontent.forwptr = GTM_BYTESWAP_24(SIZEOF(repl_old_triple_jnl_t));
			oldtriple_msg.triplecontent.start_seqno = GTM_BYTESWAP_64(gtmsource_local->read_jnl_seqno);
			oldtriple_msg.triplecontent.cycle = GTM_BYTESWAP_32(histinfo.root_primary_cycle);
		} else
		{
			oldtriple_msg.triplecontent.forwptr = SIZEOF(repl_old_triple_jnl_t);
			oldtriple_msg.triplecontent.start_seqno = gtmsource_local->read_jnl_seqno;
			oldtriple_msg.triplecontent.cycle = histinfo.root_primary_cycle;
		}
		memcpy(oldtriple_msg.triplecontent.instname, histinfo.root_primary_instname, MAX_INSTNAME_LEN - 1);
		gtmsource_repl_send((repl_msg_ptr_t)&oldtriple_msg, "REPL_OLD_TRIPLE",
					gtmsource_local->read_jnl_seqno, INVALID_SUPPL_STRM);
	} else
	{	/* Remote side supports supplementary protocol. Communicate using new message protocol */
		if (INVALID_HISTINFO_NUM != zero_histinfo_num)
		{	/* Send REPL_HISTREC message corresponding to the 0th stream history record */
			assert(gtmsource_local->read_jnl_seqno >= zero_histinfo.start_seqno);
			GTMSOURCE_SEND_REPL_HISTREC(zero_histinfo, gtmsource_local, remote_side->cross_endian);
			/* the above macro would have invoked "gtmsource_repl_send" so check for "gtmsource_state" */
			if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
				return; /* send did not succeed */
			/* If we have a non-zero stream history record, then check if its "start_seqno" is lesser than
			 * gtmsource_local->read_jnl_seqno. If so, we dont even need to send this history record. If it is
			 * equal though, we do need to send this across.
			 */
			assert(gtmsource_local->read_jnl_seqno >= histinfo.start_seqno);
			if (gtmsource_local->read_jnl_seqno == histinfo.start_seqno)
				GTMSOURCE_SEND_REPL_HISTREC(histinfo, gtmsource_local, remote_side->cross_endian);
		} else
			GTMSOURCE_SEND_REPL_HISTREC(histinfo, gtmsource_local, remote_side->cross_endian);
	}
	if ((GTMSOURCE_CHANGING_MODE == gtmsource_state) || (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
		return; /* send did not succeed */
	repl_dump_histinfo(gtmsource_log_fp, TRUE, TRUE, "New History Content", &histinfo);
	gtmsource_local->send_new_histrec = FALSE;
}

/* This function is invoked once for each histinfo record that the source server goes past while sending journal records across.
 * This function sets the boundary seqno field "next_histinfo_seqno" to be the "start_seqno" of the next histinfo record so the
 * source server does not send any seqnos corresponding to the next histinfo record before sending a REPL_HISTREC message.
 * It will set "gtmsource_local->next_histinfo_seqno" and "gtmsource_local->next_histinfo_num" to correspond to the next histinfo
 * record and set the private copy "gtmsource_local->num_histinfo" to a copy of what is currently present in
 * "repl_inst_filehdr->num_histinfo".
 *
 * The input variable "detect_new_histinfo" is set to TRUE if this function is called from "gtmsource_readfiles" or
 * "gtmsource_readpool" the moment they detect that the instance file has had a new histinfo record since the last time this
 * source server took a copy of it in its private "gtmsource_local->num_histinfo". In this case, the only objective
 * is to find the start_seqno of the next histinfo record and note that down as "gtmsource_local->next_histinfo_seqno".
 *
 * If the input variable "detect_new_histinfo" is set to FALSE, "next_histinfo_seqno" is set to the starting seqno of the
 * histinfo record immediately after that corresponding to "gtmsource_local->read_jnl_seqno".
 *
 * This can end up closing the connection if the call to "gtmsource_histinfo_get" or "repl_inst_histinfo_find_seqno" fails.
 */
void	gtmsource_set_next_histinfo_seqno(boolean_t detect_new_histinfo)
{
	unix_db_info		*udi;
	int4			status, next_histinfo_num, num_histinfo;
	repl_histinfo		next_histinfo, prev_histinfo;
	gtmsource_local_ptr_t	gtmsource_local;
	repl_msg_t		instnohist_msg;
	char			histdetail[256];
	seq_num			read_seqno;

	DEBUG_ONLY(sgmnt_addrs	*csa;)

	assert((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open);
	DEBUG_ONLY(
		csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		ASSERT_VALID_JNLPOOL(csa);
	)
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
	if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
		return;
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	gtmsource_local = jnlpool.gtmsource_local;
	next_histinfo_num = gtmsource_local->next_histinfo_num;
	/* assert((-1 == next_histinfo_num) || (gtmsource_local->next_histinfo_seqno >= gtmsource_local->read_jnl_seqno)); */
	read_seqno = gtmsource_local->read_jnl_seqno;
	assert(gtmsource_local->next_histinfo_seqno >= read_seqno);
	num_histinfo = jnlpool.repl_inst_filehdr->num_histinfo;
	if (!detect_new_histinfo)
	{
		if (-1 == next_histinfo_num)
		{	/* This is the first invocation of this function after this connection with a receiver was established.
			 * Find the first histinfo record in the local instance file corresponding to the next seqno to be sent
			 * across i.e. "gtmsource_local->read_jnl_seqno". The below function will return the history record
			 * just BEFORE read_jnl_seqno. So fetch the immediately next history record to get the desired record.
			 */
			assert(read_seqno <= jnlpool.jnlpool_ctl->jnl_seqno);
			status = repl_inst_histinfo_find_seqno(read_seqno, INVALID_SUPPL_STRM, &prev_histinfo);
			if (0 != status)
			{
				assert(ERR_REPLINSTNOHIST == status); /* only error returned by "repl_inst_histinfo_find_seqno" */
				assert((INVALID_HISTINFO_NUM == prev_histinfo.histinfo_num)
						|| (prev_histinfo.start_seqno >= read_seqno));
				if ((INVALID_HISTINFO_NUM == prev_histinfo.histinfo_num)
					|| (prev_histinfo.start_seqno > read_seqno))
				{	/* The read seqno is PRIOR to the starting seqno of the instance file.
					 * In that case, issue error and close the connection.
					 */
					NON_GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%llx]", read_seqno - 1));
					GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%lx]", read_seqno - 1));
					udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLINSTNOHIST, 4,
							LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
					/* Send error status to the receiver server before closing the connection. This way the
					 * receiver will know to shut down rather than loop back trying to reconnect. This avoids
					 * an infinite loop of connection open/closes between the source and receiver servers.
					 */
					instnohist_msg.type = REPL_INST_NOHIST;
					instnohist_msg.len = MIN_REPL_MSGLEN;
					memset(&instnohist_msg.msg[0], 0, SIZEOF(instnohist_msg.msg));
					gtmsource_repl_send((repl_msg_ptr_t)&instnohist_msg, "REPL_INST_NOHIST",
											MAX_SEQNO, INVALID_SUPPL_STRM);
					rel_lock(jnlpool.jnlpool_dummy_reg);
					repl_log(gtmsource_log_fp, TRUE, TRUE,
						"Connection reset due to above REPLINSTNOHIST error\n");
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					return;
				}
				next_histinfo_num = prev_histinfo.histinfo_num;
			} else
			{
				assert(prev_histinfo.start_seqno < read_seqno);
				next_histinfo_num = prev_histinfo.histinfo_num;
				if ((next_histinfo_num + 1) < num_histinfo)
				{
					gtmsource_histinfo_get(next_histinfo_num + 1, &next_histinfo);
					if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
					{
						assert(FALSE);
						rel_lock(jnlpool.jnlpool_dummy_reg);
						return;	/* Connection got reset in "gtmsource_histinfo_get" due to REPLINSTNOHIST */
					}
					assert(next_histinfo.start_seqno >= read_seqno);
					if (next_histinfo.start_seqno == read_seqno)
					{
						next_histinfo_num++;
						assert(next_histinfo_num == next_histinfo.histinfo_num);
					}
				}
			}
		}
		/* else: next_histinfo_num was already found. So just move on to the NEXT history record. */
		assert(0 <= next_histinfo_num);
		assert(next_histinfo_num < num_histinfo);
		assert((gtmsource_local->next_histinfo_num != next_histinfo_num)
			|| (read_seqno == gtmsource_local->next_histinfo_seqno)
			|| (MAX_SEQNO == gtmsource_local->next_histinfo_seqno));
		next_histinfo_num++;
	} else
	{	/* A new histinfo record got added to the instance file since we knew last.
		 * Set "next_histinfo_seqno" for our current histinfo down from its current value of MAX_SEQNO.
		 */
		assert(gtmsource_local->next_histinfo_seqno == MAX_SEQNO);
		assert(next_histinfo_num < num_histinfo);
		if (READ_FILE == gtmsource_local->read_state)
		{	/* It is possible that we have already read the journal records for the next
			 * read_jnl_seqno before detecting that a histinfo has to be sent first. In that case,
			 * the journal files may have been positioned ahead of the read_jnl_seqno for the
			 * next read. Indicate that they have to be repositioned into the past.
			 */
			gtmsource_set_lookback();
		}
	}
	if (num_histinfo > next_histinfo_num)
	{	/* Read the next histinfo record to determine its "start_seqno" */
		gtmsource_histinfo_get(next_histinfo_num, &next_histinfo);
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
		{
			assert(FALSE);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			return;	/* Connection got reset in "gtmsource_histinfo_get" due to REPLINSTNOHIST */
		}
		assert(next_histinfo.start_seqno >= read_seqno);
		gtmsource_local->next_histinfo_seqno = next_histinfo.start_seqno;
	} else
		gtmsource_local->next_histinfo_seqno = MAX_SEQNO;
	gtmsource_local->next_histinfo_num = next_histinfo_num;
	gtmsource_local->num_histinfo = num_histinfo;
	rel_lock(jnlpool.jnlpool_dummy_reg);
}
