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

#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_time.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_stdio.h"

#include <sys/time.h>
#include <errno.h>
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
#include "gtmrecv.h"
#include "repl_comm.h"
#include "repl_msg.h"
#include "repl_dbg.h"
#include "repl_errno.h"
#include "iosp.h"
#include "gtm_event_log.h"
#include "eintr_wrappers.h"
#include "jnl.h"
#include "repl_sp.h"
#include "repl_filter.h"
#include "repl_log.h"
#include "gtmsource.h"
#include "gtm_netdb.h"
#include "sgtm_putmsg.h"
#include "gt_timer.h"
#include "min_max.h"
#include "error.h"
#include "copy.h"
#include "memcoherency.h"
#include "replgbl.h"

#define RECVBUFF_REPLMSGLEN_FACTOR 		8

#define GTMRECV_WAIT_FOR_STARTJNLSEQNO		100 /* ms */

#define GTMRECV_WAIT_FOR_UPD_PROGRESS		100 /* ms */
#define GTMRECV_WAIT_FOR_UPD_PROGRESS_US	(GTMRECV_WAIT_FOR_UPD_PROGRESS * 1000) /* micro sec */

/* By having different high and low watermarks, we can reduce the # of XOFF/XON exchanges */
#define RECVPOOL_HIGH_WATERMARK_PCTG		90	/* Send XOFF when %age of receive pool space occupied goes beyond this */
#define RECVPOOL_LOW_WATERMARK_PCTG		80	/* Send XON when %age of receive pool space occupied falls below this */
#define RECVPOOL_XON_TRIGGER_SIZE		(1 * 1024 * 1024) /* Keep the low water mark within this amount of high water mark
								   * so that we don't wait too long to send XON */

#define GTMRECV_XOFF_LOG_CNT			100

#define GTMRECV_HEARTBEAT_PERIOD		10	/* seconds, timer that goes off every this period is the time keeper for
							 * receiver server; used to reduce calls to time related systemc calls */

GBLDEF	repl_msg_ptr_t		gtmrecv_msgp;
GBLDEF	int			gtmrecv_max_repl_msglen;
GBLDEF	int			gtmrecv_sock_fd = FD_INVALID;
GBLDEF	boolean_t		repl_connection_reset = FALSE;
GBLDEF	boolean_t		gtmrecv_wait_for_jnl_seqno = FALSE;
GBLDEF	boolean_t		gtmrecv_bad_trans_sent = FALSE;
GBLDEF	struct addrinfo		primary_ai;
GBLDEF	struct sockaddr_storage	primary_sas;

GBLDEF	qw_num			repl_recv_data_recvd = 0;
GBLDEF	qw_num			repl_recv_data_processed = 0;
GBLDEF	qw_num			repl_recv_prefltr_data_procd = 0;
GBLDEF	qw_num			repl_recv_lastlog_data_recvd = 0;
GBLDEF	qw_num			repl_recv_lastlog_data_procd = 0;

GBLDEF	time_t			repl_recv_prev_log_time;
GBLDEF	time_t			repl_recv_this_log_time;
GBLDEF	volatile time_t		gtmrecv_now = 0;

GBLREF  gtmrecv_options_t	gtmrecv_options;
GBLREF	int			gtmrecv_listen_sock_fd;
GBLREF	recvpool_addrs		recvpool;
GBLREF  boolean_t               gtmrecv_logstats;
GBLREF	int			gtmrecv_filter;
GBLREF	int			gtmrecv_log_fd;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	seq_num			seq_num_zero, seq_num_one, seq_num_minus_one;
GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
GBLREF	unsigned char		*repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	boolean_t 		primary_side_std_null_coll;
GBLREF	boolean_t		primary_side_trigger_support;
GBLREF	boolean_t 		secondary_side_std_null_coll;
GBLREF	boolean_t		secondary_side_trigger_support;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;
GBLREF	qw_num			trans_recvd_cnt, last_log_tr_recvd_cnt;

error_def(ERR_JNLNEWREC);
error_def(ERR_JNLRECFMT);
error_def(ERR_JNLSETDATA2LONG);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLGBL2LONG);
error_def(ERR_REPLTRANS2BIG);
error_def(ERR_REPLWARN);
error_def(ERR_SECNODZTRIGINTP);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);

static	unsigned char	*buffp, *buff_start, *msgbuff, *filterbuff;
static	int		buff_unprocessed;
static	int		buffered_data_len;
static	int		max_recv_bufsiz;
static	int		data_len;
static 	boolean_t	xoff_sent;
static	repl_msg_t	xon_msg, xoff_msg;
static	int		xoff_msg_log_cnt = 0;
static	long		recvpool_high_watermark, recvpool_low_watermark;
static	uint4		write_loc, write_wrap;
static  uint4		write_len, write_off,
			pre_filter_write_len, pre_filter_write, pre_intlfilter_datalen;
static	double		time_elapsed;
static	int		recvpool_size;
static	int		heartbeat_period;

