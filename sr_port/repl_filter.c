/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#ifdef UNIX
#include "gtm_ipc.h"
#include <sys/mman.h>
#include <sys/shm.h>
#elif defined(VMS)
#include <descrip.h>
#endif
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_string.h"
#include <sys/wait.h>
#include "gtm_stdio.h"

#include "iotcp_select.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "cli.h"
#include "iosp.h"
#include "repl_log.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "jnl.h"
#include "repl_filter.h"
#include "repl_ctl.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "gtmmsg.h"
#include "gtmio.h"

GBLREF uchar_ptr_t	repl_filter_buff;
GBLREF int		repl_filter_bufsiz;
GBLREF unsigned char	jnl_ver, remote_jnl_ver;

GBLDEF	int	jnl2filterfmt[REPL_JNL_MAX + 1] =
{
	REPL_FILTER_V12,	/* filter version for REPL_JNL_V12 */
	REPL_FILTER_V12,	/* filter version for REPL_JNL_V13 */
	REPL_FILTER_V12,	/* filter version for REPL_JNL_V14 */
	REPL_FILTER_V15,	/* filter version for REPL_JNL_V15 */
	REPL_FILTER_V16,	/* filter version for REPL_JNL_V16 */
	REPL_FILTER_V17,	/* filter version for REPL_JNL_V17 */
	REPL_FILTER_V17,	/* filter version for REPL_JNL_V18 */
	-1,			/* filter version for REPL_JNL_MAX */
};

GBLDEF	intlfltr_t repl_internal_filter[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1][JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	/* This should be a square matrix. If you add a row, make sure you add a column too. */
                /* 12          13        14           15          16          17          18     */
        /* 12 */{IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO15,  IF_12TO16,  IF_12TO17,  IF_12TO17},
        /* 13 */{IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO15,  IF_12TO16,  IF_12TO17,  IF_12TO17},
        /* 14 */{IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO15,  IF_12TO16,  IF_12TO17,  IF_12TO17},
        /* 15 */{IF_15TO12, IF_15TO12, IF_15TO12, IF_NONE,    IF_15TO16,  IF_15TO17,  IF_15TO17},
        /* 16 */{IF_16TO12, IF_16TO12, IF_16TO12, IF_16TO15,  IF_16TO16,  IF_NONE,    IF_NONE  },
        /* 17 */{IF_17TO12, IF_17TO12, IF_17TO12, IF_17TO15,  IF_NONE,    IF_17TO17,  IF_17TO17},
        /* 18 */{IF_17TO12, IF_17TO12, IF_17TO12, IF_17TO15,  IF_NONE,    IF_17TO17,  IF_17TO17},
};
GBLREF unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF unsigned char	jnl_source_rectype, jnl_dest_maxrectype;
GBLREF seq_num		seq_num_zero, seq_num_one;
GBLREF int4		gv_keysize;
GBLREF gv_key  		*gv_currkey; /* for jnl_extr_init() */
GBLREF bool    		transform; /* for jnl_extr_init() */
LITREF boolean_t	jrt_is_replicated[JRT_RECTYPES];
LITREF int		jrt_update[JRT_RECTYPES];

static	pid_t	repl_filter_pid = -1;
static int 	repl_srv_filter_fd[2] = {FD_INVALID, FD_INVALID};
static int	repl_filter_srv_fd[2] = {FD_INVALID, FD_INVALID};
static char 	*extract_buff;
static char	*extr_rec;
static char	*srv_buff_start, *srv_buff_end, *srv_line_start, *srv_line_end, *srv_read_end;

static struct_jrec_null	null_jnlrec;

static seq_num		save_jnl_seqno;
static boolean_t	is_nontp, is_null;
VMS_ONLY(int decc$set_child_standard_streams(int, int, int);)

