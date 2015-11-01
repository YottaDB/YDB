/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#include <sys/stat.h>
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
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "gtm_event_log.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "repl_filter.h"
#include "repl_log.h"
#include "sgtm_putmsg.h"
#include "longcpy.h"		/* for longcpy() prototype */

GBLREF	gd_addr			*gd_header;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	int			gtmsource_sock_fd;
GBLREF  seq_num                 gtmsource_save_read_jnl_seqno;
GBLREF	struct timeval		gtmsource_poll_wait, gtmsource_poll_immediate;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	unsigned char		*gtmsource_tcombuff_end;
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
GBLREF  repl_ctl_element        *repl_ctl_list;

void gtmsource_init_sec_addr(struct sockaddr_in *secondary_addr)
{
	gtmsource_local_ptr_t	gtmsource_local;

	gtmsource_local = jnlpool.gtmsource_local;
	memset((char *)secondary_addr, 0, sizeof(*secondary_addr));
	(*secondary_addr).sin_family = AF_INET;
	(*secondary_addr).sin_addr.s_addr = gtmsource_local->secondary_inet_addr;
	(*secondary_addr).sin_port = htons(gtmsource_local->secondary_port);
}

int gtmsource_est_conn(struct sockaddr_in *secondary_addr)
{
	int			connection_attempts, alert_attempts, save_errno, connect_res;
	char			print_msg[1024], msg_str[1024];
	gtmsource_local_ptr_t	gtmsource_local;

	error_def(ERR_REPLWARN);

	gtmsource_local = jnlpool.gtmsource_local;
	/* Connect to the secondary - use hard tries, soft tries ... */
	connection_attempts = 0;
	gtmsource_comm_init();
	repl_log(gtmsource_log_fp, FALSE, TRUE, "Connect hard tries count = %d, Connect hard tries period = %d\n",
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT],
		 gtmsource_local->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD]);
	do
	{
		CONNECT_SOCKET(gtmsource_sock_fd, (struct sockaddr *)secondary_addr, sizeof(*secondary_addr), connect_res);
		if (0 == connect_res)
			break;
		repl_log(gtmsource_log_fp, FALSE, FALSE, "%d hard connection atempt failed : %s\n", connection_attempts + 1,
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
		repl_log(gtmsource_log_fp, FALSE, TRUE, "Soft tries period = %d, Alert period = %d\n",
			 gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD],
			 alert_attempts * gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
		connection_attempts = 0;
		do
		{
			CONNECT_SOCKET(gtmsource_sock_fd, (struct sockaddr *)secondary_addr, sizeof(*secondary_addr), connect_res);
			if (0 == connect_res)
				break;
			repl_close(&gtmsource_sock_fd);
			repl_log(gtmsource_log_fp, FALSE, TRUE, "%d soft connection attempt failed : %s\n",
				 connection_attempts + 1, STRERROR(ERRNO));
			LONG_SLEEP(gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			gtmsource_comm_init();
			connection_attempts++;
			if (0 == connection_attempts % alert_attempts)
			{
				/* Log ALERT message */
				SPRINTF(msg_str, "GTM Replication Source Server : Could not connect to secondary in %d seconds\n",
					connection_attempts *
					gtmsource_local->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);
				sgtm_putmsg(print_msg, VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(msg_str));
				repl_log(gtmsource_log_fp, TRUE, TRUE, print_msg);
				gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "REPLWARN", print_msg);
			}
		} while (TRUE);
	}
	return (SS_NORMAL);
}

int gtmsource_alloc_tcombuff(void)
{
	/* Allocate buffer for TCOM, ZTCOM records */
	int	max_tcombufsiz;

	max_tcombufsiz = gd_header->n_regions * TCOM_RECLEN;

	if (!gtmsource_tcombuff_start &&
	    NULL == (gtmsource_tcombuff_start = (unsigned char *)malloc(max_tcombufsiz)))
		return (ERRNO);

	gtmsource_tcombuffp = gtmsource_tcombuff_start;
	gtmsource_tcombuff_end = gtmsource_tcombuff_start + max_tcombufsiz;

	return (SS_NORMAL);
}

int gtmsource_alloc_filter_buff(int bufsiz)
{
	uchar_ptr_t	old_filter_buff;

	if (gtmsource_filter != NO_FILTER && repl_filter_bufsiz < bufsiz)
	{
		REPL_DPRINT3("Expanding filter buff from %d to %d\n", repl_filter_bufsiz, bufsiz);
		old_filter_buff = repl_filter_buff;
		repl_filter_buff = (uchar_ptr_t)malloc(bufsiz);
		if (old_filter_buff)
		{
			longcpy(repl_filter_buff, old_filter_buff, repl_filter_bufsiz);
			free(old_filter_buff);
		}
		repl_filter_bufsiz = bufsiz;
	}
	return (SS_NORMAL);
}