static void do_flow_control(uint4 write_pos)
{
	/* Check for overflow before writing */

	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	long			space_used;
	unsigned char		*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int			status;					/* needed for REPL_{SEND,RECV}_LOOP */
	int			read_pos;
	char			print_msg[1024];

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	space_used = 0;
	if (recvpool_ctl->wrapped)
		space_used = write_pos + recvpool_size - (read_pos = upd_proc_local->read);
	if (!recvpool_ctl->wrapped || space_used > recvpool_size)
		space_used = write_pos - (read_pos = upd_proc_local->read);
	if (space_used >= recvpool_high_watermark && !xoff_sent)
	{
		/* Send XOFF message */
		xoff_msg.type = REPL_XOFF;
		memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, SIZEOF(seq_num));
		xoff_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xoff_msg, xoff_msg.len, REPL_POLL_NOWAIT)
		{
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}
		if (SS_NORMAL != status)
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_connection_reset = TRUE;
				return;
			}
			if (EREPL_SEND == repl_errno)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending XOFF msg. Error in send : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
			if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending XOFF msg. Error in select : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
		}
		if (gtmrecv_logstats)
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Space used = %ld, High water mark = %d Low water mark = %d, "
					"Updproc Read = %d, Recv Write = %d, Sent XOFF\n", space_used, recvpool_high_watermark,
					recvpool_low_watermark, read_pos, write_pos);
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_XOFF sent as receive pool has %ld bytes transaction data yet to be "
				"processed\n", space_used);
		xoff_sent = TRUE;
		xoff_msg_log_cnt = 1;
	} else if (space_used < recvpool_low_watermark && xoff_sent)
	{
		xon_msg.type = REPL_XON;
		memcpy((uchar_ptr_t)&xon_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, SIZEOF(seq_num));
		xon_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xon_msg, xon_msg.len, REPL_POLL_NOWAIT)
		{
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}
		if (SS_NORMAL != status)
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_connection_reset = TRUE;
				return;
			}
			if (EREPL_SEND == repl_errno)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending XON msg. Error in send : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
			if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending XON msg. Error in select : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
		}
		if (gtmrecv_logstats)
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Space used now = %ld, High water mark = %d, "
				 "Low water mark = %d, Updproc Read = %d, Recv Write = %d, Sent XON\n", space_used,
				 recvpool_high_watermark, recvpool_low_watermark, read_pos, write_pos);
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_XON sent as receive pool has %ld bytes free space to buffer transaction "
				"data\n", recvpool_size - space_used);
		xoff_sent = FALSE;
		xoff_msg_log_cnt = 0;
	}
	return;
}

static int gtmrecv_est_conn(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	fd_set			input_fds;
	int			status;
	const   int     	disable_keepalive = 0;
	struct  linger  	disable_linger = {0, 0};
	struct  timeval 	poll_interval;
	char			print_msg[1024];
	int			send_buffsize, recv_buffsize, tcp_r_bufsize;

	/*
	 * Wait for a connection from a Source Server.
	 * The Receiver Server is an iterative server.
	 */

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	primary_ai.ai_addr = (sockaddr_ptr)&primary_sas;

	gtmrecv_comm_init((in_port_t)gtmrecv_local->listen_port);
	primary_ai.ai_addrlen = SIZEOF(primary_sas);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for a connection...\n");
	FD_ZERO(&input_fds);
	FD_SET(gtmrecv_listen_sock_fd, &input_fds);
	/*
	 * Note - the following while loop checks for EINTR on the select. The
	 * SELECT macro is not used because the FD_SET is redone before the new
	 * call to select (after the continue).
	 */
	poll_interval.tv_sec = 0;
	poll_interval.tv_usec = REPL_POLL_WAIT;
	while (0 >= (status = select(gtmrecv_listen_sock_fd + 1, &input_fds, NULL, NULL, &poll_interval)))
	{
		assert(0 == poll_interval.tv_sec);
		poll_interval.tv_usec = REPL_POLL_WAIT;
		FD_SET(gtmrecv_listen_sock_fd, &input_fds);
		if (0 == status)
			gtmrecv_poll_actions(0, 0, NULL);
		else if (EINTR == errno || EAGAIN == errno)
			continue;
		else
		{
			status = ERRNO;
			SNPRINTF(print_msg, SIZEOF(print_msg), "Error in select on listen socket : %s", STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
		}
	}
	ACCEPT_SOCKET(gtmrecv_listen_sock_fd, primary_ai.ai_addr, (sssize_t *)&primary_ai.ai_addrlen, gtmrecv_sock_fd);
	if (FD_INVALID == gtmrecv_sock_fd)
	{
		status = ERRNO;
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error accepting connection from Source Server : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
	/* Connection established */
	repl_close(&gtmrecv_listen_sock_fd); /* Close the listener socket */
	repl_connection_reset = FALSE;
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection accepted. Connection socket created.\n");
	if (-1 == setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, SIZEOF(disable_linger)))
	{
		status = ERRNO;
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error with receiver server socket disable linger : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}

#ifdef REPL_DISABLE_KEEPALIVE
	if (-1 == setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&disable_keepalive,
				SIZEOF(disable_keepalive)))
	{ /* Till SIGPIPE is handled properly */
		status = ERRNO;
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error with receiver server socket disable keepalive : %s",
				STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
#endif
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &send_buffsize)))
	{
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
	if (send_buffsize < GTMRECV_TCP_SEND_BUFSIZE)
	{
		if (0 != (status = set_send_sock_buff_size(gtmrecv_sock_fd, GTMRECV_TCP_SEND_BUFSIZE)))
		{
			if (send_buffsize < GTMRECV_MIN_TCP_SEND_BUFSIZE)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Could not set TCP send buffer size to %d : %s",
						GTMRECV_MIN_TCP_SEND_BUFSIZE, STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2,
						LEN_AND_STR(print_msg));
			}
		}
	}
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize))) /* may have changed */
	{
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
	if (0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &recv_buffsize)))
	{
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error getting socket recv buffsize : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
	if (recv_buffsize < GTMRECV_TCP_RECV_BUFSIZE)
	{
		for (tcp_r_bufsize = GTMRECV_TCP_RECV_BUFSIZE;
		     tcp_r_bufsize >= MAX(recv_buffsize, GTMRECV_MIN_TCP_RECV_BUFSIZE)
		     &&  0 != (status = set_recv_sock_buff_size(gtmrecv_sock_fd, tcp_r_bufsize));
		     tcp_r_bufsize -= GTMRECV_TCP_RECV_BUFSIZE_INCR)
			;
		if (tcp_r_bufsize < GTMRECV_MIN_TCP_RECV_BUFSIZE)
		{
			SNPRINTF(print_msg, SIZEOF(print_msg), "Could not set TCP receive buffer size in range [%d, %d], last "
					"known error : %s", GTMRECV_MIN_TCP_RECV_BUFSIZE, GTMRECV_TCP_RECV_BUFSIZE,
					STRERROR(status));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2,
					LEN_AND_STR(print_msg));
		}
	}
	if (0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &repl_max_recv_buffsize))) /* may have changed */
	{
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error getting socket recv buffsize : %s", STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection established, using TCP send buffer size %d receive buffer size %d\n",
			repl_max_send_buffsize, repl_max_recv_buffsize);
	return (SS_NORMAL);
}