void jnl_extr_init(void)
{
	/* Should be a non-filter related function. But for now,... Needs GBLREFs gv_currkey and transform */
	transform = FALSE;      /* to avoid the assert in mval2subsc() */
	gv_keysize = DBKEYSIZE(MAX_KEY_SZ);
	gv_currkey = (gv_key *)malloc(sizeof(gv_key) + gv_keysize);
	gv_currkey->top = gv_keysize;
	gv_currkey->prev = gv_currkey->end = 0;
	gv_currkey->base[0] = '\0';
}

static void repl_filter_close_all_pipes(void)
{
	int	close_res;

	if (FD_INVALID != repl_srv_filter_fd[READ_END])
		F_CLOSE(repl_srv_filter_fd[READ_END], close_res);	/* resets "repl_srv_filter_fd[READ_END]" to FD_INVALID */
	if (FD_INVALID != repl_srv_filter_fd[WRITE_END])
		F_CLOSE(repl_srv_filter_fd[WRITE_END], close_res); /* resets "_CLOSE(repl_srv_filter_fd[WRITE_END]" to FD_INVALID */
	if (FD_INVALID != repl_filter_srv_fd[READ_END])
		F_CLOSE(repl_filter_srv_fd[READ_END], close_res);	/* resets "repl_filter_srv_fd[READ_END]" to FD_INVALID */
	if (FD_INVALID != repl_filter_srv_fd[WRITE_END])
		F_CLOSE(repl_filter_srv_fd[WRITE_END], close_res);	/* resets "repl_filter_srv_fd[WRITE_END]" to FD_INVALID */

}

