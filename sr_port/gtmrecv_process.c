/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_socket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#include "gtm_string.h"
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
#include "gtm_stdio.h"
#include "gtm_event_log.h"
#include "eintr_wrappers.h"
#include "jnl.h"
#include "repl_sp.h"
#include "repl_filter.h"
#include "repl_log.h"
#include "gtmsource.h"
#include "sgtm_putmsg.h"

#define RECVBUFF_REPLMSGLEN_FACTOR 		8

#define GTMRECV_POLL_INTERVAL			(1000000 - 1)/* micro sec, almost 1 sec */
#define MAX_GTMRECV_POLL_INTERVAL		1000000 /* 1 sec in micro sec */

#define GTMRECV_WAIT_FOR_STARTJNLSEQNO		1 /* ms */

#define GTMRECV_WAIT_FOR_UPD_PROGRESS		1 /* ms */
#define GTMRECV_WAIT_FOR_UPD_PROGRESS_US	GTMRECV_WAIT_FOR_UPD_PROGRESS * 1000 /* micro sec */

#define RECVPOOL_HIGH_WATERMARK_PCTG		90	/* Send XOFF when receive pool
					   		 * space occupied goes beyonf this
					   		 * percentage
					   		 */
#define GTMRECV_XOFF_LOG_CNT			100

#define LOGTRNUM_INTERVAL			100

#define GTMRECV_LOGSTATS_INTERVAL		10 /* sec */

GBLDEF	repl_msg_ptr_t		gtmrecv_msgp;
GBLDEF	int			gtmrecv_max_repl_msglen;
GBLDEF	struct timeval		gtmrecv_poll_interval, gtmrecv_poll_immediate;
GBLDEF	int			gtmrecv_sock_fd = -1;
GBLDEF	boolean_t		repl_connection_reset = FALSE;
GBLDEF	boolean_t		gtmrecv_wait_for_jnl_seqno = FALSE;
GBLDEF	boolean_t		gtmrecv_bad_trans_sent = FALSE;
GBLDEF	struct sockaddr_in	primary_addr;

GBLDEF	long			repl_recv_data_recvd = 0;
GBLDEF	long			repl_recv_data_processed = 0;
GBLDEF	long			repl_recv_prefltr_data_procd = 0;
GBLDEF	long			repl_recv_lastlog_data_recvd = 0;
GBLDEF	long			repl_recv_lastlog_data_procd = 0;

GBLDEF	time_t			repl_recv_prev_log_time;
GBLDEF	time_t			repl_recv_this_log_time;

GBLREF  gtmrecv_options_t	gtmrecv_options;
GBLREF	int			gtmrecv_listen_sock_fd;
GBLREF	recvpool_addrs		recvpool;
GBLREF  boolean_t               gtmrecv_logstats;
GBLREF	int			gtmrecv_filter;
GBLREF	int			gtmrecv_log_fd;
GBLREF	int			gtmrecv_statslog_fd;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	FILE			*gtmrecv_statslog_fp;
GBLREF	seq_num			seq_num_zero, seq_num_one, seq_num_minus_one;
GBLREF	uint4			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;

static	uchar_ptr_t	buffp;
static	uchar_ptr_t	buff_start;
static	int		buff_unprocessed;
static	int		buffered_data_len;
static	int		max_recv_bufsiz;
static	int		data_len;
static 	boolean_t	xoff_sent;
static	repl_msg_t	xon_msg, xoff_msg;
static	int		xoff_msg_log_cnt = 0;
static	long		recvpool_high_watermark;
static	uint4		write_loc, write_wrap;
static	seq_num		lastlog_seqno;
static  uint4		write_len, write_off,
			pre_filter_write_len, pre_filter_write, pre_intlfilter_datalen;
static	long		trans_recvd_cnt = 0;
static	long		last_log_tr_recvd_cnt = 0;
static	double		time_elapsed;