int gtmrecv_alloc_filter_buff(int bufsiz)
{
	unsigned char	*old_filter_buff, *free_filter_buff;

	bufsiz = ROUND_UP2(bufsiz, OS_PAGE_SIZE);
	if (NO_FILTER != gtmrecv_filter && repl_filter_bufsiz < bufsiz)
	{
		REPL_DPRINT3("Expanding filter buff from %d to %d\n", repl_filter_bufsiz, bufsiz);
		free_filter_buff = filterbuff;
		old_filter_buff = repl_filter_buff;
		filterbuff = (unsigned char *)malloc(bufsiz + OS_PAGE_SIZE);
		repl_filter_buff = (uchar_ptr_t)ROUND_UP2((unsigned long)filterbuff, OS_PAGE_SIZE);
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

void gtmrecv_free_filter_buff(void)
{
	if (NULL != filterbuff)
	{
		assert(NULL != repl_filter_buff);
		free(filterbuff);
		filterbuff = repl_filter_buff = NULL;
		repl_filter_bufsiz = 0;
	}
}

int gtmrecv_alloc_msgbuff(void)
{
	gtmrecv_max_repl_msglen = MAX_REPL_MSGLEN + SIZEOF(gtmrecv_msgp->type); /* add SIZEOF(...) for alignment */
	assert(NULL == gtmrecv_msgp); /* first time initialization. The receiver server doesn't need to re-allocate */
	msgbuff = (unsigned char *)malloc(gtmrecv_max_repl_msglen + OS_PAGE_SIZE);
	gtmrecv_msgp = (repl_msg_ptr_t)ROUND_UP2((unsigned long)msgbuff, OS_PAGE_SIZE);
	gtmrecv_alloc_filter_buff(gtmrecv_max_repl_msglen);
	return (SS_NORMAL);
}

void gtmrecv_free_msgbuff(void)
{
	if (NULL != msgbuff)
	{
		assert(NULL != gtmrecv_msgp);
		free(msgbuff);
		msgbuff = NULL;
		gtmrecv_msgp = NULL;
	}
}

static void process_tr_buff(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	seq_num			log_seqno, upd_seqno, diff_seqno;
	uint4			future_write, in_size, out_size, out_bufsiz, tot_out_size, save_buff_unprocessed,
				save_buffered_data_len, upd_read;
	boolean_t		filter_pass = FALSE;
	uchar_ptr_t		save_buffp, save_filter_buff, in_buff, out_buff;
	int			status;
	qw_num			msg_total;

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	do
	{
		if (write_loc + data_len > recvpool_size)
		{
#			ifdef REPL_DEBUG
			if (recvpool_ctl->wrapped)
				REPL_DPRINT1("Update Process too slow. Waiting for it to free up space and wrap\n");
#			endif
			while (recvpool_ctl->wrapped)
			{
				 /* Wait till the updproc wraps */
				SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
				gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
				if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
					return;
			}
			assert(recvpool_ctl->wrapped == FALSE);
			REPL_DPRINT3("Write wrapped%sat : %d\n", (NO_FILTER != gtmrecv_filter && !filter_pass) ?
				     " prior to filtering " : (NO_FILTER != gtmrecv_filter) ? " after filtering " : " ", write_loc);
			recvpool_ctl->write_wrap = write_wrap = write_loc;
			/* The update process reads (a) "recvpool_ctl->write" first. If "write" is not equal to
			 * "upd_proc_local->read", it then reads (b) "recvpool_ctl->write_wrap" and assumes that "write_wrap"
			 * holds a non-stale value. This is in turn used to compare "temp_read" and "write_wrap" to determine
			 * how much of unprocessed data there is in the receive pool. If it so happens that the receiver server
			 * sets "write_wrap" in the above line to a value that is lesser than its previous value (possible if
			 * in the previous wrap of the pool, transactions used more portions of the pool than in the current wrap),
			 * it is important that the update process sees the updated value of "write_wrap" as long as it sees the
			 * corresponding update to "write". This is because it will otherwise end up processing the tail section
			 * of the receive pool (starting from the uptodate value of "write" to the stale value of "write_wrap")
			 * that does not contain valid journal data. For this read order dependency to hold good, the receiver
			 * server needs to do a write memory barrier after updating "write_wrap" but before updating "write".
			 * The update process will do a read memory barrier after reading "wrapped" but before reading "write".
			 */
			SHM_WRITE_MEMORY_BARRIER;
			/* The update process looks at "recvpool_ctl->write" first and then reads (a) "recvpool_ctl->write_wrap"
			 * AND (b) all journal data in the receive pool upto this offset. It assumes that (a) and (b) will never
			 * hold stale values corresponding to a previous state of "recvpool_ctl->write". In order for this
			 * assumption to hold good, the receiver server needs to do a write memory barrier after updating the
			 * receive pool data and "write_wrap" but before updating "write". The update process will do a read
			 * memory barrier after reading "write" but before reading "write_wrap" or the receive pool data. Not
			 * enforcing the read order will result in the update process attempting to read/process invalid data
			 * from the receive pool (which could end up in db out of sync situation between primary and secondary).
			 */
			recvpool_ctl->write = write_loc = 0;
			SHM_WRITE_MEMORY_BARRIER;
			recvpool_ctl->wrapped = TRUE;
		}
		assert(buffered_data_len <= recvpool_size);
		do_flow_control(write_loc);
		if (repl_connection_reset)
		{
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset\n");
			repl_close(&gtmrecv_sock_fd);
			return;
		}
		if (gtmrecv_wait_for_jnl_seqno)
			return;
		future_write = write_loc + buffered_data_len;
		upd_read = upd_proc_local->read;
#		ifdef REPL_DEBUG
		if (recvpool_ctl->wrapped && (write_loc <= upd_read) && (upd_read <= future_write))
			REPL_DPRINT1("Update Process too slow. Waiting for it to free up space\n");
#		endif
		while (recvpool_ctl->wrapped && (write_loc <= upd_read) && (upd_read <= future_write))
		{	/* Write will cause overflow. Wait till there is more space available */
			SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
			upd_read = upd_proc_local->read;
		}
		memcpy(recvpool.recvdata_base + write_loc, buffp, buffered_data_len);
		write_loc = future_write;
		if (write_loc > write_wrap)
			write_wrap = write_loc;

		repl_recv_data_processed += (qw_num)buffered_data_len;
		buffp += buffered_data_len;
		buff_unprocessed -= buffered_data_len;
		data_len -= buffered_data_len;

		if (0 == data_len)
		{
			write_len = ((recvpool_ctl->write != write_wrap) ?
					(write_loc - recvpool_ctl->write) : write_loc);
			write_off = ((recvpool_ctl->write != write_wrap) ? recvpool_ctl->write : 0);
			if ((recvpool_ctl->jnl_seqno - lastlog_seqno >= log_interval)
				&& (NO_FILTER == gtmrecv_filter || filter_pass))
			{
				log_seqno = recvpool_ctl->jnl_seqno;
				upd_seqno = recvpool.upd_proc_local->read_jnl_seqno;
				assert(log_seqno >= upd_seqno);
				diff_seqno = (log_seqno - upd_seqno);
				trans_recvd_cnt += (log_seqno - lastlog_seqno);
				msg_total = repl_recv_data_recvd - buff_unprocessed;
					/* Don't include data not yet processed, we'll include that count in a later log */
				if (NO_FILTER == gtmrecv_filter)
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Seqno : "INT8_FMT" "INT8_FMTX
						"  Jnl Total : "INT8_FMT" "INT8_FMTX"  Msg Total : "INT8_FMT" "INT8_FMTX
						"  Current backlog : "INT8_FMT" "INT8_FMTX"\n",
						log_seqno, log_seqno, repl_recv_data_processed, repl_recv_data_processed,
						msg_total, msg_total, diff_seqno, diff_seqno);
				} else
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO - Seqno : "INT8_FMT" "INT8_FMTX
						"  Pre filter total : "INT8_FMT" "INT8_FMTX"  Post filter total : "
						INT8_FMT" "INT8_FMTX"  Msg Total : "INT8_FMT" "INT8_FMTX
						"  Current backlog : "INT8_FMT" "INT8_FMTX"\n",
						log_seqno, log_seqno, repl_recv_prefltr_data_procd, repl_recv_prefltr_data_procd,
						repl_recv_data_processed, repl_recv_data_processed,
						msg_total, msg_total, diff_seqno, diff_seqno);
				}
				/* Approximate time with an error not more than GTMRECV_HEARTBEAT_PERIOD. We use this instead of
				 * calling time(), and expensive system call, especially on VMS. The consequence of this choice
				 * is that we may defer logging when we may have logged. We can live with that. Currently, the
				 * logging interval is not changeable by users. When/if we provide means of choosing log interval,
				 * this code may have to be re-examined.
				 * Vinaya 2003, Sep 08
				 */
				assert(0 != gtmrecv_now);
				repl_recv_this_log_time = gtmrecv_now;
				assert(repl_recv_this_log_time >= repl_recv_prev_log_time);
				time_elapsed = difftime(repl_recv_this_log_time, repl_recv_prev_log_time);
				if ((double)GTMRECV_LOGSTATS_INTERVAL <= time_elapsed)
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO since last log : Time elapsed : %00.f  "
						 "Tr recvd : "INT8_FMT"  Tr bytes : "INT8_FMT"  Msg bytes : "INT8_FMT"\n",
						 time_elapsed, trans_recvd_cnt - last_log_tr_recvd_cnt,
						 repl_recv_data_processed - repl_recv_lastlog_data_procd,
						 msg_total - repl_recv_lastlog_data_recvd);
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO since last log : Time elapsed : %00.f  "
						 "Tr recvd/s : %f  Tr bytes/s : %f  Msg bytes/s : %f\n", time_elapsed,
						 (float)(trans_recvd_cnt - last_log_tr_recvd_cnt)/time_elapsed,
						 (float)(repl_recv_data_processed - repl_recv_lastlog_data_procd)/time_elapsed,
						 (float)(msg_total - repl_recv_lastlog_data_recvd)/time_elapsed);
					repl_recv_lastlog_data_procd = repl_recv_data_processed;
					repl_recv_lastlog_data_recvd = msg_total;
					last_log_tr_recvd_cnt = trans_recvd_cnt;
					repl_recv_prev_log_time = repl_recv_this_log_time;
				}
				lastlog_seqno = log_seqno;
			}
			if (gtmrecv_logstats && (NO_FILTER == gtmrecv_filter || filter_pass))
			{
				if (NO_FILTER == gtmrecv_filter)
					repl_log(gtmrecv_log_fp, FALSE, FALSE, "Tr : "INT8_FMT"  Size : %d  Write : %d  "
						 "Total : "INT8_FMT"\n", recvpool_ctl->jnl_seqno, write_len,
						 write_off, repl_recv_data_processed);
				else
					repl_log(gtmrecv_log_fp, FALSE, FALSE, "Tr : "INT8_FMT"  Pre filter Size : %d  "
						 "Post filter Size  : %d  Pre filter Write : %d  Post filter Write : %d  "
						 "Pre filter Total : "INT8_FMT"  Post filter Total : "INT8_FMT"\n",
						 recvpool_ctl->jnl_seqno, pre_filter_write_len, write_len,
						 pre_filter_write, write_off, repl_recv_prefltr_data_procd,
						 repl_recv_data_processed);
			}
			if ((NO_FILTER == gtmrecv_filter) || filter_pass)
			{
				recvpool_ctl->write_wrap = write_wrap;
				QWINCRBYDW(recvpool_ctl->jnl_seqno, 1);
				/* The update process looks at "recvpool_ctl->write" first and then reads
				 * (a) "recvpool_ctl->write_wrap" AND (b) all journal data in the receive pool upto this offset.
				 * It assumes that (a) and (b) will never hold stale values that reflect a corresponding previous
				 * state of "recvpool_ctl->write". In order for this assumption to hold good, the receiver server
				 * needs to do a write memory barrier after updating the receive pool data and "write_wrap" but
				 * before updating "write". The update process will do a read memory barrier after reading
				 * "write" but before reading "write_wrap" or the receive pool data. Not enforcing the read order
				 * will result in the update process attempting to read/process invalid data from the receive pool.
				 */
				SHM_WRITE_MEMORY_BARRIER;
				recvpool_ctl->write = write_loc;
				if (filter_pass)
				{	/* Switch buffers back */
					buffp = save_buffp;
					buff_unprocessed = save_buff_unprocessed;
					buffered_data_len = save_buffered_data_len;
					filter_pass = FALSE;
				}
			} else
			{
				pre_filter_write = write_off;
				pre_filter_write_len = write_len;
				repl_recv_prefltr_data_procd += (qw_num)pre_filter_write_len;
				if (gtmrecv_filter & INTERNAL_FILTER)
				{
					pre_intlfilter_datalen = write_len;
					in_buff = recvpool.recvdata_base + write_off;
					in_size = pre_intlfilter_datalen;
					out_buff = repl_filter_buff;
					out_bufsiz = repl_filter_bufsiz;
					tot_out_size = 0;
					while (SS_NORMAL != (status =
						repl_filter_old2cur[remote_jnl_ver - JNL_VER_EARLIEST_REPL](
							in_buff, &in_size, out_buff, &out_size, out_bufsiz)) &&
					       EREPL_INTLFILTER_NOSPC == repl_errno)
					{
						save_filter_buff = repl_filter_buff;
						gtmrecv_alloc_filter_buff(repl_filter_bufsiz + (repl_filter_bufsiz >> 1));
						in_buff += in_size;
						in_size = pre_filter_write_len - (in_buff - recvpool.recvdata_base - write_off);
						out_bufsiz = repl_filter_bufsiz - (out_buff - save_filter_buff) - out_size;
						out_buff = repl_filter_buff + (out_buff - save_filter_buff) + out_size;
						tot_out_size += out_size;
					}
					if (SS_NORMAL == status)
						write_len = tot_out_size + out_size;
					else
					{
						if (EREPL_INTLFILTER_BADREC == repl_errno)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JNLRECFMT);
						else if (EREPL_INTLFILTER_DATA2LONG == repl_errno)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JNLSETDATA2LONG, 2,
									jnl_source_datalen, jnl_dest_maxdatalen);
						else if (EREPL_INTLFILTER_NEWREC == repl_errno)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_JNLNEWREC, 2,
									(unsigned int)jnl_source_rectype,
									(unsigned int)jnl_dest_maxrectype);
						else if (EREPL_INTLFILTER_REPLGBL2LONG == repl_errno)
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REPLGBL2LONG);
						else if (EREPL_INTLFILTER_SECNODZTRIGINTP == repl_errno)
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SECNODZTRIGINTP, 1,
									&recvpool_ctl->jnl_seqno);
						else /* (EREPL_INTLFILTER_INCMPLREC == repl_errno) */
							GTMASSERT;
					}
				} else
				{
					if (write_len > repl_filter_bufsiz)
						gtmrecv_alloc_filter_buff(write_len);
					memcpy(repl_filter_buff, recvpool.recvdata_base + write_off, write_len);
				}
				assert(write_len <= repl_filter_bufsiz);
				if ((gtmrecv_filter & EXTERNAL_FILTER) &&
				    (SS_NORMAL != (status = repl_filter(recvpool_ctl->jnl_seqno, repl_filter_buff, (int*)&write_len,
									repl_filter_bufsiz))))
					repl_filter_error(recvpool_ctl->jnl_seqno, status);
				assert(write_len <= repl_filter_bufsiz);
				if (write_len > recvpool_size)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_REPLTRANS2BIG, 5, &recvpool_ctl->jnl_seqno,
						  write_len, 0, LEN_AND_LIT("Receive"), ERR_TEXT, 2,
						  LEN_AND_LIT("Post filter tr len larger than receive pool size"));

				/* Switch buffers */
				save_buffp = buffp;
				save_buff_unprocessed = buff_unprocessed;
				save_buffered_data_len = buffered_data_len;

				data_len = buff_unprocessed = buffered_data_len = write_len;
				buffp = repl_filter_buff;
				write_loc = write_off;
				repl_recv_data_processed -= (qw_num)pre_filter_write_len;
				filter_pass = TRUE;
			}
		} else
			filter_pass = FALSE;
	} while (NO_FILTER != gtmrecv_filter && filter_pass);
	return;
}

