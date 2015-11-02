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

#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_time.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_stdio.h"	/* for FILE * in repl_comm.h */

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
#include "sgtm_putmsg.h"
#include "gt_timer.h"
#include "min_max.h"
#include "error.h"
#include "copy.h"
#include "repl_instance.h"
#include "ftok_sems.h"
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gtmmsg.h"
#include "is_proc_alive.h"
#include "jnl_typedef.h"

#define RECVBUFF_REPLMSGLEN_FACTOR 		8

#define GTMRECV_POLL_INTERVAL			(1000000 - 1)/* micro sec, almost 1 sec */
#define MAX_GTMRECV_POLL_INTERVAL		1000000 /* 1 sec in micro sec */

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
GBLDEF	struct timeval		gtmrecv_poll_interval, gtmrecv_poll_immediate;
GBLDEF	int			gtmrecv_sock_fd = -1;
GBLDEF	boolean_t		repl_connection_reset = FALSE;
GBLDEF	boolean_t		gtmrecv_wait_for_jnl_seqno = FALSE;
GBLDEF	boolean_t		gtmrecv_bad_trans_sent = FALSE;
GBLDEF	struct sockaddr_in	primary_addr;

GBLDEF	qw_num			repl_recv_data_recvd = 0;
GBLDEF	qw_num			repl_recv_data_processed = 0;
GBLDEF	qw_num			repl_recv_prefltr_data_procd = 0;
GBLDEF	qw_num			repl_recv_lastlog_data_recvd = 0;
GBLDEF	qw_num			repl_recv_lastlog_data_procd = 0;

GBLDEF	time_t			repl_recv_prev_log_time;
GBLDEF	time_t			repl_recv_this_log_time;
GBLDEF	volatile time_t		gtmrecv_now = 0;

GBLDEF	boolean_t	src_node_same_endianness = TRUE;
GBLDEF	boolean_t 	src_node_endianness_known = FALSE;

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
GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
GBLREF	unsigned char		*repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	boolean_t		null_subs_xform;
GBLREF	boolean_t 		primary_side_std_null_coll;
GBLREF	boolean_t 		secondary_side_std_null_coll;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;
GBLREF	qw_num			trans_recvd_cnt, last_log_tr_recvd_cnt;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	mur_gbls_t		murgbl;

LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];

static	unsigned char	*buffp, *buff_start, *msgbuff, *filterbuff;
static	int		buff_unprocessed;
static	int		buffered_data_len;
static	int		max_recv_bufsiz;
static	int		data_len;
static	boolean_t	xoff_sent;
static	repl_msg_t	xon_msg, xoff_msg;
static	int		xoff_msg_log_cnt = 0;
static	long		recvpool_high_watermark, recvpool_low_watermark;
static	uint4		write_loc, write_wrap;
static	uint4		write_len, write_off, pre_filter_write_len, pre_filter_write, pre_intlfilter_datalen;
static	double		time_elapsed;
static	int		recvpool_size;
static	int		heartbeat_period;
static	char		assumed_remote_proto_ver;

/* convert endianness of transaction */
static int repl_tr_endian_convert(uchar_ptr_t jnl_buff, uint4 jnl_len)
{
	unsigned char		*jb, *jstart, *ptr;
	enum	jnl_record_type	rectype;
	int			status, reclen;
	uint4			jlen;
	jrec_prefix 		*prefix;
	jnl_record	*rec;
	jnl_string	*keystr;
	mstr_len_t	*val_ptr;
	mval	val_mv;
	repl_triple_jnl_ptr_t	triplecontent;

	jb = jnl_buff;
	status = SS_NORMAL;
	jlen = jnl_len;
	assert(0 == ((uint4)jb % sizeof(uint4)));
   	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((uint4)jb % sizeof(uint4)));
		rec = (jnl_record *) jb;
		rectype = rec->prefix.jrec_type; 
		reclen = rec->prefix.forwptr = GTM_BYTESWAP_24(rec->prefix.forwptr);

		if (JRT_TRIPLE != rectype)
		{		
			rec->prefix.pini_addr = GTM_BYTESWAP_32(rec->prefix.pini_addr);
			rec->prefix.time = GTM_BYTESWAP_32(rec->prefix.time);
			rec->prefix.checksum = GTM_BYTESWAP_32(rec->prefix.checksum);
			rec->prefix.tn = GTM_BYTESWAP_64(rec->prefix.tn);
			((jrec_suffix *)((unsigned char *)rec + reclen - JREC_SUFFIX_SIZE))->backptr =
				GTM_BYTESWAP_24(((jrec_suffix *)((unsigned char *)rec + reclen - JREC_SUFFIX_SIZE))->backptr);
			assert(IS_REPLICATED(rectype));
			if (IS_REPLICATED(rectype))
				rec->jrec_null.jnl_seqno = GTM_BYTESWAP_64(rec->jrec_null.jnl_seqno);
		}
		
		jstart = jb;
   		if (0 != reclen)
		{
   			if (reclen <= jlen)
			{
				if (JRT_TRIPLE == rectype)
				{
					triplecontent = (repl_triple_jnl_ptr_t) rec;
					triplecontent->cycle = GTM_BYTESWAP_32(triplecontent->cycle);
					triplecontent->start_seqno = GTM_BYTESWAP_64(triplecontent->start_seqno);
				}
				
				if (IS_SET_KILL_ZKILL(rectype))
				{
					if (IS_ZTP(rectype))
					{
						keystr = (jnl_string *)&rec->jrec_fset.mumps_node;
						rec->jrec_fset.token = GTM_BYTESWAP_64(rec->jrec_fset.token);
					}
					else
					{
						keystr = (jnl_string *)&rec->jrec_set.mumps_node;
						keystr->length = GTM_BYTESWAP_32(keystr->length);
					} 
					if (IS_SET(rectype))
					{
						val_ptr = (mstr_len_t *)&keystr->text[keystr->length];
						*val_ptr = GTM_BYTESWAP_32(*val_ptr);
					}
				}
					
					if (JRT_TCOM == rectype)
					{
						*(trans_num *)&rec->jrec_tcom.jnl_tid = GTM_BYTESWAP_64(*(trans_num *)&rec->jrec_tcom.jnl_tid);						
					}
					
					if (JRT_ZTCOM == rectype)
					{
					rec->jrec_ztcom.token = GTM_BYTESWAP_64(rec->jrec_ztcom.token);
					rec->jrec_ztcom.participants = GTM_BYTESWAP_32(rec->jrec_ztcom.participants);
					}
				
				
				jb = jb + reclen;
				assert(jb == jstart + reclen);
				jlen -= reclen;
				continue;
			}
/*      Incomplete record */
			assert(FALSE);
			status = -1;
			break;
		}