int repl_filter_init(char *filter_cmd)
{
	int		fcntl_res, status, argc, delim_count, close_res;
	char		cmd[4096], *delim_p;
	char_ptr_t	arg_ptr, argv[MAX_FILTER_ARGS];
	error_def(ERR_REPLFILTER);
	error_def(ERR_TEXT);

	REPL_DPRINT1("Initializing FILTER\n");
	repl_filter_close_all_pipes();
	/* Set up pipes for filter I/O */
	/* For Server -> Filter */
	OPEN_PIPE(repl_srv_filter_fd, status);
	if (0 > status)
	{
		repl_filter_close_all_pipes();
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not create pipe for Server->Filter I/O"), ERRNO);
		repl_errno = EREPL_FILTERSTART_PIPE;
		return(FILTERSTART_ERR);
	}
	/* For Filter -> Server */
	OPEN_PIPE(repl_filter_srv_fd, status);
	if (0 > status)
	{
		repl_filter_close_all_pipes();
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not create pipe for Server->Filter I/O"), ERRNO);
		repl_errno = EREPL_FILTERSTART_PIPE;
		return(FILTERSTART_ERR);
	}
	/* Parse the filter_cmd */
	repl_log(stdout, FALSE, TRUE, "Filter command is %s\n", filter_cmd);
	strcpy(cmd, filter_cmd);
	if (NULL == (arg_ptr = strtok(cmd, FILTER_CMD_ARG_DELIM_TOKENS)))
	{
		repl_filter_close_all_pipes();
		gtm_putmsg(VARLSTCNT(6) ERR_REPLFILTER, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Null filter command specified"));
		repl_errno = EREPL_FILTERSTART_NULLCMD;
		return(FILTERSTART_ERR);
	}
	argv[0] = arg_ptr;
	for (argc = 1; NULL != (arg_ptr = strtok(NULL, FILTER_CMD_ARG_DELIM_TOKENS)); argc++)
		argv[argc] = arg_ptr;
	argv[argc] = NULL;
	REPL_DPRINT2("Arg %d is NULL\n", argc);
	REPL_DEBUG_ONLY(
	{
		int index;
		for (index = 0; argv[index]; index++)
		{
			REPL_DPRINT3("Filter Arg %d : %s\n", index, argv[index]);
		}
		REPL_DPRINT2("Filter argc %d\n", index);
	}
	)
	if (0 < (repl_filter_pid = UNIX_ONLY(fork)VMS_ONLY(vfork)()))
	{ /* Server */
		UNIX_ONLY(
			F_CLOSE(repl_srv_filter_fd[READ_END], close_res); /* SERVER: WRITE only on server -> filter pipe;
								* also resets "repl_srv_filter_fd[READ_END]" to FD_INVALID */
			F_CLOSE(repl_filter_srv_fd[WRITE_END], close_res); /* SERVER: READ only on filter -> server pipe;
								* also resets "repl_srv_filter_fd[WRITE_END]" to FD_INVALID */
		)
		memset((char *)&null_jnlrec, 0, NULL_RECLEN);
		null_jnlrec.prefix.jrec_type = JRT_NULL;
		null_jnlrec.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		null_jnlrec.prefix.forwptr = null_jnlrec.suffix.backptr = NULL_RECLEN;
		assert(NULL == extr_rec);
		jnl_extr_init();
		extr_rec = malloc(ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE) + 1); /* +1 to accommodate terminating null */
		assert(MAX_EXTRACT_BUFSIZ > ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE));
		extract_buff = malloc(MAX_EXTRACT_BUFSIZ + 1); /* +1 to accommodate terminating null */
		srv_line_start = srv_line_end = srv_read_end = srv_buff_start = malloc(MAX_EXTRACT_BUFSIZ + 1);
		srv_buff_end = srv_buff_start + MAX_EXTRACT_BUFSIZ + 1;
		return(SS_NORMAL);
	}
	if (0 == repl_filter_pid)
	{ /* Filter */
		UNIX_ONLY(
			F_CLOSE(repl_srv_filter_fd[WRITE_END], close_res); /* FILTER: READ only on server -> filter pipe;
								* also resets "repl_srv_filter_fd[WRITE_END]" to FD_INVALID */
			F_CLOSE(repl_filter_srv_fd[READ_END], close_res); /* FILTER: WRITE only on filter -> server pipe;
								* also resets "repl_srv_filter_fd[READ_END]" to FD_INVALID */
			/* Make the server->filter pipe stdin for filter */
			DUP2(repl_srv_filter_fd[READ_END], 0, status);
			if (0 > status)
				GTMASSERT;
			/* Make the filter->server pipe stdout for filter */
			DUP2(repl_filter_srv_fd[WRITE_END], 1, status);
			if (0 > status)
				GTMASSERT;
		)
		VMS_ONLY(decc$set_child_standard_streams(repl_srv_filter_fd[READ_END], repl_filter_srv_fd[WRITE_END], -1));
		/* Start the filter */
		if (0 > EXECV(argv[0], argv))
		{ /* exec error, close all pipe fds */
			repl_filter_close_all_pipes();
			VMS_ONLY(
				/* For vfork(), there is no real child process. So, both ends of both the pipes have to be closed */
				F_CLOSE(repl_srv_filter_fd[WRITE_END], close_res);
					/* resets "repl_srv_filter_fd[WRITE_END]" to FD_INVALID */
				F_CLOSE(repl_filter_srv_fd[READ_END], close_res);
					/* resets "repl_filter_srv_fd[READ_END]" to FD_INVALID */
			)
			gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Could not exec filter"), ERRNO);
			repl_errno = EREPL_FILTERSTART_EXEC;
			return(FILTERSTART_ERR);
		}
	} else
	{
		/* Error in fork */
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Could not fork filter"), ERRNO);
		repl_errno = EREPL_FILTERSTART_FORK;
		return(FILTERSTART_ERR);
	}

	return -1; /* This should never get executed, added to make compiler happy */
}

