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

#include <arpa/inet.h>
#include <netinet/in.h>
#ifdef UNIX
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#endif
#include <fcntl.h>
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

GBLREF uchar_ptr_t	repl_filter_buff;
GBLREF int		repl_filter_bufsiz;
GBLREF unsigned char	jnl_ver, remote_jnl_ver;
GBLDEF	intlfltr_t repl_internal_filter[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1][JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	/* This should be a square matrix. If you add a row, make sure you add a column too. */
		/*  11         12         13	     14     	15	*/
	/* 11 */{IF_NONE,   IF_11TO12, IF_11TO12, IF_11TO12, IF_11TO15},
	/* 12 */{IF_12TO11, IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO15},
	/* 13 */{IF_12TO11, IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO15},
	/* 14 */{IF_12TO11, IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO15},
	/* 15 */{IF_15TO11, IF_15TO12, IF_15TO12, IF_15TO12, IF_NONE  },
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
static int 	repl_srv_filter_fd[2] = {-1, -1}, repl_filter_srv_fd[2] = {-1, -1};
static FILE 	*repl_srv_filter_write_fp = NULL, *repl_srv_filter_read_fp = NULL, *repl_filter_srv_write_fp = NULL,
		*repl_filter_srv_read_fp = NULL;
static char 	*extract_buff;
static char	*extr_rec;

static struct_jrec_null	null_jnlrec;

static seq_num		save_jnl_seqno;
static boolean_t	is_nontp, is_null;
VMS_ONLY(int decc$set_child_standard_streams(int, int, int);)

void jnl_extr_init(void)
{
	/* Should be a non-filter related function. But for now,... Needs GBLREFs gv_currkey and transform */
	transform = FALSE;      /* to avoid the assert in mval2subsc() */
	gv_keysize = (MAX_KEY_SZ + MAX_NUM_SUBSC_LEN + 4) & (-4);
	gv_currkey = (gv_key *)malloc(sizeof(gv_key) + gv_keysize);
	gv_currkey->top = gv_keysize;
	gv_currkey->prev = gv_currkey->end = 0;
	gv_currkey->base[0] = '\0';
}

int repl_filter_init(char *filter_cmd)
{
	int		fcntl_res, status, argc, delim_count;
	char		cmd[4096], *delim_p;
	char_ptr_t	arg_ptr, argv[MAX_FILTER_ARGS];
	error_def(ERR_REPLFILTER);
	error_def(ERR_TEXT);

	REPL_DPRINT1("Initializing FILTER\n");
	if (-1 != repl_srv_filter_fd[READ_END])
		close(repl_srv_filter_fd[READ_END]);
	if (-1 != repl_srv_filter_fd[WRITE_END])
		close(repl_srv_filter_fd[WRITE_END]);
	if (-1 != repl_filter_srv_fd[READ_END])
		close(repl_filter_srv_fd[READ_END]);
	if (-1 != repl_filter_srv_fd[WRITE_END])
		close(repl_filter_srv_fd[WRITE_END]);
	/* Set up pipes for filter I/O */
	/* For Server -> Filter */
	OPEN_PIPE(repl_srv_filter_fd, status);
	if (0 > status)
	{
		close(repl_srv_filter_fd[READ_END]);
		close(repl_srv_filter_fd[WRITE_END]);
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not create pipe for Server->Filter I/O"), ERRNO);
		repl_errno = EREPL_FILTERSTART_PIPE;
		return(FILTERSTART_ERR);
	}
	/*********
	FCNTL3(repl_srv_filter_fd[READ_END], F_SETFL, O_NONBLOCK | O_NDELAY, fcntl_res);
	if (0 > fcntl_res)
	{
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not make Server->Filter I/O non-blocking"), ERRNO);
		return(FILTERSTART_ERR);
	}
	*********/
	/****************
	FCNTL3(repl_srv_filter_fd[WRITE_END], F_SETFL, O_NONBLOCK | O_NDELAY, fcntl_res);
	if (0 > fcntl_res)
	{
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not make Server->Filter I/O non-blocking"), ERRNO);
		return(FILTERSTART_ERR);
	}
	*****************/
	if (NULL == (repl_srv_filter_write_fp = FDOPEN(repl_srv_filter_fd[WRITE_END], "w")))
		GTMASSERT;
	if (0 != SETVBUF(repl_srv_filter_write_fp, NULL, _IOLBF, 0)) /* Make the output of the server line buffered */
		GTMASSERT;
	if (NULL == (repl_srv_filter_read_fp = FDOPEN(repl_srv_filter_fd[READ_END], "r")))
		GTMASSERT;
	if (0 != SETVBUF(repl_srv_filter_read_fp, NULL, _IOLBF, 0)) /* Make the input to the filter line buffered */
		GTMASSERT;
	/* For Filter -> Server */
	OPEN_PIPE(repl_filter_srv_fd, status);
	if (0 > status)
	{
		close(repl_srv_filter_fd[READ_END]);
		close(repl_srv_filter_fd[WRITE_END]);
		close(repl_filter_srv_fd[READ_END]);
		close(repl_filter_srv_fd[WRITE_END]);
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not create pipe for Server->Filter I/O"), ERRNO);
		repl_errno = EREPL_FILTERSTART_PIPE;
		return(FILTERSTART_ERR);
	}
	/***************
	FCNTL3(repl_filter_srv_fd[READ_END], F_SETFL, O_NONBLOCK | O_NDELAY, fcntl_res);
	if (0 > fcntl_res)
	{
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not make Filter->Server I/O non-blocking"), ERRNO);
		return(FILTERSTART_ERR);
	}
	***************/
	/****************
	FCNTL3(repl_filter_srv_fd[WRITE_END], F_SETFL, O_NONBLOCK | O_NDELAY, fcntl_res);
	if (0 > fcntl_res)
	{
		gtm_putmsg(VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not make Filter->Server I/O non-blocking"), ERRNO);
		return(FILTERSTART_ERR);
	}
	*****************/
	if (NULL == (repl_filter_srv_write_fp = FDOPEN(repl_filter_srv_fd[WRITE_END], "w")))
		GTMASSERT;
	if (0 != SETVBUF(repl_filter_srv_write_fp, NULL, _IOLBF, 0)) /* Make the output of the filter line buffered */
		GTMASSERT;
	if (NULL == (repl_filter_srv_read_fp = FDOPEN(repl_filter_srv_fd[READ_END], "r")))
		GTMASSERT;
	if (0 != SETVBUF(repl_filter_srv_read_fp, NULL, _IOLBF, 0)) /* Make the input to the server line buffered */
		GTMASSERT;
	/* Parse the filter_cmd */
	repl_log(stdout, FALSE, TRUE, "Filter command is %s\n", filter_cmd);
	strcpy(cmd, filter_cmd);
	if (NULL == (arg_ptr = strtok(cmd, FILTER_CMD_ARG_DELIM_TOKENS)))
	{
		close(repl_srv_filter_fd[READ_END]);
		close(repl_filter_srv_fd[WRITE_END]);
		gtm_putmsg(VARLSTCNT(6) ERR_REPLFILTER, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Null filter command specified"));
		repl_errno = EREPL_FILTERSTART_NULLCMD;
		return(FILTERSTART_ERR);
	}
	argv[0] = arg_ptr;
	for (argc = 1; NULL != (arg_ptr = strtok(NULL, FILTER_CMD_ARG_DELIM_TOKENS)); argc++)
		argv[argc] = arg_ptr;
	argv[argc] = NULL;
	REPL_DPRINT2("Arg %d is NULL\n", argc);
#ifdef REPL_DEBUG
	{
		int index;
		for (index = 0; argv[index]; index++)
		{
			REPL_DPRINT3("Filter Arg %d : %s\n", index, argv[index]);
		}
		REPL_DPRINT2("Filter argc %d\n", index);
	}
#endif
	if (0 < (repl_filter_pid = UNIX_ONLY(fork)VMS_ONLY(vfork)()))
	{
		/* Server */
		UNIX_ONLY(
			close(repl_srv_filter_fd[READ_END]);
			close(repl_filter_srv_fd[WRITE_END]);
		)
		memset((char *)&null_jnlrec, 0, NULL_RECLEN);
		null_jnlrec.prefix.jrec_type = JRT_NULL;
		null_jnlrec.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		null_jnlrec.prefix.forwptr = null_jnlrec.suffix.backptr = NULL_RECLEN;
		assert(NULL == extr_rec);
		jnl_extr_init();
		extr_rec = malloc(ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE));
		assert(MAX_EXTRACT_BUFSIZ > ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE));
		extract_buff = malloc(MAX_EXTRACT_BUFSIZ);
		return(SS_NORMAL);
	}
	if (0 == repl_filter_pid)
	{
		/* Filter */
		UNIX_ONLY(
			close(repl_srv_filter_fd[WRITE_END]);
			close(repl_filter_srv_fd[READ_END]);
			/* Make the stdin/stdout of the filter line buffered */
			if (0 != SETVBUF(stdin, NULL, _IOLBF, 0))
				GTMASSERT;
			if (0 != SETVBUF(stdout, NULL, _IOLBF, 0))
				GTMASSERT;
			DUP2(repl_srv_filter_fd[READ_END], 0, status);
			if (0 > status)
				GTMASSERT;
			DUP2(repl_filter_srv_fd[WRITE_END], 1, status);
			if (0 > status)
				GTMASSERT;
		)
		VMS_ONLY(decc$set_child_standard_streams(repl_srv_filter_fd[READ_END], repl_filter_srv_fd[WRITE_END], -1));
		/* Start the filter */
		if (0 > EXECV(argv[0], argv))
		{
			close(repl_srv_filter_fd[READ_END]);
			close(repl_filter_srv_fd[WRITE_END]);
			VMS_ONLY(
				/* For vfork(), there is no real child process. So, both ends of both the pipes have to be closed */
				close(repl_srv_filter_fd[WRITE_END]);
				close(repl_filter_srv_fd[READ_END]);
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
}

static int repl_filter_send(seq_num tr_num, unsigned char *tr, int tr_len)
{
	/* Send the transaction tr_num in buffer tr of len tr_len to the filter */
	int	extr_len, send_len, sent_len;
	char	first_rectype, *send_ptr, *extr_end;

	if (QWNE(tr_num, seq_num_zero))
	{
		first_rectype = ((jnl_record *)tr)->prefix.jrec_type;
		is_nontp = !IS_FENCED(first_rectype);
		is_null = (JRT_NULL == first_rectype);
		save_jnl_seqno = GET_REPL_JNL_SEQNO(tr);
		if (NULL == (extr_end = jnl2extcvt((jnl_record *)tr, tr_len, extract_buff)))
			GTMASSERT;
		extr_len = extr_end - extract_buff;
	} else
	{
		is_nontp = TRUE;
		is_null = FALSE;
		strcpy(extract_buff, FILTER_EOT);
		extr_len = strlen(FILTER_EOT);
	}
#ifdef REPL_DEBUG
	if (QWNE(tr_num, seq_num_zero))
	{
		REPL_DPRINT3("Extract for tr %ld : %s", tr_num, extract_buff);
	} else
	{
		REPL_DPRINT1("Sending FILTER_EOT\n");
	}
#endif
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

static int repl_filter_recv(seq_num tr_num, unsigned char *tr, int *tr_len)
{
	/* Receive the transaction tr_num into buffer tr. Return the length of the transaction received in tr_len */
	int		firstrec_len, tcom_len, rec_cnt, extr_len, extr_reclen;
	unsigned char	seq_num_str[32], *seq_num_ptr;
	char		*extr_ptr, *tr_end, *fgets_res;

	/* This routine should do non-blocking read() instead of fgets (will wait till \n is found in input) to detect failure
	 * of filter. When more input is expected, and there is none available, server should check if the filter is alive. */
	/* wait_for_filter_input(); */

	assert(NULL != extr_rec);
	/* First record should be TSTART or NULL */
	FGETS(extr_rec, ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE), repl_filter_srv_read_fp, fgets_res);
	REPL_DPRINT3("Filter output for "INT8_FMT" : %s", INT8_PRINT(tr_num), extr_rec);
	if (!('0' == extr_rec[0] && ('8' == extr_rec[1] || '0' == extr_rec[1])))
		return (repl_errno = EREPL_FILTERBADCONV);
	firstrec_len = strlen(extr_rec);
	memcpy(extract_buff, extr_rec, firstrec_len + 1); /* + 1 to include the terminating '\0' */
	extr_len = firstrec_len;
	rec_cnt = 0;
	if (!is_null && ('0' != extr_rec[0] || '0' != extr_rec[1]))
	{
		while ('0' != extr_rec[0] || '9' != extr_rec[1])
		{
			FGETS(extr_rec, ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE), repl_filter_srv_read_fp, fgets_res);
			REPL_DPRINT2("%s", extr_rec);
			extr_reclen = strlen(extr_rec);
			memcpy(extract_buff + extr_len, extr_rec, extr_reclen);
			extr_len += extr_reclen;
			rec_cnt++;
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
				/* Eliminate the dummy TSTART */
				extr_ptr = extract_buff + firstrec_len;
				extr_len -= firstrec_len;
				/* Eliminate the dummy TCOMMIT */
				/* extr_ptr[extr_len - tcom_len] = '\0'; ??? */
				extr_len -= tcom_len;
			}
		}
		extr_ptr[extr_len] = '\0';	/* For safety */
		if (NULL == (tr_end = ext2jnlcvt(extr_ptr, extr_len, (jnl_record *)tr)))
			return (repl_errno = EREPL_FILTERBADCONV);
		*tr_len = tr_end - (char *)&tr[0];
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
	int repl_filter_send(), repl_filter_recv(), status;

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
	close(repl_srv_filter_fd[WRITE_END]);
	close(repl_filter_srv_fd[READ_END]);
	repl_srv_filter_fd[READ_END] = repl_filter_srv_fd[READ_END] = repl_srv_filter_fd[WRITE_END] =
		repl_filter_srv_fd[WRITE_END] = -1;
	repl_log(stdout, TRUE, TRUE, "Waiting for Filter to Stop\n");
	WAITPID(repl_filter_pid, &filter_exit_status, 0, waitpid_res); /* Release the defunct filter */
	repl_log(stdout, TRUE, TRUE, "Filter Stopped\n");
	return (SS_NORMAL);
}

void repl_filter_error(seq_num filter_seqno, int why)
{
	unsigned char	seq_num_str[32], *seq_num_ptr;

	error_def(ERR_FILTERNOTALIVE);
	error_def(ERR_FILTERCOMM);
	error_def(ERR_FILTERBADCONV);

	repl_log(stderr, TRUE, TRUE, "Stopping filter due to error\n");
	repl_stop_filter();
	seq_num_ptr = i2ascl(seq_num_str, filter_seqno);
	switch (repl_errno)
	{
		case EREPL_FILTERNOTALIVE :
			rts_error(VARLSTCNT(4) ERR_FILTERNOTALIVE, 2, seq_num_ptr - seq_num_str, seq_num_str);
			break;

		case EREPL_FILTERSEND :
			rts_error(VARLSTCNT(5) ERR_FILTERCOMM, 2, seq_num_ptr - seq_num_str, seq_num_str, why);
			break;

		case EREPL_FILTERBADCONV :
			rts_error(VARLSTCNT(4) ERR_FILTERBADCONV, 2, seq_num_ptr - seq_num_str, seq_num_str);
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

	/* On VMS, no customer has a pre V4.3-001 version running replication in production.
	 * On Unix, effective V4.4-002, we stopped supporting dual site config with V4.1 versions.
	 * Customers who want to run replication to upgrade to V4.3-001 (on VMS), V4.2 (on Unix).
	 * We don't want to write/maintain internal filters to support rolling upgrades b/n pre
	 * V4.3-001 (VMS), V4.2 (Unix) and contemporary releases.
	 * Vinaya May 08, 2003 */
	UNIX_ONLY(assert(JNL_VER_EARLIEST_REPL <= remote_jnl_ver);) /* remote must be V4.2+ */
	VMS_ONLY(assert(V13_JNL_VER <= remote_jnl_ver);) /* remote must be V4.3+ */
	if (VMS_ONLY(V13_JNL_VER) UNIX_ONLY(JNL_VER_EARLIEST_REPL) > remote_jnl_ver)
		rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Dual site configuration not supported between these two GT.M versions"));
}