/*      Bad record */
		assert(FALSE);
		status = -1;
		break;
	}
	if ((-1 != status) && (0 != jlen))
	{
/*      Incomplete record */
		assert(FALSE);
		status = -1;
	}
	assert(0 == jlen || -1 == status);
	return(status);
}

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

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

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
		if ( src_node_same_endianness )
		{
			xoff_msg.type = REPL_XOFF;
			memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, sizeof(seq_num));
			xoff_msg.len = MIN_REPL_MSGLEN;
		}
		else
		{
			xoff_msg.type = GTM_BYTESWAP_32(REPL_XOFF);
			*((seq_num*)&xoff_msg.msg[0]) = GTM_BYTESWAP_64(upd_proc_local->read_jnl_seqno);
			xoff_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);		
		}
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xoff_msg, MIN_REPL_MSGLEN, FALSE, &gtmrecv_poll_immediate)
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
				SNPRINTF(print_msg, sizeof(print_msg), "Error sending XOFF msg. Error in send : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
			if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, sizeof(print_msg), "Error sending XOFF msg. Error in select : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
		}
		if (gtmrecv_logstats)
			repl_log(gtmrecv_statslog_fp, TRUE, TRUE, "Space used = %ld, High water mark = %d Low water mark = %d, "
					"Updproc Read = %d, Recv Write = %d, Sent XOFF\n", space_used, recvpool_high_watermark,
					recvpool_low_watermark, read_pos, write_pos);
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_XOFF sent as receive pool has %ld bytes transaction data yet to be "
				"processed\n", space_used);
		xoff_sent = TRUE;
		xoff_msg_log_cnt = 1;
		assert(GTMRECV_WAIT_FOR_UPD_PROGRESS_US < MAX_GTMRECV_POLL_INTERVAL);
		gtmrecv_poll_interval.tv_sec = 0;
		gtmrecv_poll_interval.tv_usec = GTMRECV_WAIT_FOR_UPD_PROGRESS_US;
	} else if (space_used < recvpool_low_watermark && xoff_sent)
	{
		if ( src_node_same_endianness )
		{
			xon_msg.type = REPL_XON;
			memcpy((uchar_ptr_t)&xon_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, sizeof(seq_num));
			xon_msg.len = MIN_REPL_MSGLEN;
		}
		else
		{
			xon_msg.type = GTM_BYTESWAP_32(REPL_XON);
			*((seq_num*)&xon_msg.msg[0]) = GTM_BYTESWAP_64(upd_proc_local->read_jnl_seqno);
			xon_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
		}
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xon_msg, MIN_REPL_MSGLEN, FALSE, &gtmrecv_poll_immediate)
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
				SNPRINTF(print_msg, sizeof(print_msg), "Error sending XON msg. Error in send : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
			if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, sizeof(print_msg), "Error sending XON msg. Error in select : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
		}
		if (gtmrecv_logstats)
			repl_log(gtmrecv_statslog_fp, TRUE, TRUE, "Space used now = %ld, High water mark = %d, "
				 "Low water mark = %d, Updproc Read = %d, Recv Write = %d, Sent XON\n", space_used,
				 recvpool_high_watermark, recvpool_low_watermark, read_pos, write_pos);
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL_XON sent as receive pool has %ld bytes free space to buffer transaction "
				"data\n", recvpool_size - space_used);
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
	char			print_msg[1024];
	int			send_buffsize, recv_buffsize, tcp_r_bufsize;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	/* Wait for a connection from a Source Server.
	 * The Receiver Server is an iterative server.
	 */
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;

	gtmrecv_comm_init((in_port_t)gtmrecv_local->listen_port);
	primary_addr_len = sizeof(primary_addr);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for a connection...\n");

	/* Null initialize fields that need to be initialized only after connecting to the primary.
	 * It is ok not to hold a lock on the journal pool while updating jnlpool_ctl fields since this will be the only
	 * process updating those fields.
	 */
	gtmrecv_local->remote_proto_ver = REPL_PROTO_VER_UNINITIALIZED;
	jnlpool_ctl->primary_instname[0] = '\0';
	jnlpool_ctl->primary_is_dualsite = FALSE;

	FD_ZERO(&input_fds);
	FD_SET(gtmrecv_listen_sock_fd, &input_fds);
	/*
	 * Note - the following while loop checks for EINTR on the select. The
	 * SELECT macro is not used because the FD_SET is redone before the new
	 * call to select (after the continue).
	 */
	save_gtmrecv_poll_interval = gtmrecv_poll_interval;
	while (0 >= (status = select(gtmrecv_listen_sock_fd + 1, &input_fds, NULL, NULL, &save_gtmrecv_poll_interval)))
	{
		save_gtmrecv_poll_interval = gtmrecv_poll_interval;
		FD_SET(gtmrecv_listen_sock_fd, &input_fds);
		if (0 == status)
			gtmrecv_poll_actions(0, 0, NULL);
		else if (EINTR == errno || EAGAIN == errno)
			continue;
		else
		{
			status = ERRNO;
			SNPRINTF(print_msg, sizeof(print_msg), "Error in select on listen socket : %s", STRERROR(status));
			rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
		}
	}
	ACCEPT_SOCKET(gtmrecv_listen_sock_fd, (struct sockaddr *)&primary_addr, (sssize_t *)&primary_addr_len, gtmrecv_sock_fd);
	if (-1 == gtmrecv_sock_fd)
	{
		status = ERRNO;
		SNPRINTF(print_msg, sizeof(print_msg), "Error accepting connection from Source Server : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
	}
	/* Connection established */
	repl_close(&gtmrecv_listen_sock_fd); /* Close the listener socket */
	repl_connection_reset = FALSE;
	if (-1 == setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, sizeof(disable_linger)))
	{
		status = ERRNO;
		SNPRINTF(print_msg, sizeof(print_msg), "Error with receiver server socket disable linger : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
	}

#ifdef REPL_DISABLE_KEEPALIVE
	if (-1 == setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&disable_keepalive,
				sizeof(disable_keepalive)))
	{ /* Till SIGPIPE is handled properly */
		status = ERRNO;
		SNPRINTF(print_msg, sizeof(print_msg), "Error with receiver server socket disable keepalive : %s",
				STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
	}
#endif
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &send_buffsize)))
	{
		SNPRINTF(print_msg, sizeof(print_msg), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
	}
	if (send_buffsize < GTMRECV_TCP_SEND_BUFSIZE)
	{
		if (0 != (status = set_send_sock_buff_size(gtmrecv_sock_fd, GTMRECV_TCP_SEND_BUFSIZE)))
		{
			if (send_buffsize < GTMRECV_MIN_TCP_SEND_BUFSIZE)
			{
				SNPRINTF(print_msg, sizeof(print_msg), "Could not set TCP send buffer size to %d : %s",
						GTMRECV_MIN_TCP_SEND_BUFSIZE, STRERROR(status));
				rts_error(VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
		}
	}
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize))) /* may have changed */
	{
		SNPRINTF(print_msg, sizeof(print_msg), "Error getting socket send buffsize : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
	}
	if (0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &recv_buffsize)))
	{
		SNPRINTF(print_msg, sizeof(print_msg), "Error getting socket recv buffsize : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
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
			SNPRINTF(print_msg, sizeof(print_msg), "Could not set TCP receive buffer size in range [%d, %d], last "
					"known error : %s", GTMRECV_MIN_TCP_RECV_BUFSIZE, GTMRECV_TCP_RECV_BUFSIZE,
					STRERROR(status));
			rts_error(VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
		}
	}
	if (0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &repl_max_recv_buffsize))) /* may have changed */
	{
		SNPRINTF(print_msg, sizeof(print_msg), "Error getting socket recv buffsize : %s", STRERROR(status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
	}
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection established, using TCP send buffer size %d receive buffer size %d\n",
			repl_max_send_buffsize, repl_max_recv_buffsize);
	repl_log_conn_info(gtmrecv_sock_fd, gtmrecv_log_fp);
	/* re-determine endianness of other side */
	src_node_endianness_known = FALSE;
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
	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	gtmrecv_max_repl_msglen = MAX_REPL_MSGLEN + sizeof(gtmrecv_msgp->type); /* add sizeof(...) for alignment */
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

static void process_tr_buff(int msg_type)
{
	recvpool_ctl_ptr_t		recvpool_ctl;
	upd_proc_local_ptr_t		upd_proc_local;
	gtmrecv_local_ptr_t		gtmrecv_local;
	seq_num				log_seqno;
	uint4				future_write, in_size, out_size, out_bufsiz, tot_out_size, save_buff_unprocessed,
					save_buffered_data_len;
	boolean_t			filter_pass = FALSE, is_new_triple;
	uchar_ptr_t			save_buffp, save_filter_buff, in_buff, out_buff;
	int				status;
	qw_num				msg_total;
	static int			triplelen = 0;
	static repl_triple_jnl_t	triplecontent;

	error_def(ERR_REPLTRANS2BIG);
	error_def(ERR_TEXT);
	error_def(ERR_JNLRECFMT);
	error_def(ERR_JNLSETDATA2LONG);
	error_def(ERR_JNLNEWREC);
	error_def(ERR_REPLGBL2LONG);

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	is_new_triple = (REPL_NEW_TRIPLE == msg_type);
	do
	{
		if (write_loc + data_len > recvpool_size)
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
#ifdef REPL_DEBUG
		if (recvpool_ctl->wrapped && write_loc <= upd_proc_local->read && upd_proc_local->read <= future_write)
			REPL_DPRINT1("Update Process too slow. Waiting for it to free up space\n");
#endif
		while (recvpool_ctl->wrapped && write_loc <= upd_proc_local->read && upd_proc_local->read <= future_write)
		{	/* Write will cause overflow. Wait till there is more space available */
			SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}
		memcpy(recvpool.recvdata_base + write_loc, buffp, buffered_data_len);
		if (is_new_triple)
		{
			assert((triplelen + buffered_data_len) <= sizeof(triplecontent));
			if ((triplelen + buffered_data_len) <= sizeof(triplecontent))
			{
				memcpy(((sm_uc_ptr_t)&triplecontent) + triplelen, buffp, buffered_data_len);
				triplelen += buffered_data_len;
			}
		}
		write_loc = future_write;
		if (write_loc > write_wrap)
			write_wrap = write_loc;
		repl_recv_data_processed += (qw_num)buffered_data_len;
		buffp += buffered_data_len;
		buff_unprocessed -= buffered_data_len;
		data_len -= buffered_data_len;
		if (0 != data_len)
			break;
		write_len = ((recvpool_ctl->write != write_wrap) ?  (write_loc - recvpool_ctl->write) : write_loc);
		write_off = ((recvpool_ctl->write != write_wrap) ? recvpool_ctl->write : 0);
		
		if (!src_node_same_endianness)
			if (SS_NORMAL != (status = repl_tr_endian_convert(recvpool.recvdata_base + write_off,write_len)))
				repl_log(gtmrecv_log_fp, FALSE, TRUE, 
					"REPL ERROR - Journal records did not endian convert properly\n");
		
		if (filter_pass || (NO_FILTER == gtmrecv_filter) || is_new_triple)
		{
			if (recvpool_ctl->jnl_seqno - lastlog_seqno >= log_interval)
			{
				log_seqno = recvpool_ctl->jnl_seqno;
				trans_recvd_cnt += (log_seqno - lastlog_seqno);
				msg_total = repl_recv_data_recvd - buff_unprocessed;
					/* Don't include data not yet processed, we'll include that count in a later log */
				if (NO_FILTER == gtmrecv_filter)
				{
					repl_log(gtmrecv_log_fp, FALSE, TRUE, "REPL INFO - Tr num : %llu"
						"  Tr Total : %llu  Msg Total : %llu\n", log_seqno,
						repl_recv_data_processed, msg_total);
				} else
				{
					repl_log(gtmrecv_log_fp, FALSE, TRUE, "REPL INFO - Tr num : %llu  Pre filter "
						"total : %llu  Post filter total : %llu  Msg Total : %llu\n", log_seqno,
						repl_recv_prefltr_data_procd, repl_recv_data_processed, msg_total);
				}
				/* Approximate time with an error not more than GTMRECV_HEARTBEAT_PERIOD. We use this
				 * instead of calling time(), and expensive system call, especially on VMS. The
				 * consequence of this choice is that we may defer logging when we may have logged. We
				 * can live with that. Currently, the logging interval is not changeable by users.
				 * When/if we provide means of choosing log interval, this code may have to be re-examined.
				 * 	- Vinaya 2003/09/08.
				 */
				assert(0 != gtmrecv_now);
				repl_recv_this_log_time = gtmrecv_now;
				assert(repl_recv_this_log_time >= repl_recv_prev_log_time);
				time_elapsed = difftime(repl_recv_this_log_time, repl_recv_prev_log_time);
				if ((double)GTMRECV_LOGSTATS_INTERVAL <= time_elapsed)
				{
					repl_log(gtmrecv_log_fp, TRUE, FALSE, "REPL INFO since last log : Time elapsed : "
						"%00.f  Tr recvd : %llu  Tr bytes : %llu  Msg bytes : %llu\n",
						time_elapsed, trans_recvd_cnt - last_log_tr_recvd_cnt,
						repl_recv_data_processed - repl_recv_lastlog_data_procd,
						msg_total - repl_recv_lastlog_data_recvd);
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "REPL INFO since last log : Time elapsed : "
						"%00.f  Tr recvd/s : %f  Tr bytes/s : %f  Msg bytes/s : %f\n", time_elapsed,
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
			if (gtmrecv_logstats)
			{
				if (!filter_pass)
				{
					repl_log(gtmrecv_statslog_fp, FALSE, FALSE, "Tr : %llu  Size : %d  Write : %d  "
						 "Total : %llu\n", recvpool_ctl->jnl_seqno, write_len,
						 write_off, repl_recv_data_processed);
				} else
				{
					assert(!is_new_triple);
					repl_log(gtmrecv_statslog_fp, FALSE, FALSE, "Tr : %llu  Pre filter Size : %d  "
						"Post filter Size  : %d  Pre filter Write : %d  Post filter Write : %d  "
						"Pre filter Total : %llu  Post filter Total : %llu\n",
						recvpool_ctl->jnl_seqno, pre_filter_write_len, write_len,
						pre_filter_write, write_off, repl_recv_prefltr_data_procd,
						repl_recv_data_processed);
				}
			}
			recvpool_ctl->write_wrap = write_wrap;
			if (!is_new_triple)
			{
				if (recvpool_ctl->jnl_seqno == recvpool_ctl->last_rcvd_triple.start_seqno)
				{	/* Move over stuff from "last_rcvd_triple" to "last_valid_triple" */
					memcpy(&recvpool_ctl->last_valid_triple,
						&recvpool_ctl->last_rcvd_triple, sizeof(repl_triple));
				}
				QWINCRBYDW(recvpool_ctl->jnl_seqno, 1);
				assert(recvpool_ctl->last_valid_triple.start_seqno < recvpool_ctl->jnl_seqno);
			} else
			{
				if ( !src_node_same_endianness )
				{ 
					triplecontent.cycle = GTM_BYTESWAP_32(triplecontent.cycle);
					triplecontent.forwptr = GTM_BYTESWAP_24(triplecontent.forwptr); 
					triplecontent.start_seqno = GTM_BYTESWAP_64(triplecontent.start_seqno);
				}

				assert(sizeof(triplecontent) == triplelen);
				assert(JRT_TRIPLE == triplecontent.jrec_type);
				assert(triplecontent.forwptr == sizeof(triplecontent));
				assert(triplecontent.start_seqno == recvpool_ctl->jnl_seqno);
				assert(triplecontent.start_seqno >= recvpool.upd_proc_local->read_jnl_seqno);
				assert(triplecontent.start_seqno > recvpool_ctl->last_valid_triple.start_seqno);
				assert(triplecontent.start_seqno >= recvpool_ctl->last_rcvd_triple.start_seqno);
				/* Copy relevant fields from received triple message to "last_rcvd_triple" structure */
				memcpy(recvpool_ctl->last_rcvd_triple.root_primary_instname, triplecontent.instname,
					MAX_INSTNAME_LEN - 1);
				recvpool_ctl->last_rcvd_triple.start_seqno = triplecontent.start_seqno;
				recvpool_ctl->last_rcvd_triple.root_primary_cycle = triplecontent.cycle;
				triplelen = 0;
				repl_log(gtmrecv_log_fp, TRUE, TRUE, "New Triple Content : Start Seqno = "
					"%llu [0x%llx] : Root Primary = [%s] : Cycle = [%d] : Received from "
					"instance = [%s]\n", triplecontent.start_seqno, triplecontent.start_seqno,
					triplecontent.instname, triplecontent.cycle, jnlpool_ctl->primary_instname);
			}
			recvpool_ctl->write = write_loc;
			if (filter_pass)
			{	/* Switch buffers back */
				buffp = save_buffp;
				buff_unprocessed = save_buff_unprocessed;
				buffered_data_len = save_buffered_data_len;
				filter_pass = FALSE;
			}
			break;
		}
		/* Need to pass through filter */
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
				repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
						    [jnl_ver - JNL_VER_EARLIEST_REPL](
					in_buff, &in_size, out_buff, &out_size, out_bufsiz)) &&
			       EREPL_INTLFILTER_NOSPC == repl_errno)
			{
				save_filter_buff = repl_filter_buff;
				gtmrecv_alloc_filter_buff(repl_filter_bufsiz + (repl_filter_bufsiz >> 1));
				in_buff += in_size;
				in_size = (uint4)(pre_filter_write_len - (in_buff - recvpool.recvdata_base - write_off));
				out_bufsiz = (uint4)(repl_filter_bufsiz - (out_buff - save_filter_buff) - out_size);
				out_buff = repl_filter_buff + (out_buff - save_filter_buff) + out_size;
				tot_out_size += out_size;
			}
			if (SS_NORMAL == status)
				write_len = tot_out_size + out_size;
			else
			{
				assert(FALSE);
				if (EREPL_INTLFILTER_BADREC == repl_errno)
					rts_error(VARLSTCNT(1) ERR_JNLRECFMT);
				else if (EREPL_INTLFILTER_DATA2LONG == repl_errno)
					rts_error(VARLSTCNT(4) ERR_JNLSETDATA2LONG, 2, jnl_source_datalen,
						  jnl_dest_maxdatalen);
				else if (EREPL_INTLFILTER_NEWREC == repl_errno)
					rts_error(VARLSTCNT(4) ERR_JNLNEWREC, 2, (unsigned int)jnl_source_rectype,
						  (unsigned int)jnl_dest_maxrectype);
				else if (EREPL_INTLFILTER_REPLGBL2LONG == repl_errno)
						rts_error(VARLSTCNT(1) ERR_REPLGBL2LONG);
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
		{
			rts_error(VARLSTCNT(10) ERR_REPLTRANS2BIG, 4, &recvpool_ctl->jnl_seqno,
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
		repl_recv_data_processed -= (qw_num)pre_filter_write_len;
		filter_pass = TRUE;
	} while (TRUE);
	return;
}

/* This function can be used to only send fixed-size message types across the replication pipe.
 * This in turn uses REPL_SEND* macros but also does error checks and sets the global variables
 *	"repl_connection_reset" or "gtmrecv_wait_for_jnl_seqno" accordingly.
 *
 *	msg            = Pointer to the message buffer to send
 *	msgtypestr     = Message name as a string to display meaningful error messages
 *	optional_seqno = Optional seqno that needs to be printed along with the message name
 */
void	gtmrecv_repl_send(repl_msg_ptr_t msgp, char *msgtypestr, seq_num optional_seqno)
{
	unsigned char		*msg_ptr;				/* needed for REPL_SEND_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			status;					/* needed for REPL_SEND_LOOP */
	FILE			*log_fp;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	assert(!mur_options.rollback || (NULL == recvpool.gtmrecv_local));
	assert(mur_options.rollback || (NULL != recvpool.gtmrecv_local));
	assert((REPL_MULTISITE_MSG_START > msgp->type)
		|| !mur_options.rollback && (REPL_PROTO_VER_MULTISITE <= recvpool.gtmrecv_local->remote_proto_ver)
		|| mur_options.rollback && (REPL_PROTO_VER_MULTISITE <= murgbl.remote_proto_ver));
	log_fp = (NULL == gtmrecv_log_fp) ? stdout : gtmrecv_log_fp;
	if (MAX_SEQNO != optional_seqno)
	{
		repl_log(log_fp, TRUE, TRUE, "Sending %s message with seqno %llu [0x%llx]\n", msgtypestr,
			optional_seqno, optional_seqno);
	} else
		repl_log(log_fp, TRUE, TRUE, "Sending %s message\n", msgtypestr);
	if ( src_node_same_endianness )
	{
		REPL_SEND_LOOP(gtmrecv_sock_fd, msgp, msgp->len, FALSE, &gtmrecv_poll_immediate)
		{
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}
	}

	if ( !src_node_same_endianness )
	{
		REPL_SEND_LOOP(gtmrecv_sock_fd, msgp, GTM_BYTESWAP_32(msgp->len), FALSE, &gtmrecv_poll_immediate)
		{
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
		}
	}
	if (SS_NORMAL != status)
	{
		if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
		{
			repl_log(log_fp, TRUE, TRUE, "Connection reset while sending %s message\n", msgtypestr);
			repl_connection_reset = TRUE;
			repl_close(&gtmrecv_sock_fd);
			return;
		} else if (EREPL_SEND == repl_errno)
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error sending %s message. Error in send"), msgtypestr, status);
		else if (EREPL_SELECT == repl_errno)
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error sending %s message. Error in select"), msgtypestr, status);
	}
	assert(SS_NORMAL == status);
}

/* This function is invoked on receipt of a REPL_NEED_TRIPLE_INFO message. This in turn sends a sequence of
 * REPL_TRIPLE_INFO1 and REPL_TRIPLE_INFO2 messages containing triple information.
 */
void gtmrecv_send_triple_info(repl_triple *triple, int4 triple_num)
{
	repl_tripinfo1_msg_t	tripinfo1_msg;
	repl_tripinfo2_msg_t	tripinfo2_msg;
	FILE			*log_fp;

	/*************** Send REPL_TRIPLE_INFO1 message ***************/
	memset(&tripinfo1_msg, 0, sizeof(tripinfo1_msg));
	if ( src_node_same_endianness )
	{
		tripinfo1_msg.type = REPL_TRIPLE_INFO1;
		tripinfo1_msg.len = MIN_REPL_MSGLEN;
		tripinfo1_msg.start_seqno = triple->start_seqno;
	}
	else
	{
		tripinfo1_msg.type = GTM_BYTESWAP_32(REPL_TRIPLE_INFO1);
		tripinfo1_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
		tripinfo1_msg.start_seqno = GTM_BYTESWAP_64(triple->start_seqno);
	}
	memcpy(tripinfo1_msg.instname, triple->root_primary_instname, MAX_INSTNAME_LEN - 1);
	gtmrecv_repl_send((repl_msg_ptr_t)&tripinfo1_msg, "REPL_TRIPLE_INFO1", tripinfo1_msg.start_seqno);
	if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
		return;
	/*************** Send REPL_TRIPLE_INFO2 message ***************/
	memset(&tripinfo2_msg, 0, sizeof(tripinfo2_msg));
	if ( src_node_same_endianness )
	{
		tripinfo2_msg.type = REPL_TRIPLE_INFO2;
		tripinfo2_msg.len = MIN_REPL_MSGLEN;
		tripinfo2_msg.start_seqno = triple->start_seqno;
		tripinfo2_msg.cycle = triple->root_primary_cycle;
		tripinfo2_msg.triple_num = triple_num;
	}
	else
	{
		tripinfo2_msg.type = GTM_BYTESWAP_32(REPL_TRIPLE_INFO2);
		tripinfo2_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
		tripinfo2_msg.start_seqno = GTM_BYTESWAP_64(triple->start_seqno);
		tripinfo2_msg.cycle = GTM_BYTESWAP_32(triple->root_primary_cycle);
		tripinfo2_msg.triple_num = GTM_BYTESWAP_32(triple_num);
	}	
	/* Since this is not a root primary instance, updates should be disabled. Assert that */
	assert((NULL == jnlpool_ctl) || jnlpool_ctl->upd_disabled);
	assert((NULL == jnlpool_ctl) || (jnlpool_ctl->jnl_seqno >= jnlpool_ctl->start_jnl_seqno));
	assert((NULL == jnlpool_ctl) || (recvpool.recvpool_ctl->jnl_seqno >= jnlpool_ctl->jnl_seqno));
		/* Update process could have done zero or more updates hence the ">=" in the assert above */
	gtmrecv_repl_send((repl_msg_ptr_t)&tripinfo2_msg, "REPL_TRIPLE_INFO2", tripinfo1_msg.start_seqno);
	if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
		return;
	log_fp = (NULL == gtmrecv_log_fp) ? stdout : gtmrecv_log_fp;
	repl_log(log_fp, TRUE, FALSE, "Triple Sent with Start Seqno = %llu [0x%llx] : Root Primary = [%s] : Cycle = [%d]\n",
		triple->start_seqno, triple->start_seqno, triple->root_primary_instname, triple->root_primary_cycle);
}

/* This routine goes through all source server slots and checks if there is one slot with an active source server.
 * This returns TRUE in that case. In all other cases it returns FALSE. Note that this routine does not grab any locks.
 * It rather expects the caller to hold any locks that matter.
 */
static boolean_t	is_active_source_server_running(void)
{
	int4			index;
	uint4			gtmsource_pid;
	boolean_t		srv_alive;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;

	gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
	for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
	{
		if ('\0' == gtmsourcelocal_ptr->secondary_instname[0])
			continue;
		gtmsource_pid = gtmsourcelocal_ptr->gtmsource_pid;
		srv_alive = (0 == gtmsource_pid) ? FALSE : is_proc_alive(gtmsource_pid, 0);
		if (!srv_alive)
			continue;
		if (GTMSOURCE_MODE_ACTIVE == gtmsourcelocal_ptr->mode)
			return TRUE;
	}
	return FALSE;
}

static void do_main_loop(boolean_t crash_restart)
{
	/* The work-horse of the Receiver Server */
	recvpool_ctl_ptr_t		recvpool_ctl;
	upd_proc_local_ptr_t		upd_proc_local;
	gtmrecv_local_ptr_t		gtmrecv_local;
	seq_num				request_from, recvd_jnl_seqno;
	seq_num				input_triple_seqno, last_valid_triple_seqno;
	seq_num				first_unprocessed_seqno, last_unprocessed_triple_seqno;
	int				skip_for_alignment, msg_type, msg_len;
	unsigned char			*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int				tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int				torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int				status;					/* needed for REPL_{SEND,RECV}_LOOP */
	char				print_msg[1024];
	repl_heartbeat_msg_t		heartbeat;
	repl_start_msg_ptr_t		msgp;
	repl_start_reply_msg_t		*start_msg;
	repl_needinst_msg_ptr_t		need_instinfo_msg;
	repl_needtriple_msg_ptr_t	need_tripleinfo_msg;
	repl_instinfo_msg_t		instinfo_msg;
	uint4				recvd_start_flags;
	repl_triple			triple;
	int4				triple_num;

	error_def(ERR_PRIMARYNOTROOT);
	error_def(ERR_REPLCOMM);
	error_def(ERR_REPLINSTNOHIST);
	error_def(ERR_REPLTRANS2BIG);
	error_def(ERR_REPLUPGRADEPRI);
	error_def(ERR_REPLWARN);
	error_def(ERR_SECONDAHEAD);
	error_def(ERR_TEXT);
	error_def(ERR_UNIMPLOP);

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	gtmrecv_wait_for_jnl_seqno = FALSE;

	/* If BAD_TRANS was written by the update process, it would have updated recvpool_ctl->jnl_seqno accordingly.
	 * Only otherwise, do we need to wait for it to write "recvpool_ctl->start_jnl_seqno" and "recvpool_ctl->jnl_seqno".
	 */
	if (!gtmrecv_bad_trans_sent)
	{
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for Update Process to write jnl_seqno\n");
		while (QWEQ(recvpool_ctl->jnl_seqno, seq_num_zero))
		{
			SHORT_SLEEP(GTMRECV_WAIT_FOR_STARTJNLSEQNO);
			gtmrecv_poll_actions(0, 0, NULL);
			if (repl_connection_reset)
				return;
		}
		/* The call to "gtmrecv_poll_actions" above might have set the variable "gtmrecv_wait_for_jnl_seqno" to TRUE.
		 * In that case, we need to reset it to FALSE here as we are now going to wait for the jnl_seqno below.
		 * Not doing so will cause us to wait for jnl_seqno TWICE (once now and once when we later enter this function).
		 */
		gtmrecv_wait_for_jnl_seqno = FALSE;
		secondary_side_std_null_coll = recvpool_ctl->std_null_coll;
		if (QWEQ(recvpool_ctl->start_jnl_seqno, seq_num_zero))
			QWASSIGN(recvpool_ctl->start_jnl_seqno, recvpool_ctl->jnl_seqno);
		/* If we assume remote primary is multisite capable, we need to send the journal seqno of this instance
		 * for comparison. If on the other hand, it is assumed to be only dualsite capable, we need to send the
		 * dualsite_resync_seqno of this instance which is maintained in "recvpool_ctl->max_dualsite_resync_seqno".
		 * But in either case, if the receiver has received more seqnos than have been processed by the update process,
		 * we should be sending the last received seqno across to avoid receiving duplicate and out-of-order seqnos.
		 * This is maintained in "recvpool_ctl->jnl_seqno" and is guaranteed to be greater than or equal to the journal
		 * seqno of this instance or the dualsite_resync_seqno of this instance.
		 */
		assert((REPL_PROTO_VER_MULTISITE == assumed_remote_proto_ver)
			|| (REPL_PROTO_VER_DUALSITE == assumed_remote_proto_ver));
		QWASSIGN(request_from, recvpool_ctl->jnl_seqno);
		/* If this is the first time the update process initialized "recvpool_ctl->jnl_seqno", it should be
		 * equal to "jnlpool_ctl->jnl_seqno". But if the receiver had already connected and received a bunch
		 * of seqnos and if the update process did not process all of them and if the receiver disconnects
		 * and re-establishes the connection, the value of "recvpool_ctl->jnl_seqno" could be greater than
		 * "jnlpool_ctl->jnl_seqno" if there is non-zero backlog on the secondary. Assert accordingly.
		 */
		assert(recvpool_ctl->jnl_seqno >= jnlpool_ctl->jnl_seqno);
		assert(recvpool_ctl->jnl_seqno >= recvpool_ctl->max_dualsite_resync_seqno);
		assert(request_from);
		repl_log(gtmrecv_log_fp, TRUE, FALSE, "Requesting transactions from JNL_SEQNO %llu [0x%llx]\n",
			request_from, request_from);
		/* Send (re)start JNL_SEQNO to Source Server */
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Sending REPL_START_JNL_SEQNO message with seqno %llu [0x%llx]\n",
			request_from, request_from);
		msgp = (repl_start_msg_ptr_t)gtmrecv_msgp;
		memset(msgp, 0, sizeof(*msgp));
		msgp->type = REPL_START_JNL_SEQNO;
		msgp->start_flags = START_FLAG_NONE;
		msgp->start_flags |= (gtmrecv_options.stopsourcefilter ? START_FLAG_STOPSRCFILTER : 0);
		msgp->start_flags |= (gtmrecv_options.updateresync ? START_FLAG_UPDATERESYNC : 0);
		msgp->start_flags |= START_FLAG_HASINFO;
		if (secondary_side_std_null_coll)
			msgp->start_flags |= START_FLAG_COLL_M;
		msgp->start_flags |= START_FLAG_VERSION_INFO;
		msgp->jnl_ver = jnl_ver;
		msgp->proto_ver = REPL_PROTO_VER_THIS;
		msgp->node_endianness = NODE_ENDIANNESS;
		QWASSIGN(*(seq_num *)&msgp->start_seqno[0], request_from);
		msgp->len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, msgp, msgp->len, FALSE, &gtmrecv_poll_immediate)
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
			{
				SNPRINTF(print_msg, sizeof(print_msg), "Error sending (re)start jnlseqno. Error in send : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
			if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, sizeof(print_msg), "Error sending (re)start jnlseqno. Error in select : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
		}
	}
	gtmrecv_bad_trans_sent = FALSE;
	request_from = recvpool_ctl->jnl_seqno;
	assert(request_from >= seq_num_one);
	gtmrecv_reinit_logseqno();

	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for REPL_WILL_RESTART_WITH_INFO or REPL_ROLLBACK_FIRST message\n");
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
		while ((status = repl_recv(gtmrecv_sock_fd, (buffp + buff_unprocessed), &recvd_len, FALSE, &gtmrecv_poll_interval))
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
				SHORT_SLEEP(GTMRECV_POLL_INTERVAL >> 10); /* approximate in ms */
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
				{
					SNPRINTF(print_msg, sizeof(print_msg), "Error in receiving from source. "
							"Error in recv : %s", STRERROR(status));
					rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
				}
			} else if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(print_msg, sizeof(print_msg), "Error in receiving from source. Error in select : %s",
						STRERROR(status));
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(print_msg));
			}
		}
		if (repl_connection_reset)
			return;
		/* Something on the replication pipe - read it */
		REPL_DPRINT3("Pending data len : %d  Prev buff unprocessed : %d\n", data_len, buff_unprocessed);
		buff_unprocessed += recvd_len;
		repl_recv_data_recvd += (qw_num)recvd_len;
		if (gtmrecv_logstats)
			repl_log(gtmrecv_statslog_fp, FALSE, FALSE, "Recvd : %d  Total : %d\n", recvd_len, repl_recv_data_recvd);
		while (REPL_MSG_HDRLEN <= buff_unprocessed)
		{
			if (0 == data_len)
			{
				assert(0 == ((unsigned long)buffp & (sizeof(((repl_msg_ptr_t)buffp)->type) - 1)));

				if (!src_node_endianness_known)
				{
					if ( ((repl_msg_ptr_t)buffp)->type > 256 && GTM_BYTESWAP_32(((repl_msg_ptr_t)buffp)->type) < 256 )
					{
						src_node_endianness_known = FALSE;
						src_node_same_endianness = FALSE;
					}
					else
					{
						src_node_endianness_known = FALSE;
						src_node_same_endianness = TRUE;
					}
				}
				if ( !src_node_same_endianness )
				{
					((repl_msg_ptr_t)buffp)->type = GTM_BYTESWAP_32(((repl_msg_ptr_t)buffp)->type);
					((repl_msg_ptr_t)buffp)->len = GTM_BYTESWAP_32(((repl_msg_ptr_t)buffp)->len);				
				}
				msg_type = ((repl_msg_ptr_t)buffp)->type;
				msg_len = data_len = ((repl_msg_ptr_t)buffp)->len - REPL_MSG_HDRLEN;
				assert(0 == (msg_len & ((sizeof((repl_msg_ptr_t)buffp)->type) - 1)));
				buffp += REPL_MSG_HDRLEN;
				buff_unprocessed -= REPL_MSG_HDRLEN;
				if (data_len > recvpool_size)
				{	/* Too large a transaction to be accommodated in the Receive Pool */
					rts_error(VARLSTCNT(6) ERR_REPLTRANS2BIG, 4, &recvpool_ctl->jnl_seqno,
						  data_len, RTS_ERROR_LITERAL("Receive"));
				}
			}
			buffered_data_len = ((data_len <= buff_unprocessed) ? data_len : buff_unprocessed);
			switch(msg_type)
			{
				case REPL_TR_JNL_RECS:
				case REPL_NEW_TRIPLE:
					process_tr_buff(msg_type);
					if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
						return;
					break;

				case REPL_LOSTTNCOMPLETE:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{
						assert(REPL_PROTO_VER_MULTISITE <= recvpool.gtmrecv_local->remote_proto_ver);
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_LOSTTNCOMPLETE message\n");
						repl_inst_reset_zqgblmod_seqno_and_tn();
					}
					break;

				case REPL_HEARTBEAT:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{	/* Heartbeat msg contents start from buffp - msg_len */
						memcpy(heartbeat.ack_seqno, buffp - msg_len, msg_len);
						if ( !src_node_same_endianness )
						{													
							 *(gtm_time4_t *)&heartbeat.ack_time[0] = GTM_BYTESWAP_32(*(gtm_time4_t *)&heartbeat.ack_time[0]);
							 *(seq_num *)&heartbeat.ack_seqno[0] = GTM_BYTESWAP_64(*(seq_num *)&heartbeat.ack_seqno[0]);
						}
						REPL_DPRINT4("HEARTBEAT received with time %ld SEQNO %llu at %ld\n",
							     *(gtm_time4_t *)&heartbeat.ack_time[0],
							     (*(seq_num *)&heartbeat.ack_seqno[0]), time(NULL));
						if ( src_node_same_endianness )
						{													
							heartbeat.type = REPL_HEARTBEAT;
							heartbeat.len = MIN_REPL_MSGLEN;
							QWASSIGN(*(seq_num *)&heartbeat.ack_seqno[0], upd_proc_local->read_jnl_seqno);
						}
						else
						{
							heartbeat.type = GTM_BYTESWAP_32(REPL_HEARTBEAT);
							heartbeat.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
							QWASSIGN(*(seq_num *)&heartbeat.ack_seqno[0], upd_proc_local->read_jnl_seqno);
							*(seq_num *)&heartbeat.ack_seqno[0] = GTM_BYTESWAP_64(*(seq_num *)&heartbeat.ack_seqno[0]);
							*(gtm_time4_t *)&heartbeat.ack_time[0] = GTM_BYTESWAP_32(*(gtm_time4_t *)&heartbeat.ack_time[0]);
						}
						REPL_SEND_LOOP(gtmrecv_sock_fd, &heartbeat, MIN_REPL_MSGLEN,
								FALSE, &gtmrecv_poll_immediate)
						{
							gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
							if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
								return;
						}
						/* Error handling for the above send_loop is not required as it'll be caught
						 * in the next recv_loop of the receiver server */
						if ( !src_node_same_endianness )
						{													
							 *(gtm_time4_t *)&heartbeat.ack_time[0] = GTM_BYTESWAP_32(*(gtm_time4_t *)&heartbeat.ack_time[0]);
							 *(seq_num *)&heartbeat.ack_seqno[0] = GTM_BYTESWAP_64(*(seq_num *)&heartbeat.ack_seqno[0]);
						}
						REPL_DPRINT4("HEARTBEAT sent with time %ld SEQNO %llu at %ld\n",
							     *(gtm_time4_t *)&heartbeat.ack_time[0],
							     (*(seq_num *)&heartbeat.ack_seqno[0]), time(NULL));
					}
					break;

				case REPL_NEED_INSTANCE_INFO:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						need_instinfo_msg = (repl_needinst_msg_ptr_t)(buffp - msg_len - REPL_MSG_HDRLEN);
						repl_log(gtmrecv_log_fp, TRUE, FALSE, "Received REPL_NEED_INSTANCE_INFO message"
							" from primary instance [%s]\n", need_instinfo_msg->instname);
						/* Initialize the remote side protocol version from "proto_ver" field of this msg */
						assert(REPL_PROTO_VER_DUALSITE != need_instinfo_msg->proto_ver);
						assert(REPL_PROTO_VER_UNINITIALIZED != need_instinfo_msg->proto_ver);
						recvpool.gtmrecv_local->remote_proto_ver = need_instinfo_msg->proto_ver;
						assert(REPL_PROTO_VER_MULTISITE <= recvpool.gtmrecv_local->remote_proto_ver);
						/*************** Send REPL_INSTANCE_INFO message ***************/
						memset(&instinfo_msg, 0, sizeof(instinfo_msg));
						if ( src_node_same_endianness )
						{
							instinfo_msg.type = REPL_INSTANCE_INFO;
							instinfo_msg.len = MIN_REPL_MSGLEN;	
						}
						else
						{
							instinfo_msg.type = GTM_BYTESWAP_32(REPL_INSTANCE_INFO);
							instinfo_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);						
						}
						memcpy(instinfo_msg.instname, jnlpool.repl_inst_filehdr->this_instname,
							MAX_INSTNAME_LEN - 1);
						instinfo_msg.was_rootprimary = (unsigned char)repl_inst_was_rootprimary();
						gtmrecv_repl_send((repl_msg_ptr_t)&instinfo_msg, "REPL_INSTANCE_INFO", MAX_SEQNO);
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
						/* Do not allow an instance which was formerly a root primary or which still
						 * has a non-zero value of "zqgblmod_seqno" to start up as a tertiary.
						 */
						if ((instinfo_msg.was_rootprimary || jnlpool.jnlpool_ctl->max_zqgblmod_seqno)
								&& !need_instinfo_msg->is_rootprimary)
						{
							gtm_putmsg(VARLSTCNT(4) ERR_PRIMARYNOTROOT, 2,
								LEN_AND_STR((char *) need_instinfo_msg->instname));
							gtmrecv_autoshutdown();	/* should not return */
							assert(FALSE);
						}
						memcpy(jnlpool_ctl->primary_instname, need_instinfo_msg->instname,
							MAX_INSTNAME_LEN - 1);
					}
					break;

				case REPL_NEED_TRIPLE_INFO:
					assert(!gtmrecv_options.updateresync);	/* source server would not have sent this message
										 * if receiver had specified -UPDATERESYNC */
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{
						assert(REPL_PROTO_VER_UNINITIALIZED != recvpool.gtmrecv_local->remote_proto_ver);
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						need_tripleinfo_msg = (repl_needtriple_msg_ptr_t)(buffp - msg_len
													- REPL_MSG_HDRLEN);
						if ( src_node_same_endianness )
						{													
							input_triple_seqno = need_tripleinfo_msg->seqno;
						}
						else
						{
							input_triple_seqno = GTM_BYTESWAP_64(need_tripleinfo_msg->seqno);
						}
						repl_log(gtmrecv_log_fp, TRUE, FALSE, "Received REPL_NEED_TRIPLE_INFO message"
							" for seqno %llu [0x%llx]\n", input_triple_seqno, input_triple_seqno);
						first_unprocessed_seqno = upd_proc_local->read_jnl_seqno;
						last_valid_triple_seqno = recvpool.recvpool_ctl->last_valid_triple.start_seqno;
						repl_log(gtmrecv_log_fp, TRUE, FALSE, "Update process has processed upto seqno"
							" %llu [0x%llx]\n", first_unprocessed_seqno, first_unprocessed_seqno);
						repl_log(gtmrecv_log_fp, TRUE, FALSE, "Starting seqno of the last valid triple is"
							" %llu [0x%llx]\n", last_valid_triple_seqno, last_valid_triple_seqno);
						if (last_valid_triple_seqno >= first_unprocessed_seqno)
							last_unprocessed_triple_seqno = last_valid_triple_seqno;
						else
							last_unprocessed_triple_seqno = MAX_SEQNO;
						assert(last_valid_triple_seqno < input_triple_seqno);
						if (input_triple_seqno > last_unprocessed_triple_seqno)
						{	/* The primary is requesting triple information for a seqno whose
							 * corresponding triple has also not yet been processed by the update
							 * process (and hence not present in the instance file). Find latest
							 * triple information that is stored in receive pool.
							 */
							repl_log(gtmrecv_log_fp, TRUE, FALSE, "Searching for the desired triple in "
								"the receive pool\n");
							memcpy(&triple, &recvpool_ctl->last_valid_triple, sizeof(repl_triple));
							triple_num = jnlpool.repl_inst_filehdr->num_triples + 1;
								/* "triple_num" is potentially inaccurate (as we dont maintain
								 * a count of the unprocessed triples in the receive pool), but
								 * does not matter to the primary as long as it is non-zero. */
						} else
						{	/* The seqno has been processed by the update process. Hence the triple
							 * for this will be found in the instance file. Search there. */
							assert(NULL != jnlpool.jnlpool_dummy_reg);
							repl_log(gtmrecv_log_fp, TRUE, FALSE, "Searching for the desired triple in "
								"the replication instance file\n");
							repl_inst_ftok_sem_lock();
							status = repl_inst_wrapper_triple_find_seqno(input_triple_seqno,
													&triple, &triple_num);
							repl_inst_ftok_sem_release();
							if (0 != status)
							{	/* Close the connection */
								assert(ERR_REPLINSTNOHIST == status);
								gtmrecv_autoshutdown();	/* should not return */
								assert(FALSE);
							}
							assert(triple.start_seqno < input_triple_seqno);
							assert((triple_num != (jnlpool.repl_inst_filehdr->num_triples - 1))
								|| (triple.start_seqno == jnlpool_ctl->last_triple_seqno));
						}
						gtmrecv_send_triple_info(&triple, triple_num);
						/* Send the triple */
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
					}
					break;

				case REPL_WILL_RESTART_WITH_INFO:
				case REPL_ROLLBACK_FIRST:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{	/* Have received a REPL_WILL_RESTART_WITH_INFO or REPL_ROLLBACK_FIRST message.
						 * If have not yet received a REPL_NEED_INSTANCE_INFO message (which would have
						 * initialized "gtmrecv_local->remote_proto_ver"), it means the remote side does
						 * not understand multi-site replication communication protocol. Note that down.
						 */
						if (REPL_PROTO_VER_UNINITIALIZED == recvpool.gtmrecv_local->remote_proto_ver)
						{
							recvpool.gtmrecv_local->remote_proto_ver = REPL_PROTO_VER_DUALSITE;
							/* Since there can only be ONE receiver server at any point in time and
							 * only the receiver server updates the following fields, it is ok not to
							 * hold the journal pool lock while doing so.
							 */
							jnlpool_ctl->primary_is_dualsite = TRUE;
							jnlpool_ctl->primary_instname[0] = '\0';
						}
						/* Check if our assumed remote protocol version matches the actual. If so, fine.
						 * If not, we need to reset our assumed remote protocol version, close the
						 * current connection and reconnect using the newly assumed protocol version.
						 * This is because if the remote side is dualsite, we will send the resync seqno
						 * across and if it is multisite, we will send the jnl seqno across. But if the
						 * resync seqno and jnl seqno are not different, there is no need to disconnect.
						 * Keep retrying until the assumed and actual protocol versions match.
						 */
						assert(REPL_PROTO_VER_DUALSITE <= recvpool.gtmrecv_local->remote_proto_ver);
						if (REPL_PROTO_VER_DUALSITE != assumed_remote_proto_ver)
						{
							assert(REPL_PROTO_VER_MULTISITE == assumed_remote_proto_ver);
							if (REPL_PROTO_VER_DUALSITE == recvpool.gtmrecv_local->remote_proto_ver)
							{	/* Assumed is multisite, but actual is dualsite. */
								assumed_remote_proto_ver = REPL_PROTO_VER_DUALSITE;
								if (recvpool_ctl->jnl_seqno !=
											recvpool_ctl->max_dualsite_resync_seqno)
								{	/* Resync seqno is different from jnl seqno of secondary. */
									repl_log(gtmrecv_log_fp, TRUE, TRUE, "Primary does not "
										"support multisite functionality. Reconnecting "
										"using dualsite communication protocol.\n");
									repl_close(&gtmrecv_sock_fd);
									repl_connection_reset = TRUE;
									return;
								}
							}
						} else
						{
							if (REPL_PROTO_VER_DUALSITE != recvpool.gtmrecv_local->remote_proto_ver)
							{	/* Assumed dualsite, but actual is multisite. */
								repl_log(gtmrecv_log_fp, TRUE, TRUE,
									"Primary supports multisite functionality. "
									"Reconnecting using multisite communication protocol.\n");
								assumed_remote_proto_ver = REPL_PROTO_VER_MULTISITE;
								repl_close(&gtmrecv_sock_fd);
								repl_connection_reset = TRUE;
								return;
							}
							/* Assumed is dualsite, actual is dualsite. Check if secondary's starting
							 * jnl seqno is NOT EQUAL to the last seqno communicated with the primary
							 * (before startup). If so we have to issue a SECONDAHEAD message and
							 * shut down the secondary. Note that we should NOT look at the secondary's
							 * current jnl seqno as that could potentially be changing (due to update
							 * process concurrently doing updates of seqnos backlogged in the receive
							 * pool).
							 */
							assert(jnlpool.jnlpool_ctl->upd_disabled);
							assert(jnlpool_ctl->jnl_seqno >= recvpool_ctl->max_dualsite_resync_seqno);
							assert(recvpool_ctl->start_jnl_seqno
								>= recvpool_ctl->max_dualsite_resync_seqno);
							if (recvpool_ctl->start_jnl_seqno
								!= recvpool_ctl->max_dualsite_resync_seqno)
							{
								repl_log(gtmrecv_log_fp, TRUE, FALSE,
									"JNLSEQNO last updated by update process = "INT8_FMT
									" "INT8_FMTX" \n",
									INT8_PRINT(recvpool_ctl->max_dualsite_resync_seqno),
									INT8_PRINTX(recvpool_ctl->max_dualsite_resync_seqno));
								repl_log(gtmrecv_log_fp, TRUE, TRUE,
									"JNLSEQNO of this instance at secondary startup = "
									INT8_FMT" "INT8_FMTX" \n",
									INT8_PRINT(recvpool_ctl->start_jnl_seqno),
									INT8_PRINTX(recvpool_ctl->start_jnl_seqno));
								gtm_putmsg(VARLSTCNT(1) ERR_SECONDAHEAD);
								gtmrecv_autoshutdown();	/* should not return */
								assert(FALSE);
							}
						}
						if (jnlpool.repl_inst_filehdr->was_rootprimary)
						{	/* This is the first time an instance that was formerly a root primary
							 * is brought up as an immediate secondary of the new root primary. Once
							 * fetchresync rollback has happened and the receiver and source server
							 * have communicated successfully, the instance file header field that
							 * indicates this was a root primary can be reset to FALSE as the zero
							 * or non-zeroness of the "zqgblmod_seqno" field in the respective
							 * database file headers henceforth controls whether this instance can
							 * be brought up as a tertiary or not. Flush changes to file on disk.
							 */
							repl_inst_ftok_sem_lock();
							jnlpool.repl_inst_filehdr->was_rootprimary = FALSE;
							repl_inst_flush_filehdr();
							repl_inst_ftok_sem_release();
						}
						assert(REPL_PROTO_VER_DUALSITE <= recvpool.gtmrecv_local->remote_proto_ver);
						if ((REPL_PROTO_VER_DUALSITE == recvpool.gtmrecv_local->remote_proto_ver)
							&& is_active_source_server_running())
						{	/* This receiver server has connected to a primary that supports only
							 * dual-site functionality. Check if any active source server is running.
							 * If so, require that the primary be upgraded first.
							 */
							gtm_putmsg(VARLSTCNT(4) ERR_REPLUPGRADEPRI, 2,
								LEN_AND_STR((char *)jnlpool_ctl->primary_instname));
							gtmrecv_autoshutdown();	/* should not return */
							assert(FALSE);
						}
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						start_msg = (repl_start_reply_msg_ptr_t)(buffp - msg_len - REPL_MSG_HDRLEN);
						assert((unsigned long)start_msg % sizeof(seq_num) == 0); /* alignment check */
						QWASSIGN(recvd_jnl_seqno, *(seq_num *)start_msg->start_seqno);
						if (  !src_node_same_endianness )
						{
							recvd_jnl_seqno = GTM_BYTESWAP_64(recvd_jnl_seqno);
						}
						recvpool.gtmrecv_local->last_valid_remote_proto_ver =
							recvpool.gtmrecv_local->remote_proto_ver;
						if (REPL_WILL_RESTART_WITH_INFO == msg_type)
						{
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_WILL_RESTART_WITH_INFO"
								" message. Primary acked the restart point\n");
							remote_jnl_ver = start_msg->jnl_ver;
							REPL_DPRINT3("Local jnl ver is octal %o, remote jnl ver is octal %o\n",
								     jnl_ver, remote_jnl_ver);
							repl_check_jnlver_compat();
							/* older versions zero filler that was in place of start_msg->start_flags,
							 * so we are okay fetching start_msg->start_flags unconditionally.
							 */
							GET_ULONG(recvd_start_flags, start_msg->start_flags);
							if ( !src_node_same_endianness )
							{
								recvd_start_flags = GTM_BYTESWAP_32(recvd_start_flags);
							}
							assert(remote_jnl_ver > V15_JNL_VER || 0 == recvd_start_flags);
							if (remote_jnl_ver <= V15_JNL_VER) /* safety in pro */
								recvd_start_flags = 0;
							primary_side_std_null_coll = (recvd_start_flags & START_FLAG_COLL_M) ?
								TRUE : FALSE;
							if (FALSE != (null_subs_xform = ((primary_side_std_null_coll &&
										!secondary_side_std_null_coll)
									|| (secondary_side_std_null_coll &&
										!primary_side_std_null_coll))))
								null_subs_xform = (primary_side_std_null_coll ?
									STDNULL_TO_GTMNULL_COLL : GTMNULL_TO_STDNULL_COLL);
								/* this sets null_subs_xform regardless of remote_jnl_ver */
							if ((jnl_ver > remote_jnl_ver &&
								IF_NONE != repl_internal_filter[remote_jnl_ver -
								JNL_VER_EARLIEST_REPL][jnl_ver - JNL_VER_EARLIEST_REPL])
								|| (jnl_ver == remote_jnl_ver && 0 != null_subs_xform))
							{
								assert(IF_INVALID !=
									repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
											[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
								assert(IF_INVALID !=
									repl_internal_filter[remote_jnl_ver - JNL_VER_EARLIEST_REPL]
											[jnl_ver - JNL_VER_EARLIEST_REPL]);
								/* reverse transformation should exist */
								assert(IF_NONE !=
									repl_internal_filter[jnl_ver - JNL_VER_EARLIEST_REPL]
											[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
								gtmrecv_filter |= INTERNAL_FILTER;
								gtmrecv_alloc_filter_buff(gtmrecv_max_repl_msglen);
							} else
							{
								gtmrecv_filter &= ~INTERNAL_FILTER;
								if (NO_FILTER == gtmrecv_filter)
									gtmrecv_free_filter_buff();
							}
							/* Don't send any more stopsourcefilter, or updateresync messages */
							gtmrecv_options.stopsourcefilter = FALSE;
							gtmrecv_options.updateresync = FALSE;
							assert(QWEQ(recvd_jnl_seqno, request_from));
							break;
						}
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_ROLLBACK_FIRST message. "
							"Secondary is out of sync with the primary. "
							"Secondary at %llu [0x%llx], Primary at %llu [0x%llx]. "
							"Do ROLLBACK FIRST\n",
							request_from, request_from, recvd_jnl_seqno, recvd_jnl_seqno);
						gtmrecv_autoshutdown();	/* should not return */
						assert(FALSE);
					}
					break;

				case REPL_INST_NOHIST:
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					if (0 == data_len)
					{
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_INST_NOHIST message. "
							"Primary encountered a REPLINSTNOHIST error due to lack of history in "
							"the instance file. Receiver server exiting.\n");
						gtmrecv_autoshutdown();	/* should not return */
						assert(FALSE);
					}
					break;

				default:
					/* Discard the message */
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received UNKNOWN message (type = %d). "
						"Discarding it.\n", msg_type);
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

static void gtmrecv_heartbeat_timer(TID tid, int4 interval_len, int *interval_ptr)
{
	assert(0 != gtmrecv_now);
	UNIX_ONLY(assert(*interval_ptr == heartbeat_period);)	/* interval_len and interval_ptr are dummies on VMS */
	gtmrecv_now += heartbeat_period;
	REPL_DPRINT2("Starting heartbeat timer with %d s\n", heartbeat_period);
	start_timer((TID)gtmrecv_heartbeat_timer, heartbeat_period * 1000, gtmrecv_heartbeat_timer, sizeof(heartbeat_period),
			&heartbeat_period); /* start_timer expects time interval in milli seconds, heartbeat_period is in seconds */
}

static void gtmrecv_main_loop(boolean_t crash_restart)
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

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;

	/* Check all message sizes are the same size (32 bytes = MIN_REPL_MSGLEN) except for the REPL_NEW_TRIPLE message
	 * (repl_triple_msg_t structure) which is 8 bytes more. The receiver server knows to handle different sized messages
	 * only for two messages types REPL_TR_JNL_RECS and REPL_NEW_TRIPLE.
	 */
	assert(MIN_REPL_MSGLEN == sizeof(repl_start_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_start_reply_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_resync_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_needinst_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_needtriple_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_instinfo_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_tripinfo1_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_tripinfo2_msg_t));
	assert(MIN_REPL_MSGLEN < sizeof(repl_triple_msg_t));
	assert(MIN_REPL_MSGLEN == sizeof(repl_heartbeat_msg_t));
	jnl_setver();
	assert(GTMRECV_POLL_INTERVAL < MAX_GTMRECV_POLL_INTERVAL);
	gtmrecv_poll_interval.tv_sec = 0;
	gtmrecv_poll_interval.tv_usec = GTMRECV_POLL_INTERVAL;
	gtmrecv_poll_immediate.tv_sec = 0;
	gtmrecv_poll_immediate.tv_usec = 0;
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
	start_timer((TID)gtmrecv_heartbeat_timer, heartbeat_period * 1000, gtmrecv_heartbeat_timer, sizeof(heartbeat_period),
			&heartbeat_period); /* start_timer expects time interval in milli seconds, heartbeat_period is in seconds */
	assumed_remote_proto_ver = REPL_PROTO_VER_MULTISITE;
	do
	{
		gtmrecv_main_loop(crash_restart);
	} while (repl_connection_reset);
	GTMASSERT; /* shouldn't reach here */
	return;
}
