/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

GBLDEF uchar_ptr_t	repl_filter_buff = NULL;
GBLDEF unsigned int	repl_filter_bufsiz = 0;
GBLDEF unsigned char	jnl_ver, remote_jnl_ver;

GBLDEF intlfltr_t repl_internal_filter[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1][JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	/* This should be a square matrix. If you add a row, make sure you add a column too.	            */
		/*  07	       08,        09,        10,        11,          12           13	     14     */
	/* 07 */{IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,   IF_07TO11,   IF_07TO12,   IF_07TO12,  IF_07TO12},
	/* 08 */{IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,	  IF_NONE,     IF_NONE  ,  IF_NONE  },
	/* 09 */{IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,	  IF_NONE,     IF_NONE  ,  IF_NONE  },
	/* 10 */{IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,	  IF_NONE,     IF_NONE  ,  IF_NONE  },
	/* 11 */{IF_11TO07, IF_NONE,   IF_NONE,   IF_NONE,   IF_NONE,	  IF_11TO12,   IF_11TO12,  IF_11TO12},
	/* 12 */{IF_12TO07, IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO11,   IF_NONE,     IF_NONE  ,  IF_NONE  },
	/* 13 */{IF_12TO07, IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO11,   IF_NONE,     IF_NONE  ,  IF_NONE  },
	/* 14 */{IF_12TO07, IF_NONE,   IF_NONE,   IF_NONE,   IF_12TO11,   IF_NONE,     IF_NONE  ,  IF_NONE  },
};
GBLDEF unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLDEF unsigned char	jnl_source_rectype, jnl_dest_maxrectype;
GBLREF seq_num		seq_num_zero, seq_num_one;
GBLREF int4		gv_keysize;
GBLREF gv_key  		*gv_currkey; /* for jnl_extr_init() */
GBLREF bool    		transform; /* for jnl_extr_init() */

static	pid_t	repl_filter_pid = -1;
static int 	repl_srv_filter_fd[2] = {-1, -1}, repl_filter_srv_fd[2] = {-1, -1};
static FILE 	*repl_srv_filter_write_fp = NULL, *repl_srv_filter_read_fp = NULL, *repl_filter_srv_write_fp = NULL,
		*repl_filter_srv_read_fp = NULL;
static char 	*extract_buff;
static char	extr_rec[MAX_EXTRACT_RECLEN];
static char	tcom_rec[MAX_EXTRACT_RECLEN];
static int	tcom_rec_len;
static int	tcom_pre_jnlseqno;
static int	tcom_post_jnlseqno;

static struct
{
	jrec_prefix		prefix;
	struct_jrec_null	null_rec;
	jrec_suffix		suffix;
} null_jnlrec;

static struct
{
	jrec_prefix		prefix;
	struct_jrec_tcom	tcom_rec;
	jrec_suffix		suffix;
} tcommit_jnlrec;

static int		tcommit_jnlrec_len, null_jnlrec_len;
static seq_num		save_jnl_seqno;
static boolean_t	is_nontp, is_null;
VMS_ONLY(int decc$set_child_standard_streams(int, int, int);)