static void do_flow_control(uint4 write_pos)
{
	/* Check for overflow before writing */

	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	long			space_used;
	unsigned char		*msg_ptr;
	int			status, send_len, sent_len, recv_len, recvd_len, read_pos;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	space_used = 0;
	if (recvpool_ctl->wrapped)
		space_used = write_pos + recvpool_ctl->recvpool_size - (read_pos = upd_proc_local->read);
	if (!recvpool_ctl->wrapped || space_used > recvpool_ctl->recvpool_size)
		space_used = write_pos - (read_pos = upd_proc_local->read);
	if (space_used >= recvpool_high_watermark && !xoff_sent)
	{
		/* Send XOFF message */
		xoff_msg.type = REPL_XOFF;
		memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, sizeof(seq_num));
		xoff_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xoff_msg, xoff_msg.len, &gtmrecv_poll_immediate)
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
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending XOFF msg. Error in send"), status);
			if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending XOFF msg. Error in select"), status);
		}
		if (gtmrecv_logstats)
			repl_log(gtmrecv_statslog_fp, TRUE, TRUE, "Space used = %ld, High water mark = %d Updproc Read = %d, "
				 "Recv Write = %d, Sent XOFF\n", space_used, recvpool_high_watermark, read_pos, write_pos);
		xoff_sent = TRUE;
		xoff_msg_log_cnt = 1;
		assert(GTMRECV_WAIT_FOR_UPD_PROGRESS_US < MAX_GTMRECV_POLL_INTERVAL);
		gtmrecv_poll_interval.tv_sec = 0;
		gtmrecv_poll_interval.tv_usec = GTMRECV_WAIT_FOR_UPD_PROGRESS_US;
	} else if (space_used < recvpool_high_watermark && xoff_sent)
	{
		xon_msg.type = REPL_XON;
		memcpy((uchar_ptr_t)&xon_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, sizeof(seq_num));
		xon_msg.len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xon_msg, xon_msg.len, &gtmrecv_poll_immediate)
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
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending XON msg. Error in send"), status);
			if (EREPL_SELECT == repl_errno)
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending XON msg. Error in select"), status);
		}
		if (gtmrecv_logstats)
			repl_log(gtmrecv_statslog_fp, TRUE, TRUE, "Space used now = %ld, High water mark = %d, "
				 "Updproc Read = %d, Recv Write = %d, Sent XON\n", space_used, recvpool_high_watermark,
				 read_pos, write_pos);
		xoff_sent = FALSE;
		xoff_msg_log_cnt = 0;
		gtmrecv_poll_interval.tv_sec = 0;
		gtmrecv_poll_interval.tv_usec = GTMRECV_POLL_INTERVAL;
	}
	return;
}

static int gtmrecv_est_conn(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	size_t			primary_addr_len;
	fd_set			input_fds;
	int			status;
	const   int     	disable_keepalive = 0;
	struct  linger  	disable_linger = {0, 0};
        struct  timeval 	save_gtmrecv_poll_interval;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	/*
	 * Wait for a connection from a Source Server.
	 * The Receiver Server is an iterative server.
	 */

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;

	gtmrecv_comm_init((in_port_t)gtmrecv_local->listen_port);
	primary_addr_len = sizeof(primary_addr);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for a connection...\n");
	FD_ZERO(&input_fds);
	FD_SET(gtmrecv_listen_sock_fd, &input_fds);
	/*
	 * Note - the following while loop checks for EINTR on the select. The
	 * SELECT macro is not used because the FD_SET is redone before the new
	 * call to select (after the continue).
	 */
        save_gtmrecv_poll_interval = gtmrecv_poll_interval;
	while (0 >= (status = select(gtmrecv_listen_sock_fd + 1, &input_fds, NULL, NULL, &gtmrecv_poll_interval)))
	{
                gtmrecv_poll_interval = save_gtmrecv_poll_interval;
		FD_SET(gtmrecv_listen_sock_fd, &input_fds);
		if (0 == status)
			gtmrecv_poll_actions(0, 0, NULL);
		else if (EINTR == errno || EAGAIN == errno)
			continue;
		else
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				  RTS_ERROR_LITERAL("Error in select on listen socket"), ERRNO);
	}
        gtmrecv_poll_interval = save_gtmrecv_poll_interval;
	ACCEPT_SOCKET(gtmrecv_listen_sock_fd, (struct sockaddr *)&primary_addr, (sssize_t *)&primary_addr_len, gtmrecv_sock_fd);
	if (0 > gtmrecv_sock_fd)
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
		          RTS_ERROR_LITERAL("Error accepting connection from Source Server"), ERRNO);

	/* Connection established */
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection established\n");

	/* Close the listener socket */
	repl_close(&gtmrecv_listen_sock_fd);

	repl_connection_reset = FALSE;

	if (0 > setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, sizeof(disable_linger)))
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error with receiver server socket disable linger"), ERRNO);