static int repl_filter_send(seq_num tr_num, unsigned char *tr, int tr_len)
{
	/* Send the transaction tr_num in buffer tr of len tr_len to the filter */
	ssize_t	extr_len, send_len, sent_len;
	char	first_rectype, *send_ptr, *extr_end;

	if (QWNE(tr_num, seq_num_zero))
	{
		first_rectype = ((jnl_record *)tr)->prefix.jrec_type;
		is_nontp = !IS_FENCED(first_rectype);
		is_null = (JRT_NULL == first_rectype);
		save_jnl_seqno = GET_JNL_SEQNO(tr);
		if (NULL == (extr_end = jnl2extcvt((jnl_record *)tr, tr_len, extract_buff)))
			GTMASSERT;
		extr_len = extr_end - extract_buff;
		extract_buff[extr_len] = '\0';
	} else
	{
		is_nontp = TRUE;
		is_null = FALSE;
		strcpy(extract_buff, FILTER_EOT);
		extr_len = strlen(FILTER_EOT);
	}
	REPL_DEBUG_ONLY(
	if (QWNE(tr_num, seq_num_zero))
	{
		REPL_DPRINT3("Extract for tr %llu :\n%s\n", tr_num, extract_buff);
	} else
	{
		REPL_DPRINT1("Sending FILTER_EOT\n");
	}
	);

	for (send_ptr = extract_buff, send_len = extr_len; send_len > 0; send_ptr += sent_len, send_len -= sent_len)
	{
		/* the check for EINTR below is valid and should not be converted to an EINTR wrapper macro, because EAGAIN
		 * is also being checked. */
		while (0 > (sent_len = write(repl_srv_filter_fd[WRITE_END], send_ptr, send_len)) &&
				(errno == EINTR || errno == EAGAIN));
		if (0 > sent_len)
		{
			repl_errno = (EPIPE == errno) ? EREPL_FILTERNOTALIVE : EREPL_FILTERSEND;
			return (ERRNO);
		}
	}
	return(SS_NORMAL);
}

#if 0 /* Not used for now - Defed out for fixing compiler warnings. This was never used earlier too */

static void wait_for_filter_input(void)
{
	fd_set	filter_input_fd;
	int	status;

	FD_ZERO(&filter_input_fd);
	FD_SET(repl_filter_srv_fd[READ_END], &filter_input_fd);
	/* the check for EINTR below is valid and should not be converted to an EINTR wrapper macro, because EAGAIN is also
	 * being checked. */
	while (0 > (status = select(repl_filter_srv_fd[READ_END] + 1, &filter_input_fd, NULL, NULL, NULL)) &&
			(errno == EINTR || errno == EAGAIN));

	return;
}

#endif /* 0 */

static int repl_filter_recv_line(char *line, int *line_len, int max_line_len)
{ /* buffer input read from repl_filter_srv_fd[READ_END], return one line at a time; line separator is '\n' */

	int	save_errno;
        ssize_t l_len, r_len, buff_remaining ;

	for (; ;)
	{
		for (; srv_line_end < srv_read_end && '\n' != *srv_line_end; srv_line_end++)
			;
		if (srv_line_end < srv_read_end) /* newline found */
		{
			l_len = (ssize_t)(srv_line_end - srv_line_start + 1); /* include '\n' in length */
			*line_len = (int4)l_len ;

			if ((int)l_len <= max_line_len)
			{
				memcpy(line, srv_line_start, l_len);
				srv_line_start = ++srv_line_end; /* move past '\n' for next line */
				assert(srv_line_end <= srv_read_end);
				REPL_EXTRA_DPRINT3("repl_filter: newline found, srv_line_end: 0x%x srv_read_end: 0x%x\n",
							srv_line_end, srv_read_end);
				return SS_NORMAL;
			}
			return (repl_errno = EREPL_FILTERNOSPC);
		}
		/* newline not found, may have to read more data */
		assert(srv_line_end == srv_read_end);
		l_len = srv_read_end - srv_line_start;
		memmove(srv_buff_start, srv_line_start, l_len);
		REPL_EXTRA_DPRINT4("repl_filter: moving %d bytes from 0x%x to 0x%x\n", l_len, srv_line_start, srv_buff_start);
		srv_line_start = srv_buff_start;
		srv_line_end = srv_read_end = srv_line_start + l_len;
		if (0 < (buff_remaining = srv_buff_end - srv_read_end))
		{
			while (-1 == (r_len = read(repl_filter_srv_fd[READ_END], srv_read_end, buff_remaining)) &&
					(EINTR == errno || EAGAIN == errno || ENOMEM == errno))
				;
			if (0 < r_len) /* successful read */
			{
				srv_read_end += r_len;
				REPL_EXTRA_DPRINT5("repl_filter: b %d srv_line_start: 0x%x srv_line_end: 0x%x srv_read_end: 0x%x\n",
							r_len, srv_line_start, srv_line_end, srv_read_end);
				assert(srv_line_end < srv_read_end);
				continue; /* continue looking for new line */
			}
			save_errno = errno;
			if (0 == r_len) /* EOF */
				return (repl_errno = EREPL_FILTERNOTALIVE);
			/* (0 > r_len) => error */
			repl_errno = EREPL_FILTERRECV;
			return save_errno;
		}
		/* srv_read_end == srv_buff_end => buffer is full but no new-line; since we allocated enough buffer for the largest
		 * possible logical record, this should be a bad conversion from the filter */
		return (repl_errno = EREPL_FILTERBADCONV);
	}
}