static void do_main_loop(boolean_t crash_restart)
{
	/* The work-horse of the Receiver Server */

	void		do_flow_control();

	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	seq_num			request_from, recvd_jnl_seqno;
	int			skip_for_alignment, msg_type, msg_len;
	unsigned char		*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int			status;					/* needed for REPL_{SEND,RECV}_LOOP */
	char			print_msg[1024];
	repl_heartbeat_msg_t	heartbeat;
	repl_start_reply_msg_t	*start_msg;
	uint4			recvd_start_flags;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	gtmrecv_wait_for_jnl_seqno = FALSE;

	if (!gtmrecv_bad_trans_sent)
	{
		/* Wait for the Update Process to  write start_jnl_seqno */
		repl_log(gtmrecv_log_fp, FALSE, FALSE, "Waiting for Update Process to write jnl_seqno\n");
		while (QWEQ(recvpool_ctl->jnl_seqno, seq_num_zero))
		{
			SHORT_SLEEP(GTMRECV_WAIT_FOR_STARTJNLSEQNO);
			gtmrecv_poll_actions(0, 0, NULL);
			if (repl_connection_reset)
				return;
		}
		secondary_side_std_null_coll = recvpool_ctl->std_null_coll;
		secondary_side_trigger_support = FALSE;
		gtmrecv_wait_for_jnl_seqno = FALSE;
		if (QWEQ(recvpool_ctl->start_jnl_seqno, seq_num_zero))
			QWASSIGN(recvpool_ctl->start_jnl_seqno, recvpool_ctl->jnl_seqno);
		repl_log(gtmrecv_log_fp, FALSE, TRUE, "Requesting transactions from JNL_SEQNO "INT8_FMT"\n",
			recvpool_ctl->jnl_seqno);
		QWASSIGN(request_from, recvpool_ctl->jnl_seqno);
		/* Send (re)start JNL_SEQNO to Source Server */
		gtmrecv_msgp->type = REPL_START_JNL_SEQNO;
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags = START_FLAG_NONE;
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags |=
			(gtmrecv_options.stopsourcefilter ? START_FLAG_STOPSRCFILTER : 0);
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags |= (gtmrecv_options.updateresync ? START_FLAG_UPDATERESYNC : 0);
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags |= START_FLAG_HASINFO;
		if (secondary_side_std_null_coll)
			((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags |= START_FLAG_COLL_M;
		((repl_start_msg_ptr_t)gtmrecv_msgp)->jnl_ver = jnl_ver;
		QWASSIGN(*(seq_num *)&((repl_start_msg_ptr_t)gtmrecv_msgp)->start_seqno[0], request_from);
		gtmrecv_msgp->len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, gtmrecv_msgp, gtmrecv_msgp->len, REPL_POLL_NOWAIT)
		{
			gtmrecv_poll_actions(0, 0, NULL);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}
		if (SS_NORMAL != status)
		{
			if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
			{
				repl_close(&gtmrecv_sock_fd);
				repl_connection_reset = TRUE;
				sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_LIT("Connection closed"));
				repl_log(gtmrecv_log_fp, TRUE, TRUE, print_msg);
				gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "ERR_REPLWARN", print_msg);
				return;
			}
			if (EREPL_SEND == repl_errno)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending (re)start jnlseqno. Error in send : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
			if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending (re)start jnlseqno. Error in select : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
		}
	}
	gtmrecv_bad_trans_sent = FALSE;
	request_from = recvpool_ctl->jnl_seqno;
	assert(request_from >= seq_num_one);
	gtmrecv_reinit_logseqno();

	repl_log(gtmrecv_log_fp, FALSE, TRUE, "Waiting for WILL_START or ROLL_BACK_FIRST message\n");

	/* Receive journal data and place it in the Receive Pool */
	buff_start = (unsigned char *)gtmrecv_msgp;
	buffp = buff_start;
	buff_unprocessed = 0;
	data_len = 0;
	skip_for_alignment = 0;
	write_loc = recvpool_ctl->write;
	write_wrap = recvpool_ctl->write_wrap;

	repl_recv_data_recvd = 0;
	repl_recv_data_processed = 0;
	repl_recv_prefltr_data_procd = 0;
	repl_recv_lastlog_data_recvd = 0;
	repl_recv_lastlog_data_procd = 0;

	while (TRUE)
	{
		recvd_len = gtmrecv_max_repl_msglen - buff_unprocessed - skip_for_alignment;
		while ((status = repl_recv(gtmrecv_sock_fd, (buffp + buff_unprocessed), &recvd_len, REPL_POLL_WAIT))
			       == SS_NORMAL && recvd_len == 0)
		{
			recvd_len = gtmrecv_max_repl_msglen - buff_unprocessed - skip_for_alignment;
			if (xoff_sent)
				do_flow_control(write_loc);
			if (xoff_sent && GTMRECV_XOFF_LOG_CNT <= xoff_msg_log_cnt)
			{
				/* update process is still running slow, gtmrecv_poll_interval is now 0.
				 * Force wait before logging any message.
				 */
				SHORT_SLEEP(REPL_POLL_WAIT >> 10); /* approximate in ms */
				REPL_DPRINT1("Waiting for Update Process to clear recvpool space\n");
				xoff_msg_log_cnt = 0;
			} else if (xoff_sent)
				xoff_msg_log_cnt++;

			if (repl_connection_reset)
			{
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset\n");
				repl_close(&gtmrecv_sock_fd);
				return;
			}
			if (gtmrecv_wait_for_jnl_seqno)
				return;
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}

		if (SS_NORMAL != status)
		{
			if (EREPL_RECV == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset. Status = %d ; %s\n",
							status, STRERROR(status));
					repl_connection_reset = TRUE;
					repl_close(&gtmrecv_sock_fd);
					return;
				} else
				{
					SNPRINTF(print_msg, SIZEOF(print_msg), "Error in receiving from source. "
							"Error in recv : %s", STRERROR(status));
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
							LEN_AND_STR(print_msg));
				}
			} else if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Error in receiving from source. Error in select : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
		}

		if (repl_connection_reset)
			return;

		/* Something on the replication pipe - read it */

		REPL_DPRINT3("Pending data len : %d  Prev buff unprocessed : %d\n", data_len, buff_unprocessed);

		buff_unprocessed += recvd_len;
		repl_recv_data_recvd += (qw_num)recvd_len;

		if (gtmrecv_logstats)
			repl_log(gtmrecv_log_fp, FALSE, FALSE, "Recvd : %d  Total : %d\n", recvd_len, repl_recv_data_recvd);

		while (REPL_MSG_HDRLEN <= buff_unprocessed)
		{
			if (0 == data_len)
			{
				assert(0 == ((unsigned long)buffp & (SIZEOF(((repl_msg_ptr_t)buffp)->type) - 1)));
				msg_type = ((repl_msg_ptr_t)buffp)->type;
				msg_len = data_len = ((repl_msg_ptr_t)buffp)->len - REPL_MSG_HDRLEN;
				assert(0 == (msg_len & ((SIZEOF(((repl_msg_ptr_t)buffp)->type)) - 1)));
				buffp += REPL_MSG_HDRLEN;
				buff_unprocessed -= REPL_MSG_HDRLEN;

				if (data_len > recvpool_size)
				{
					/* Too large a transaction to be
					 * accommodated in the Receive Pool */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLTRANS2BIG, 5, &recvpool_ctl->jnl_seqno,
						  data_len, 0, LEN_AND_LIT("Receive"));
				}
			}
			buffered_data_len = ((data_len <= buff_unprocessed) ? data_len : buff_unprocessed);
			switch(msg_type)
			{
				case REPL_TR_JNL_RECS:
					process_tr_buff();
					if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
						return;
					break;

				case REPL_HEARTBEAT:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{
						/* Heartbeat msg contents start from buffp - msg_len */
						memcpy(heartbeat.ack_seqno, buffp - msg_len, msg_len);
						REPL_DPRINT4("HEARTBEAT received with time %ld SEQNO "INT8_FMT" at %ld\n",
							     *(time_t *)&heartbeat.ack_time[0],
							     (*(seq_num *)&heartbeat.ack_seqno[0]), time(NULL));
						heartbeat.type = REPL_HEARTBEAT;
						heartbeat.len = MIN_REPL_MSGLEN;
						QWASSIGN(*(seq_num *)&heartbeat.ack_seqno[0], upd_proc_local->read_jnl_seqno);
						REPL_SEND_LOOP(gtmrecv_sock_fd, &heartbeat, heartbeat.len, REPL_POLL_NOWAIT)
						{
							gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
							if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
								return;
						}
						/* Error handling for the above send_loop is not required as it'll be caught
						 * in the next recv_loop of the receiver server */
						REPL_DPRINT4("HEARTBEAT sent with time %ld SEQNO "INT8_FMT" at %ld\n",
							     *(time_t *)&heartbeat.ack_time[0],
							     (*(seq_num *)&heartbeat.ack_seqno[0]), time(NULL));
					}
					break;

				case REPL_WILL_RESTART_WITH_INFO:
				case REPL_ROLLBACK_FIRST:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						start_msg = (repl_start_reply_msg_t *)(buffp - msg_len - REPL_MSG_HDRLEN);
						assert((unsigned long)start_msg % SIZEOF(seq_num) == 0); /* alignment check */
						QWASSIGN(recvd_jnl_seqno, *(seq_num *)start_msg->start_seqno);
						if (REPL_WILL_RESTART_WITH_INFO == msg_type)
						{
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received WILL_START message. "
											     "Primary acked the restart point\n");
							remote_jnl_ver = start_msg->jnl_ver;
							REPL_DPRINT3("Local jnl ver is octal %o, remote jnl ver is octal %o\n",
								     jnl_ver, remote_jnl_ver);
							repl_check_jnlver_compat();
							/* older versions zero filler that was in place of start_msg->start_flags,
							 * so we are okay fetching start_msg->start_flags unconditionally.
							 */
							GET_ULONG(recvd_start_flags, start_msg->start_flags);
							assert(remote_jnl_ver > V15_JNL_VER || 0 == recvd_start_flags);
							if (remote_jnl_ver <= V15_JNL_VER) /* safety in pro */
								recvd_start_flags = 0;
							primary_side_std_null_coll = (recvd_start_flags & START_FLAG_COLL_M) ?
								TRUE : FALSE;
							if (FALSE != ((TREF(replgbl)).null_subs_xform =
									((primary_side_std_null_coll &&
										!secondary_side_std_null_coll)
									|| (secondary_side_std_null_coll &&
										!primary_side_std_null_coll))))
								(TREF(replgbl)).null_subs_xform = (primary_side_std_null_coll ?
									STDNULL_TO_GTMNULL_COLL : GTMNULL_TO_STDNULL_COLL);
								/* this sets null_subs_xform regardless of remote_jnl_ver */
							primary_side_trigger_support
								= (recvd_start_flags & START_FLAG_TRIGGER_SUPPORT) ? TRUE : FALSE;
							if ((jnl_ver > remote_jnl_ver)
								&& (IF_NONE != repl_filter_old2cur[remote_jnl_ver
													- JNL_VER_EARLIEST_REPL]))
							{
								assert(IF_INVALID !=
								    repl_filter_old2cur[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
								/* reverse transformation should exist */
								assert(IF_INVALID !=
								    repl_filter_cur2old[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
								assert(IF_NONE !=
								    repl_filter_cur2old[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
								gtmrecv_filter |= INTERNAL_FILTER;
								gtmrecv_alloc_filter_buff(gtmrecv_max_repl_msglen);
							} else
							{
								gtmrecv_filter &= ~INTERNAL_FILTER;
								if (NO_FILTER == gtmrecv_filter)
									gtmrecv_free_filter_buff();
							}
							/* Don't send any more stopsourcefilter message */
							gtmrecv_options.stopsourcefilter = FALSE;
							assert(QWEQ(recvd_jnl_seqno, request_from));
							break;
						}
						repl_log(gtmrecv_log_fp, TRUE, FALSE, "ROLLBACK_FIRST message received. Secondary "
							 "ahead of primary. Secondary at "INT8_FMT, request_from);
						repl_log(gtmrecv_log_fp, FALSE, TRUE, ", primary at "INT8_FMT". "
							 "Do ROLLBACK FIRST\n", recvd_jnl_seqno);
						gtmrecv_autoshutdown();
					}
					break;

				default:
					/* Discard the message */
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Message of unknown type (%d) received\n", msg_type);
					assert(FALSE);
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					break;
			}
			if (repl_connection_reset)
				return;
		}
		skip_for_alignment = (int)((unsigned long)buffp & (SIZEOF(((repl_msg_ptr_t)buffp)->type) - 1));
		if (0 != buff_unprocessed)
		{
			REPL_DPRINT4("Incmpl msg hdr, moving %d bytes from %lx to %lx\n", buff_unprocessed, (caddr_t)buffp,
				     (caddr_t)buff_start + skip_for_alignment);
			memmove(buff_start + skip_for_alignment, buffp, buff_unprocessed);
		}
		buffp = buff_start + skip_for_alignment;

		gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
		if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
			return;
	}
}

static void gtmrecv_heartbeat_timer(TID tid, int4 interval_len, int *interval_ptr)
{
	assert(0 != gtmrecv_now);
	UNIX_ONLY(assert(*interval_ptr == heartbeat_period);)	/* interval_len and interval_ptr are dummies on VMS */
	gtmrecv_now += heartbeat_period;
	REPL_DPRINT2("Starting heartbeat timer with %d s\n", heartbeat_period);
	start_timer((TID)gtmrecv_heartbeat_timer, heartbeat_period * 1000, gtmrecv_heartbeat_timer, SIZEOF(heartbeat_period),
			&heartbeat_period); /* start_timer expects time interval in milli seconds, heartbeat_period is in seconds */
}

static void gtmrecv_main_loop(boolean_t crash_restart)
{
	assert(FD_INVALID == gtmrecv_sock_fd);
	gtmrecv_poll_actions(0, 0, NULL); /* Clear any pending bad trans */
	gtmrecv_est_conn();
	gtmrecv_bad_trans_sent = FALSE; /* this assignment should be after gtmrecv_est_conn since gtmrecv_est_conn can
					 * potentially call gtmrecv_poll_actions. If the timing is right,
					 * gtmrecv_poll_actions might set this variable to TRUE if the update process sets
					 * bad_trans in the recvpool. When we are (re)establishing connection with the
					 * source server, there is no point in doing bad trans processing. Also, we have
					 * to send START_JNL_SEQNO message to the source server. If not, there will be a
					 * deadlock with the source and receiver servers waiting for each other to send
					 * a message. */
	repl_recv_prev_log_time = gtmrecv_now;
	while (!repl_connection_reset)
		do_main_loop(crash_restart);
	return;
}

void gtmrecv_process(boolean_t crash_restart)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	void 			gtmrecv_heartbeat_timer();

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;

	jnl_ver = JNL_VER_THIS;
	assert(REPL_POLL_WAIT < MILLISECS_IN_SEC);
	recvpool_size = recvpool_ctl->recvpool_size;
	recvpool_high_watermark = (long)((float)RECVPOOL_HIGH_WATERMARK_PCTG / 100 * recvpool_size);
	recvpool_low_watermark  = (long)((float)RECVPOOL_LOW_WATERMARK_PCTG  / 100 * recvpool_size);
	if ((long)((float)(RECVPOOL_HIGH_WATERMARK_PCTG - RECVPOOL_LOW_WATERMARK_PCTG) / 100 * recvpool_size) >=
			RECVPOOL_XON_TRIGGER_SIZE)
	{ /* for large receive pools, the difference between high and low watermarks as computed above may be too large that
	   * we may not send XON quickly enough. Limit the difference to RECVPOOL_XON_TRIGGER_SIZE */
		recvpool_low_watermark = recvpool_high_watermark - RECVPOOL_XON_TRIGGER_SIZE;
	}
	REPL_DPRINT4("RECVPOOL HIGH WATERMARK is %ld, LOW WATERMARK is %ld, Receive pool size is %ld\n",
			recvpool_high_watermark, recvpool_low_watermark, recvpool_size);
	gtmrecv_alloc_msgbuff();
	gtmrecv_now = time(NULL);
	heartbeat_period = GTMRECV_HEARTBEAT_PERIOD; /* time keeper, well sorta */
	start_timer((TID)gtmrecv_heartbeat_timer, heartbeat_period * 1000, gtmrecv_heartbeat_timer, SIZEOF(heartbeat_period),
			&heartbeat_period); /* start_timer expects time interval in milli seconds, heartbeat_period is in seconds */
	do
	{
		gtmrecv_main_loop(crash_restart);
	} while (repl_connection_reset);
	GTMASSERT; /* shouldn't reach here */
	return;
}