#ifdef REPL_DISABLE_KEEPALIVE
	if (0 > setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&disable_keepalive, sizeof(disable_keepalive)))
	{
		/* Till SIGPIPE is handled properly */
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error with receiver server socket disable keepalive"), ERRNO);
	}
#endif
	if (0 > get_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize, &repl_max_recv_buffsize))
	{
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error getting socket send/recv buffsizes"), ERRNO);
		return (!SS_NORMAL);
	}

	return (SS_NORMAL);
}

static int gtmrecv_alloc_filter_buff(int bufsiz)
{
	uchar_ptr_t	old_filter_buff;

	if (NO_FILTER != gtmrecv_filter && repl_filter_bufsiz < bufsiz)
	{
		REPL_DPRINT3("Expanding filter buff from %d to %d\n", repl_filter_bufsiz, bufsiz);
		old_filter_buff = repl_filter_buff;
		repl_filter_buff = (uchar_ptr_t)malloc(bufsiz);
		if (old_filter_buff)
		{
			memcpy(repl_filter_buff, old_filter_buff, repl_filter_bufsiz);
			free(old_filter_buff);
		}
		repl_filter_bufsiz = bufsiz;
	}

	return (SS_NORMAL);
}

static int gtmrecv_alloc_msgbuff(void)
{
	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	/* Get the negotiated max TCP buffer size */
	gtmrecv_max_repl_msglen = MAX_REPL_MSGLEN + sizeof(gtmrecv_msgp->type); /* for now use MAX_REPL_MSGLEN;
										 * add sizeof(...) for alignment */
	if ((unsigned int)gtmrecv_max_repl_msglen > (unsigned int)((unsigned char *)-1))/* TCP can handle more than my max
											 * buffer size */
	{
		assert((unsigned int)MAX_REPL_MSGLEN <= (unsigned int)((unsigned char *)-1));
		gtmrecv_max_repl_msglen = MAX_REPL_MSGLEN + sizeof(gtmrecv_msgp->type);
	}
	max_recv_bufsiz = (RECVBUFF_REPLMSGLEN_FACTOR * gtmrecv_max_repl_msglen);

	if (gtmrecv_msgp) /* Free any existing buffers */
		free(gtmrecv_msgp);
	/* Allocate msg buffer */
	gtmrecv_msgp = (repl_msg_ptr_t)malloc(gtmrecv_max_repl_msglen);
	gtmrecv_alloc_filter_buff(gtmrecv_max_repl_msglen);

	return (SS_NORMAL);
}