int gtmsource_alloc_msgbuff(int maxbuffsize)
{
	/* Allocate msg buffer */

	repl_msg_ptr_t	oldmsgp;

	if (maxbuffsize > gtmsource_msgbufsiz || NULL == gtmsource_msgp)
	{
		oldmsgp = gtmsource_msgp;
		maxbuffsize = ROUND_UP(maxbuffsize, OS_PAGELET_SIZE);
		if (MIN_REPL_MSGLEN >= maxbuffsize)
			maxbuffsize = ROUND_UP(MIN_REPL_MSGLEN, OS_PAGELET_SIZE);
		gtmsource_msgp = (repl_msg_ptr_t)malloc(maxbuffsize);
		if (oldmsgp)
		{
			longcpy((uchar_ptr_t)gtmsource_msgp, (uchar_ptr_t)oldmsgp, gtmsource_msgbufsiz);
									/* Copy the existing data */
			free(oldmsgp);
		}
		gtmsource_msgbufsiz = maxbuffsize;
		gtmsource_alloc_filter_buff(gtmsource_msgbufsiz);
	}
	return (SS_NORMAL);
}

int gtmsource_recv_restart(seq_num *recvd_jnl_seqno, int *msg_type, int *start_flags)
{
	/* Receive jnl_seqno for (re)starting transmission */

	fd_set		input_fds;
	repl_msg_t	msg;
	unsigned char	*msg_ptr;
	int		recv_len, recvd_len, send_len, sent_len;
	int		status;
	unsigned char	seq_num_str[32], *seq_num_ptr;
	repl_msg_t	xoff_ack;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);
	error_def(ERR_UNIMPLOP);

	status = SS_NORMAL;
	for (; SS_NORMAL == status;)
	{
		repl_log(gtmsource_log_fp, FALSE, FALSE, "Waiting for (re)start JNL_SEQNO/FETCH RESYSNC msg\n");
		REPL_RECV_LOOP(gtmsource_sock_fd, &msg, MIN_REPL_MSGLEN, &gtmsource_poll_wait)
		{
			gtmsource_poll_actions(FALSE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
		}
		if (SS_NORMAL == status)
		{
			assert(msg.type == REPL_START_JNL_SEQNO || msg.type == REPL_FETCH_RESYNC || msg.type == REPL_XOFF_ACK_ME);
			assert(msg.len == MIN_REPL_MSGLEN);
			*msg_type = msg.type;
			*start_flags = START_FLAG_NONE;
			QWASSIGN(*recvd_jnl_seqno, *(seq_num *)&msg.msg[0]);
			if (REPL_START_JNL_SEQNO == msg.type)
			{
				repl_log(gtmsource_log_fp, FALSE, FALSE, "Received (re)start JNL_SEQNO msg %d bytes\n", recvd_len);
				repl_log(gtmsource_log_fp, FALSE, FALSE, "Received (re)start JNL_SEQNO msg %d bytes. seq no "
									 INT8_FMT"\n", recvd_len, INT8_PRINT(*recvd_jnl_seqno));
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
				return (SS_NORMAL);
			} else if (REPL_FETCH_RESYNC == msg.type)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "FETCH RESYNC msg received with SEQNO "INT8_FMT"\n",
					 INT8_PRINT(*(seq_num *)&msg.msg[0]));
				return (SS_NORMAL);
			} else if (REPL_XOFF_ACK_ME == msg.type)
			{
				repl_log(gtmsource_log_fp, FALSE, FALSE, "XOFF received when waiting for (re)start JNL_SEQNO/FETCH "
									"RESYSNC msg. Possible crash/shutdown of update process\n");
				/* Send XOFF_ACK */
				xoff_ack.type = REPL_XOFF_ACK;
				QWASSIGN(*(seq_num *)&xoff_ack.msg[0], *recvd_jnl_seqno);
				xoff_ack.len = MIN_REPL_MSGLEN;
				REPL_SEND_LOOP(gtmsource_sock_fd, &xoff_ack, xoff_ack.len, &gtmsource_poll_immediate)
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
	uint4			tmp_read, prev_tmp_read, prev_tr_size;
	int			save_lastwrite_len;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	gd_region		*reg, *region_top;
	sgmnt_addrs		*csa;
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;

	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	if (recvd_start_flags & START_FLAG_UPDATERESYNC)
	{
		grab_lock(jnlpool.jnlpool_dummy_reg);
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

		QWASSIGN(tmp_read_addr, gtmsource_local->read_addr);
		QWASSIGN(tmp_read_jnl_seqno, gtmsource_local->read_jnl_seqno);
		tmp_read = gtmsource_local->read;

		/* If there is no more input to be read, the previous
		 * transaction size should not be read from the journal pool
		 * since the read pointers point to the next read
		 */
		if (jnlpool_hasnt_overflowed(tmp_read_addr) &&
	            QWGT(tmp_read_jnl_seqno, recvd_jnl_seqno) &&
	            QWGT(tmp_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			if (QWGE(jctl->early_write_addr, tmp_read_addr))
			{
				save_lastwrite_len = jctl->lastwrite_len;
				if (QWEQ(jctl->early_write_addr, tmp_read_addr))
				{
					QWDECRBYDW(tmp_read_addr, save_lastwrite_len);
					QWDECRBYDW(tmp_read_jnl_seqno, 1);
					prev_tmp_read = tmp_read;
					tmp_read -= save_lastwrite_len;
					if (tmp_read >= prev_tmp_read)
						tmp_read += jctl->jnlpool_size;
					assert(tmp_read == QWMODDW(tmp_read_addr, jctl->jnlpool_size));
					REPL_DPRINT2("Srch restart : No more input in jnlpool, backing off to read_jnl_seqno : "
						     INT8_FMT, INT8_PRINT(tmp_read_jnl_seqno));
					REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n", INT8_PRINT(tmp_read_addr), tmp_read);
				}
			}
			while (QWEQ(tmp_read_jnl_seqno, jctl->jnl_seqno))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "SEARCHING RESYNC POINT IN POOL : Waiting for GTM process "
								       "to finish writing journal records to the pool\n");
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
				gtmsource_poll_actions(FALSE);
			}
		}

		while (jnlpool_hasnt_overflowed(tmp_read_addr) &&
	               QWGT(tmp_read_jnl_seqno, recvd_jnl_seqno) &&
	               QWGT(tmp_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			assert(tmp_read + sizeof(jnldata_hdr_struct) <= jctl->jnlpool_size);
			prev_tr_size = ((jnldata_hdr_ptr_t)(jnlpool.jnldata_base + tmp_read))->prev_jnldata_len;
			if (jnlpool_hasnt_overflowed(tmp_read_addr))
			{
				QWDECRBYDW(tmp_read_addr, prev_tr_size);
				prev_tmp_read = tmp_read;
				tmp_read -= prev_tr_size;
				if (tmp_read >= prev_tmp_read)
					tmp_read += jctl->jnlpool_size;
				assert(tmp_read == QWMODDW(tmp_read_addr, jctl->jnlpool_size));
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

		if (jnlpool_hasnt_overflowed(tmp_read_addr) &&
	            QWEQ(tmp_read_jnl_seqno, recvd_jnl_seqno) &&
	    	    QWGE(tmp_read_jnl_seqno, jctl->start_jnl_seqno))
		{
			REPL_DPRINT2("Srch restart : Now in READ_POOL state read_jnl_seqno : "INT8_FMT,
				     INT8_PRINT(tmp_read_jnl_seqno));
			REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n",INT8_PRINT(tmp_read_addr), tmp_read);
		} else
		{
			/* Overflow, or requested seqno too far back to be
			 * in pool */
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
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal files. Tr num = "INT8_FMT
							       "\n", INT8_PRINT(recvd_jnl_seqno));
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
	}

	QWASSIGN(gtmsource_local->read_jnl_seqno, recvd_jnl_seqno);

	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		csa = &FILE_INFO(reg)->s_addrs;
		if (REPL_ENABLED(csa->hdr))
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

int gtmsource_get_jnlrecs(uchar_ptr_t buff, int *data_len, int maxbufflen)
{
	int 			status, prev_read_state;
	unsigned char		seq_num_str[32], *seq_num_ptr;
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;

	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	if (QWEQ(gtmsource_local->read_jnl_seqno, jctl->jnl_seqno))
	{
		/* Nothing to read */
		*data_len = 0;
		return (0);
	}

#ifdef GTMSOURCE_ALWAYS_READ_FILES
	gtmsource_local->read_state = READ_FILE;
#endif

	/* Something to be read */

	prev_read_state = gtmsource_local->read_state;

	switch(prev_read_state)
	{
		case READ_POOL:
			if (0 == (status = gtmsource_readpool(buff, data_len, maxbufflen)))
				return (0);
			else if (0 < *data_len)
				return (-1);
			/* else status == -1 && *data_len == -1 */
			/* Overflow, switch to READ_FILE */
			gtmsource_local->read_state = READ_FILE;

			QWASSIGN(gtmsource_save_read_jnl_seqno, gtmsource_local->read_jnl_seqno);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal files. Tr num = "INT8_FMT
				 "\n", INT8_PRINT(gtmsource_save_read_jnl_seqno));

			/* CAUTION : FALL THROUGH */

		case READ_FILE:

			if (READ_POOL == prev_read_state || gtmsource_pool2file_transition /* read_pool -> read_file transition */
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
			if (0 == (status = gtmsource_readfiles(buff, data_len, maxbufflen)))
				return (0);
			else if (0 < *data_len)
				return (-1);
			/* else status == -1 && *data_len == -1 */
			GTMASSERT;
	}
}