void jnl_extr_init(void)
{
	/* Should be a non-filter related function. But for now,... Needs GBLREFs gv_currkey and transform */
	transform = FALSE;      /* to avoid the assert in mval2subsc() */
	gv_currkey = (gv_key *)malloc(sizeof(gv_key) + gv_keysize);
	gv_currkey->prev = gv_currkey->end = gv_currkey->top = gv_keysize;
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
	for (argc = 1;
	     NULL != (arg_ptr = strtok(NULL, FILTER_CMD_ARG_DELIM_TOKENS));
	     argc++)
	{
		argv[argc] = arg_ptr;
	}
	argv[argc] = NULL;
	REPL_DPRINT2("Arg %d is NULL\n", argc);
#ifdef REPL_DEBUG
	{
		int index;
		for (index = 0; argv[index]; index++)
		{
			REPL_DPRINT4("Filter Arg %d : %s Len : %d\n", index, STR_AND_LEN(argv[index])));
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
		null_jnlrec_len = sizeof(null_jnlrec);
		memset((char *)&null_jnlrec, 0, null_jnlrec_len);
		null_jnlrec.prefix.jrec_type = JRT_NULL;
#ifdef REPL_FILTER_SENDS_INCMPL_TCOMMIT
		tcommit_jnlrec_len = sizeof(tcommit_jnlrec);
		memset((char *)&tcommit_jnlrec, 0, tcommit_jnlrec_len);
		tcommit_jnlrec.prefix.jrec_type = JRT_TCOM;
		tcommit_jnlrec.tcom_rec.participants = 1;
		/* Convert tcommit_jnlrec into extr format and place it in tcom_rec. Place the length of tcom_rec in tcom_rec_len */
		JNL_REC2EXTR_REC((jnl_record *)&tcommit_jnlrec, tcommit_jnlrec_len, tcom_rec);
		tcom_rec_len = EXTRACT_REC_LENGTH(tcom_rec);
		/* Parse tcom_rec to get the offset where jnl_seqno field is */
		for (delim_count = 1, delim_p = &tcom_rec[0];
		     delim_count < TCOM_EXTR_JNLSEQNO_FIELDNUM;
		     delim_count++)
		{
			for (; *delim_p++ != JNL_EXTRACT_DELIM; );
		}
		tcom_pre_jnlseqno = delim_p - &tcom_rec[0] - 1;
		for (; *delim_p++ != JNL_EXTRACT_DELIM; );
		tcom_post_jnlseqno = delim_p - &tcom_rec[0] - 1;
#endif
		jnl_extr_init();
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
		is_nontp = (JRT_SET == (first_rectype = REF_CHAR(&((jnl_record *)tr)->jrec_type))
				|| JRT_KILL == first_rectype || JRT_ZKILL == first_rectype);
		is_null = (JRT_NULL == first_rectype);
		QWASSIGN(save_jnl_seqno, get_jnl_seqno((jnl_record *)tr));
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

	/* First record should be TSTART or NULL */
	FGETS(extr_rec, MAX_EXTRACT_RECLEN, repl_filter_srv_read_fp, fgets_res);
	REPL_DPRINT3("Filter output for "INT8_FMT" : %s", INT8_PRINT(tr_num), extr_rec);
	if (!('0' == extr_rec[0] && ('8' == extr_rec[1] || '0' == extr_rec[1])))
		return (repl_errno = EREPL_FILTERBADCONV);
	firstrec_len = strlen(extr_rec);
	strcpy(extract_buff, extr_rec);
	extr_len = firstrec_len;
	rec_cnt = 0;
	if (!is_null && ('0' != extr_rec[0] || '0' != extr_rec[1]))
	{
		while ('0' != extr_rec[0] || '9' != extr_rec[1])
		{
			FGETS(extr_rec, MAX_EXTRACT_RECLEN, repl_filter_srv_read_fp, fgets_res);
			REPL_DPRINT2("%s", extr_rec);
			extr_reclen = strlen(extr_rec);
			strcpy(extract_buff + extr_len, extr_rec);
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
			}
#ifndef REPL_FILTER_SENDS_INCMPL_TCOMMIT
			if (1 == rec_cnt)
			{
				/* Eliminate the dummy TCOMMIT */
				extr_ptr[extr_len - tcom_len] = '\0';
				extr_len -= tcom_len;
			}
#else
			/* Eliminate the dummy TCOMMIT */
			extr_ptr[extr_len - tcom_len] = '\0';
			extr_len -= tcom_len;
			if (1 < rec_cnt)
			{
				/* A single SET or a KILL has been transformed into multiple SETs/KILLs. Create the appropriate
				 * TCOMMIT record */
				memcpy(extr_ptr + extr_len, tcom_rec, tcom_pre_jnlseqno);
				extr_len += tcom_pre_jnlseqno;
				seq_num_ptr = i2ascl(seq_num_str, save_jnl_seqno);
				memcpy(extr_ptr + extr_len, seq_num_str, seq_num_ptr - &seq_num_str[0]);
				extr_len += (seq_num_ptr - &seq_num_str[0]);
				memcpy(extr_ptr + extr_len, tcom_rec + tcom_post_jnlseqno, tcom_rec_len - tcom_post_jnlseqno);
				extr_len += (tcom_rec_len - tcom_post_jnlseqno);
			}
#endif
		}
		extr_ptr[extr_len] = '\0'; /* For safety */
		if (NULL == (tr_end = ext2jnlcvt(extr_ptr, extr_len, (jnl_record *)tr)))
			return (repl_errno = EREPL_FILTERBADCONV);
		*tr_len = tr_end - (char *)&tr[0];
		if (!is_nontp || 1 < rec_cnt)
		{
			/* Fill in the rec_seqno field in TCOM */
			((struct_jrec_tcom *)(tr + *tr_len - JREC_SUFFIX_SIZE - sizeof(struct_jrec_tcom)))->rec_seqno = rec_cnt;
#ifndef REPL_FILTER_SENDS_INCMPL_TCOMMIT
			if (is_nontp)
			{
				/* Fill in the jnl_seqno in TCOM */
				QWASSIGN(((struct_jrec_tcom *)(tr + *tr_len - JREC_SUFFIX_SIZE -
							       sizeof(struct_jrec_tcom)))->jnl_seqno, save_jnl_seqno);
			}
#endif
		}
	} else /* 0 == rec_cnt */
	{
		/* Transaction filtered out, put a JRT_NULL record */
		QWASSIGN(null_jnlrec.null_rec.jnl_seqno, save_jnl_seqno);
		memcpy(tr, (char *)&null_jnlrec, null_jnlrec_len);
		*tr_len = null_jnlrec_len;
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