static void process_tr_buff(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	seq_num			log_seqno;
	uint4			future_write, in_size, out_size, out_bufsiz, tot_out_size, save_buff_unprocessed,
				save_buffered_data_len;
	boolean_t		filter_pass = FALSE;
	uchar_ptr_t		save_buffp, save_filter_buff, in_buff, out_buff;
	int			status;
	unsigned char		seq_num_str[32], *seq_num_ptr;

	error_def(ERR_REPLTRANS2BIG);
	error_def(ERR_TEXT);
	error_def(ERR_JNLRECFMT);
	error_def(ERR_JNLSETDATA2LONG);
	error_def(ERR_JNLNEWREC);

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	do
	{
		if (write_loc + data_len > recvpool_ctl->recvpool_size)
		{
#ifdef REPL_DEBUG
			if (recvpool_ctl->wrapped)
				REPL_DPRINT1("Update Process too slow. Waiting for it to free up space and wrap\n");
#endif
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
			recvpool_ctl->write = write_loc = 0;
			recvpool_ctl->wrapped = TRUE;
		}

		assert(buffered_data_len <= recvpool_ctl->recvpool_size);
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

#ifdef REPL_DEBUG
		if (recvpool_ctl->wrapped && write_loc <= upd_proc_local->read && upd_proc_local->read <= future_write)
			REPL_DPRINT1("Update Process too slow. Waiting for it to free up space\n");
#endif
		while (recvpool_ctl->wrapped && write_loc <= upd_proc_local->read && upd_proc_local->read <= future_write)
		{
			/* Write will cause overflow. Wait till there is
			 * more space available */
			SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}
		memcpy(recvpool.recvdata_base + write_loc, buffp, buffered_data_len);
		write_loc = future_write;
		if (write_loc > write_wrap)
			write_wrap = write_loc;

		repl_recv_data_processed += buffered_data_len;
		buffp += buffered_data_len;
		buff_unprocessed -= buffered_data_len;
		data_len -= buffered_data_len;

		if (0 == data_len)
		{
			write_len = ((recvpool_ctl->write != write_wrap) ?
					(write_loc - recvpool_ctl->write) : write_loc);
			write_off = ((recvpool_ctl->write != write_wrap) ? recvpool_ctl->write : 0);
			QWASSIGN(log_seqno, lastlog_seqno);
			QWINCRBYDW(log_seqno, LOGTRNUM_INTERVAL);
			if (QWLE(log_seqno, recvpool_ctl->jnl_seqno) && (NO_FILTER == gtmrecv_filter || filter_pass))
			{
				QWASSIGN(log_seqno, recvpool_ctl->jnl_seqno);
				trans_recvd_cnt += LOGTRNUM_INTERVAL;
				if (NO_FILTER == gtmrecv_filter)
					repl_log(gtmrecv_log_fp, FALSE, TRUE, "REPL INFO - Tr num : "INT8_FMT\
						"  Tr Total : %ld  Msg Total : %ld\n", INT8_PRINT(log_seqno),
						repl_recv_data_processed, repl_recv_data_recvd - buff_unprocessed);
				else
					repl_log(gtmrecv_log_fp, FALSE, TRUE, "REPL INFO - Tr num : "INT8_FMT\
						"  Pre filter total : %ld  Post filter total : %ld  Msg Total : %ld\n",
						INT8_PRINT(log_seqno), repl_recv_prefltr_data_procd, repl_recv_data_processed,
						repl_recv_data_recvd - buff_unprocessed);

				repl_recv_this_log_time = time(NULL);
				time_elapsed = difftime(repl_recv_this_log_time, repl_recv_prev_log_time);
				if ((double)GTMRECV_LOGSTATS_INTERVAL <= time_elapsed)
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO since last log : Time elapsed : %00.f  "
						 "Tr recvd : %ld  Tr bytes : %ld  Msg bytes : %ld\n", time_elapsed,
						 trans_recvd_cnt - last_log_tr_recvd_cnt,
						 repl_recv_data_processed - repl_recv_lastlog_data_procd,
						 repl_recv_data_recvd - repl_recv_lastlog_data_recvd);
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO since last log : Time elapsed : %00.f  "
						 "Tr recvd/s : %f  Tr bytes/s : %f  Msg bytes/s : %f\n", time_elapsed,
						 (float)(trans_recvd_cnt - last_log_tr_recvd_cnt)/time_elapsed,
						 (float)(repl_recv_data_processed - repl_recv_lastlog_data_procd)/time_elapsed,
						 (float)(repl_recv_data_recvd - repl_recv_lastlog_data_recvd)/time_elapsed);
					repl_recv_lastlog_data_procd = repl_recv_data_processed;
					repl_recv_lastlog_data_recvd = repl_recv_data_recvd;
					last_log_tr_recvd_cnt = trans_recvd_cnt;
					repl_recv_prev_log_time = repl_recv_this_log_time;
				}
				QWASSIGN(lastlog_seqno, log_seqno);
			}
			if (gtmrecv_logstats && (NO_FILTER == gtmrecv_filter || filter_pass))
			{
				if (NO_FILTER == gtmrecv_filter)
					repl_log(gtmrecv_statslog_fp, FALSE, FALSE, "Tr : "INT8_FMT"  Size : %d  Write : %d  "
						 "Total : %d\n", INT8_PRINT(recvpool_ctl->jnl_seqno), write_len,
						 write_off, repl_recv_data_processed);
				else
					repl_log(gtmrecv_statslog_fp, FALSE, FALSE, "Tr : "INT8_FMT"  Pre filter Size : %d  "
						 "Post filter Size  : %d  Pre filter Write : %d  Post filter Write : %d  "
						 "Pre filter Total : %d  Post filter Total : %d\n",
						 INT8_PRINT(recvpool_ctl->jnl_seqno), pre_filter_write_len, write_len,
						 pre_filter_write, write_off, repl_recv_prefltr_data_procd,
						 repl_recv_data_processed);
			}
			if (NO_FILTER == gtmrecv_filter || filter_pass)
			{
				recvpool_ctl->write_wrap = write_wrap;
				QWINCRBYDW(recvpool_ctl->jnl_seqno, 1);
				recvpool_ctl->write = write_loc;
				if (NO_FILTER != gtmrecv_filter)
				{
					/* Switch buffers back */
					buffp = save_buffp;
					buff_unprocessed = save_buff_unprocessed;
					buffered_data_len = save_buffered_data_len;
					filter_pass = FALSE;
				}
			} else
			{
				pre_filter_write = write_off;
				pre_filter_write_len = write_len;
				repl_recv_prefltr_data_procd += pre_filter_write_len;
				if (gtmrecv_filter & INTERNAL_FILTER)
				{
					pre_intlfilter_datalen = write_len;
					in_buff = recvpool.recvdata_base + write_off;
					in_size = pre_intlfilter_datalen;
					out_buff = repl_filter_buff;
					out_bufsiz = repl_filter_bufsiz;
					tot_out_size = 0;
					while (-1 == (status =
						repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
								    [jnl_ver - JNL_VER_EARLIEST_REPL](
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
					if (0 == status)
						write_len = tot_out_size + out_size;
					else
					{
						if (EREPL_INTLFILTER_BADREC == repl_errno)
							rts_error(VARLSTCNT(1) ERR_JNLRECFMT);
						else if (EREPL_INTLFILTER_DATA2LONG == repl_errno)
							rts_error(VARLSTCNT(4) ERR_JNLSETDATA2LONG, 2, jnl_source_datalen,
								  jnl_dest_maxdatalen);
						else if (EREPL_INTLFILTER_NEWREC == repl_errno)
							rts_error(VARLSTCNT(4) ERR_JNLNEWREC, 2, (unsigned int)jnl_source_rectype,
								  (unsigned int)jnl_dest_maxrectype);
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
				if (write_len > recvpool_ctl->recvpool_size)
				{
					seq_num_ptr = i2ascl(seq_num_str, recvpool_ctl->jnl_seqno);
					rts_error(VARLSTCNT(11) ERR_REPLTRANS2BIG, 5, seq_num_ptr - &seq_num_str[0], seq_num_str,
						  write_len, RTS_ERROR_LITERAL("Receive"), ERR_TEXT, 2,
						  LEN_AND_LIT("Post filter tr len larger than receive pool size"));
				}

				/* Switch buffers */
				save_buffp = buffp;
				save_buff_unprocessed = buff_unprocessed;
				save_buffered_data_len = buffered_data_len;

				data_len = buff_unprocessed = buffered_data_len = write_len;
				buffp = repl_filter_buff;
				write_loc = write_off;
				repl_recv_data_processed -= pre_filter_write_len;
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
	unsigned char		*msg_ptr, seq_num_str[32], seq_num_str1[32], *seq_num_ptr;
	seq_num			request_from, recvd_jnl_seqno;
	int			sent_len, send_len, recvd_len, recv_len, skip_for_alignment, msg_type, msg_len, status;
	char			print_msg[1024];
	repl_heartbeat_msg_t	heartbeat;

	error_def(ERR_REPLCOMM);
	error_def(ERR_REPLWARN);
	error_def(ERR_REPLTRANS2BIG);
	error_def(ERR_TEXT);

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	QWASSIGN(lastlog_seqno, seq_num_minus_one);
	QWDECRBYDW(lastlog_seqno, (LOGTRNUM_INTERVAL - 1));
	trans_recvd_cnt = -LOGTRNUM_INTERVAL + 1;
	repl_recv_prev_log_time = time(NULL);
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
		gtmrecv_wait_for_jnl_seqno = FALSE;
		if (QWEQ(recvpool_ctl->start_jnl_seqno, seq_num_zero))
			QWASSIGN(recvpool_ctl->start_jnl_seqno, recvpool_ctl->jnl_seqno);

		if (!crash_restart)
		{
			repl_log(gtmrecv_log_fp, FALSE, TRUE, "Starting JNL_SEQNO is "INT8_FMT"\n",
					INT8_PRINT(recvpool_ctl->start_jnl_seqno));
		} else
		{
			repl_log(gtmrecv_log_fp, FALSE, TRUE, "Restarting from JNL_SEQNO "INT8_FMT"\n",
					INT8_PRINT(recvpool_ctl->jnl_seqno));
		}
		QWASSIGN(request_from, recvpool_ctl->jnl_seqno);

		/* Send (re)start JNL_SEQNO to Source Server */

		gtmrecv_msgp->type = REPL_START_JNL_SEQNO;
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags = START_FLAG_NONE;
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags |=
			(gtmrecv_options.stopsourcefilter ? START_FLAG_STOPSRCFILTER : 0);
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags |= (gtmrecv_options.updateresync ? START_FLAG_UPDATERESYNC : 0);
		((repl_start_msg_ptr_t)gtmrecv_msgp)->start_flags |= START_FLAG_HASINFO;
		((repl_start_msg_ptr_t)gtmrecv_msgp)->jnl_ver = jnl_ver;
		QWASSIGN(*(seq_num *)&((repl_start_msg_ptr_t)gtmrecv_msgp)->start_seqno[0], request_from);
		gtmrecv_msgp->len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, gtmrecv_msgp, gtmrecv_msgp->len, &gtmrecv_poll_immediate)
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
				sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, RTS_ERROR_LITERAL("Connection closed"));
				repl_log(gtmrecv_log_fp, TRUE, TRUE, print_msg);
				gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "ERR_REPLWARN", print_msg);
				return;
			}
			if (EREPL_SEND == repl_errno)
			 	rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending (re)start jnlseqno. Error in send"), status);
			if (EREPL_SELECT == repl_errno)
			 	rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error sending (re)start jnlseqno. Error in select"), status);
		}
	}

	gtmrecv_bad_trans_sent = FALSE;
	QWASSIGN(request_from, recvpool_ctl->jnl_seqno);
	assert(QWGE(request_from, seq_num_one));


	repl_log(gtmrecv_log_fp, FALSE, TRUE, "Waiting for WILL_START or ROLL_BACK_FIRST message\n");


	/* Receive journal data and put it in the Receive Pool */

	buff_start = (uchar_ptr_t)gtmrecv_msgp;
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
		while ((status = repl_recv(gtmrecv_sock_fd, (buffp + buff_unprocessed), &recvd_len, &gtmrecv_poll_interval))
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
				SHORT_SLEEP(GTMRECV_POLL_INTERVAL/1000);
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
				if (REPL_CONN_RESET(status) || ETIMEDOUT == status)
				{
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection reset\n");
					repl_connection_reset = TRUE;
					repl_close(&gtmrecv_sock_fd);
					return;
				} else
					rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						  RTS_ERROR_LITERAL("Error in receiving from source. Error in recv"), status);
			} else if (EREPL_SELECT == repl_errno)
			{
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error in receiving from source. Error in select"), status);
			}
		}

		if (repl_connection_reset)
			return;

		/* Something on the replication pipe - read it */

		REPL_DPRINT3("Pending data len : %d  Prev buff unprocessed : %d\n", data_len, buff_unprocessed);

		buff_unprocessed += recvd_len;
		repl_recv_data_recvd += recvd_len;

		if (gtmrecv_logstats)
			repl_log(gtmrecv_statslog_fp, FALSE, FALSE, "Recvd : %d  Total : %d\n", recvd_len, repl_recv_data_recvd);

		while (REPL_MSG_HDRLEN <= buff_unprocessed)
		{
			if (0 == data_len)
			{
				assert(0 == ((unsigned long)buffp & (sizeof(((repl_msg_ptr_t)buffp)->type) - 1)));
				msg_type = ((repl_msg_ptr_t)buffp)->type;
				msg_len = data_len = ((repl_msg_ptr_t)buffp)->len - REPL_MSG_HDRLEN;
				assert(0 == (msg_len & ((sizeof((repl_msg_ptr_t)buffp)->type) - 1)));
				buffp += REPL_MSG_HDRLEN;
				buff_unprocessed -= REPL_MSG_HDRLEN;

				if (data_len > recvpool_ctl->recvpool_size)
				{
					/* Too large a transaction to be
					 * accommodated in the Receive Pool */
					seq_num_ptr = i2ascl(seq_num_str, recvpool_ctl->jnl_seqno);
					rts_error(VARLSTCNT(7) ERR_REPLTRANS2BIG, 5, seq_num_ptr - &seq_num_str[0], seq_num_str,
						  data_len, RTS_ERROR_LITERAL("Receive"));
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
						REPL_DPRINT3("Heartbeat received with time %ld SEQNO "INT8_FMT"\n",
							     *(time_t *)&heartbeat.ack_time[0],
							     INT8_PRINT(*(seq_num *)&heartbeat.ack_seqno[0]));
						heartbeat.type = REPL_HEARTBEAT;
						heartbeat.len = MIN_REPL_MSGLEN;
						QWASSIGN(*(seq_num *)&heartbeat.ack_seqno[0], upd_proc_local->read_jnl_seqno);
						REPL_SEND_LOOP(gtmrecv_sock_fd, &heartbeat, heartbeat.len, &gtmrecv_poll_immediate)
						{
							gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
							if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
								return;
						}
						/* Error handling for the above send_loop is not required as it'll be caught
						 * in the next recv_loop of the receiver server */
						REPL_DPRINT3("HEARTBEAT sent with time %ld SEQNO "INT8_FMT"\n",
							     *(time_t *)&heartbeat.ack_time[0],
							     INT8_PRINT(*(seq_num *)&heartbeat.ack_seqno[0]));
					}
					break;

				case REPL_WILL_RESTART:
				case REPL_WILL_RESTART_WITH_INFO:
				case REPL_ROLLBACK_FIRST:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						QWASSIGN(recvd_jnl_seqno, *(seq_num *)(buffp - msg_len));
						if (REPL_WILL_RESTART_WITH_INFO == msg_type || REPL_WILL_RESTART == msg_type)
						{
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received WILL_START message. "
											     "Primary acked the restart point\n");
							if (REPL_WILL_RESTART_WITH_INFO == msg_type) /* V4.2+ */
								remote_jnl_ver = *(buffp - msg_len + sizeof(seq_num));
							else
								remote_jnl_ver = JNL_VER_EARLIEST_REPL;
							REPL_DPRINT3("Local jnl ver is octal %o, remote jnl ver is octal %o\n",
								     jnl_ver, remote_jnl_ver);
							assert(JNL_VER_EARLIEST_REPL <= jnl_ver &&
							       JNL_VER_EARLIEST_REPL <= remote_jnl_ver);
							assert(JNL_VER_THIS >= jnl_ver && JNL_VER_THIS >= remote_jnl_ver);
							assert((intlfltr_t)0 !=
								repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
										    [remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
							assert((intlfltr_t)0 !=
								repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
										    [jnl_ver - JNL_VER_EARLIEST_REPL]);
							if (jnl_ver > remote_jnl_ver &&
							    IF_NONE != repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
							                                   [jnl_ver - JNL_VER_EARLIEST_REPL])
							{
								gtmrecv_filter |= INTERNAL_FILTER;
								gtmrecv_alloc_filter_buff(gtmrecv_max_repl_msglen);
								/* reverse transformation should exist */
								assert(IF_NONE != repl_internal_filter[jnl_ver -
												       JNL_VER_EARLIEST_REPL]
									 		  	      [remote_jnl_ver -
												       JNL_VER_EARLIEST_REPL]);
							} else
							{
								gtmrecv_filter &= ~INTERNAL_FILTER;
								if (NO_FILTER == gtmrecv_filter && repl_filter_buff)
								{
									free(repl_filter_buff);
									repl_filter_buff = NULL;
									repl_filter_bufsiz = 0;
								}
							}

							/* Don't send any more stopsourcefilter, or updateresync messages */
							gtmrecv_options.stopsourcefilter = FALSE;
							gtmrecv_options.updateresync = FALSE;
							assert(QWEQ(recvd_jnl_seqno, request_from));
							break;
						}
						repl_log(gtmrecv_log_fp, TRUE, FALSE, "ROLLBACK_FIRST message received. Secondary "
							 "ahead of primary. Secondary at "INT8_FMT, INT8_PRINT(request_from));
						repl_log(gtmrecv_log_fp, FALSE, TRUE, ", primary at "INT8_FMT". "
							 "Do ROLLBACK FIRST\n", INT8_PRINT(recvd_jnl_seqno));
						gtmrecv_autoshutdown();
					}
					break;

				default:
					/* Discard the message */
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					break;
			}
			if (repl_connection_reset)
				return;
		}
		skip_for_alignment = (int)((unsigned long)buffp & (sizeof(((repl_msg_ptr_t)buffp)->type) - 1));
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

void gtmrecv_process(boolean_t crash_restart)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;

	jnl_setver();
	assert(GTMRECV_POLL_INTERVAL < MAX_GTMRECV_POLL_INTERVAL);
	gtmrecv_poll_interval.tv_sec = 0;
	gtmrecv_poll_interval.tv_usec = GTMRECV_POLL_INTERVAL;
	gtmrecv_poll_immediate.tv_sec = 0;
	gtmrecv_poll_immediate.tv_usec = 0;
	recvpool_high_watermark = (long)((float)RECVPOOL_HIGH_WATERMARK_PCTG/100 * recvpool_ctl->recvpool_size);
	repl_log(gtmrecv_log_fp, FALSE, FALSE, "RECVPOOL HIGH WATERMARK is %d, pctg of recvpool size is %d\n",
		 recvpool_high_watermark, RECVPOOL_HIGH_WATERMARK_PCTG);
	gtmrecv_msgp = NULL;
	while (TRUE)
	{
		assert(gtmrecv_sock_fd == -1);
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
		gtmrecv_alloc_msgbuff();
		while (!repl_connection_reset)
			do_main_loop(crash_restart);
	}
	GTMASSERT; /* shouldn't reach here */
	return;
}