static int repl_filter_recv(seq_num tr_num, unsigned char *tr, int *tr_len)
{
	/* Receive the transaction tr_num into buffer tr. Return the length of the transaction received in tr_len */
	int		firstrec_len, tcom_len, rec_cnt, extr_len, extr_reclen, status;
	int		eof, err, save_errno;
	unsigned char	seq_num_str[32], *seq_num_ptr;
	char		*extr_ptr, *tr_end, *fgets_res;

	assert(NULL != extr_rec);
	if (SS_NORMAL != (status = repl_filter_recv_line(extr_rec, &firstrec_len, ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE))))
	{
		if (EREPL_FILTERNOSPC == repl_errno)
			GTMASSERT; /* why didn't we pre-allocate enough memory? */
		return status;
	}
	extr_rec[firstrec_len] = '\0';
	if (!('0' == extr_rec[0] && ('8' == extr_rec[1] || '0' == extr_rec[1]))) /* First record should be TSTART or NULL */
		return (repl_errno = EREPL_FILTERBADCONV);
	memcpy(extract_buff, extr_rec, firstrec_len + 1); /* include terminating null */
	extr_reclen = extr_len = firstrec_len;
	rec_cnt = 0;
	REPL_DEBUG_ONLY(extr_rec[extr_reclen] = '\0';)
	REPL_DPRINT6("Filter output for "INT8_FMT" :\nrec_cnt: %d\textr_reclen: %d\textr_len: %d\t%s", INT8_PRINT(tr_num),
			rec_cnt, extr_reclen, extr_len, extr_rec);
	if (!is_null && ('0' != extr_rec[0] || '0' != extr_rec[1])) /* if not NULL record */
	{
		while ('0' != extr_rec[0] || '9' != extr_rec[1]) /* while not TCOM record */
		{
			if (SS_NORMAL != (status = repl_filter_recv_line(extr_rec, &extr_reclen,
									 ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE))))
			{
				if (EREPL_FILTERNOSPC == repl_errno)
					GTMASSERT; /* why didn't we pre-allocate enough memory? */
				return status;
			}
			memcpy(extract_buff + extr_len, extr_rec, extr_reclen);
			extr_len += extr_reclen;
			rec_cnt++;
			REPL_DEBUG_ONLY(extr_rec[extr_reclen] = '\0';)
			REPL_DPRINT5("rec_cnt: %d\textr_reclen: %d\textr_len: %d\t%s", rec_cnt, extr_reclen, extr_len, extr_rec);
		}
		tcom_len = extr_reclen;
		rec_cnt--;
	}
	if (0 < rec_cnt)
	{
		extr_ptr = extract_buff;
		if (is_nontp)
		{
			if (1 == rec_cnt)
			{
				extr_ptr = extract_buff + firstrec_len; /* Eliminate the dummy TSTART */
				extr_len -= firstrec_len;
				extr_len -= tcom_len; /* Eliminate the dummy TCOMMIT */
			}
		}
		extr_ptr[extr_len] = '\0'; /* terminate with null for ext2jnlcvt */
		if (NULL == (tr_end = ext2jnlcvt(extr_ptr, extr_len, (jnl_record *)tr)))
			return (repl_errno = EREPL_FILTERBADCONV);
		*tr_len = (int4)(tr_end - (char *)&tr[0]);
		/* TCOM record for non TP converted to TP must have the same seqno as the original non TP record */
		assert(!is_nontp || 1 == rec_cnt || QWEQ(save_jnl_seqno, ((struct_jrec_tcom *)tr_end)->token_seq.jnl_seqno));
	} else /* 0 == rec_cnt */
	{ /* Transaction filtered out, put a JRT_NULL record; the prefix.{pini_addr,time,tn} fields are not filled in as they are
	   * not relevant on the secondary */
		QWASSIGN(null_jnlrec.jnl_seqno, save_jnl_seqno);
		memcpy(tr, (char *)&null_jnlrec, NULL_RECLEN);
		*tr_len = NULL_RECLEN;
	}
	return(SS_NORMAL);
}

int repl_filter(seq_num tr_num, unsigned char *tr, int *tr_len, int bufsize)
{
	int status;

	if (SS_NORMAL != (status = repl_filter_send(tr_num, tr, *tr_len)))
		return (status);
	return (repl_filter_recv(tr_num, tr, tr_len));
}

int repl_stop_filter(void)
{
	/* Send a special record to indicate stop */
	int	filter_exit_status, waitpid_res;

	REPL_DPRINT1("Stopping filter\n");
	repl_filter_send(seq_num_zero, NULL, 0);
	repl_filter_close_all_pipes();
	free(extr_rec);
	free(extract_buff);
	free(srv_buff_start);
	extr_rec = extract_buff = srv_buff_start = NULL;
	repl_log(stdout, TRUE, TRUE, "Waiting for Filter to Stop\n");
	WAITPID(repl_filter_pid, &filter_exit_status, 0, waitpid_res); /* Release the defunct filter */
	repl_log(stdout, TRUE, TRUE, "Filter Stopped\n");
	return (SS_NORMAL);
}

void repl_filter_error(seq_num filter_seqno, int why)
{

	error_def(ERR_FILTERNOTALIVE);
	error_def(ERR_FILTERCOMM);
	error_def(ERR_FILTERBADCONV);

	repl_log(stderr, TRUE, TRUE, "Stopping filter due to error\n");
	repl_stop_filter();
	switch (repl_errno)
	{
		case EREPL_FILTERNOTALIVE :
			rts_error(VARLSTCNT(3) ERR_FILTERNOTALIVE, 1, &filter_seqno);
			break;

		case EREPL_FILTERSEND :
			rts_error(VARLSTCNT(4) ERR_FILTERCOMM, 1, &filter_seqno, why);
			break;

		case EREPL_FILTERBADCONV :
			rts_error(VARLSTCNT(3) ERR_FILTERBADCONV, 1, &filter_seqno);
			break;

		case EREPL_FILTERRECV :
			rts_error(VARLSTCNT(4) ERR_FILTERCOMM, 1, &filter_seqno, why);
			break;

		default :
			GTMASSERT;
	}
	return;
}

void repl_check_jnlver_compat(void)
{
	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);

	/* see comment in repl_filter.h about list of filter-formats, jnl-formats and GT.M versions */
	UNIX_ONLY(assert(JNL_VER_EARLIEST_REPL <= remote_jnl_ver);) /* remote must be V4.3-000+ */
	VMS_ONLY(assert(V13_JNL_VER <= remote_jnl_ver);) /* remote must be V4.3-001+ */
	if (VMS_ONLY(V13_JNL_VER) UNIX_ONLY(JNL_VER_EARLIEST_REPL) > remote_jnl_ver)
		rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Dual site configuration not supported between these two GT.M versions"));
}
