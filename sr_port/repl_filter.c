/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_select.h"
#include "gtm_ipc.h"

#include <sys/mman.h>
#include <sys/shm.h>
#include <stddef.h>
#include <errno.h>
#include <sys/wait.h>

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
#include "collseq.h"
#include "jnl_typedef.h"
#include "gv_trigger_common.h" /* for HASHT* macros */
#include "replgbl.h"
#include "gtm_c_stack_trace.h"
#include "fork_init.h"
#include "wbox_test_init.h"
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
#include <sys/poll.h>
#endif
#ifdef GTM_TRIGGER
#include "trigger.h"
#endif

/* Do not apply null subscript transformations to LGTRIG and ZTWORM type records */
#define NULLSUBSC_TRANSFORM_IF_NEEDED(RECTYPE, PTR)					\
{											\
	int			keylen;							\
	uchar_ptr_t		lclptr;							\
	DCL_THREADGBL_ACCESS;								\
											\
	SETUP_THREADGBL_ACCESS;								\
	if (!IS_ZTWORM(rectype) && !IS_LGTRIG(rectype) && REMOTE_NULL_SUBS_XFORM)	\
	{										\
		assert(SIZEOF(jnl_str_len_t) == SIZEOF(uint4));				\
		keylen = *((jnl_str_len_t *)(PTR));					\
		keylen &= 0xFFFFFF;	/* to remove 8-bit nodeflags if any */		\
		lclptr = PTR + SIZEOF(jnl_str_len_t);					\
		if (STDNULL_TO_GTMNULL_COLL == REMOTE_NULL_SUBS_XFORM)			\
		{									\
			STD2GTMNULLCOLL(lclptr, keylen);				\
		} else									\
		{									\
			GTM2STDNULLCOLL(lclptr, keylen);				\
		}									\
	}										\
}											\

#define BREAK_IF_BADREC(RECLEN, STATUS)				\
{								\
	if (0 == RECLEN)					\
	{							\
		repl_errno = EREPL_INTLFILTER_BADREC;		\
		assert(FALSE);					\
		STATUS = -1;					\
		break;						\
	}							\
}

#define BREAK_IF_INCMPLREC(RECLEN, JLEN, STATUS)		\
{								\
	if (RECLEN > JLEN)					\
	{							\
		repl_errno = EREPL_INTLFILTER_INCMPLREC;	\
		assert(FALSE);					\
		STATUS = -1;					\
		break;						\
	}							\
}

#define BREAK_IF_NOSPC(REQSPACE, CONV_BUFSIZ, STATUS)		\
{								\
	if ((REQSPACE) > CONV_BUFSIZ)				\
	{							\
		repl_errno = EREPL_INTLFILTER_NOSPC;		\
		STATUS = -1;					\
		break;						\
	}							\
}

#ifdef DEBUG
#define DBG_CHECK_IF_CONVBUFF_VALID(CONV_BUFF, CONV_BUFFLEN)									\
{																\
	uint4			clen, tcom_num = 0, tupd_num = 0, num_participants;						\
	jrec_prefix		*prefix;											\
	enum jnl_record_type	rectype;											\
	uchar_ptr_t		temp_cb, lastrec_ptr;										\
																\
	clen = CONV_BUFFLEN;													\
	prefix = (jrec_prefix *)(CONV_BUFF);											\
	rectype = prefix->jrec_type;												\
	if (clen == prefix->forwptr)												\
	{	/* There is only ONE record in the conversion buffer. Make sure it is either JRT_SET/JRT_KILL/JRT_NULL */	\
		assert((JRT_SET == rectype) || (JRT_KILL == rectype) || (JRT_ZKILL == rectype) || (JRT_NULL == rectype));	\
	} else	/* We have a TP transaction buffer */										\
	{															\
		assert(IS_TUPD(rectype));											\
		lastrec_ptr = (CONV_BUFF + clen - SIZEOF(struct_jrec_tcom));							\
		prefix = (jrec_prefix *)(lastrec_ptr);										\
		assert(JRT_TCOM == prefix->jrec_type); /* final record MUST be a TCOM record */					\
		num_participants = ((struct_jrec_tcom *)(lastrec_ptr))->num_participants;					\
		assert(0 < num_participants);											\
		temp_cb = CONV_BUFF;												\
		while (JREC_PREFIX_SIZE <= clen)										\
		{														\
			assert(0 == ((UINTPTR_T)temp_cb % JNL_REC_START_BNDRY));						\
			prefix = (jrec_prefix *)(temp_cb);									\
			rectype = prefix->jrec_type;										\
			if (IS_TUPD(rectype))											\
				tupd_num++;											\
			else if (JRT_TCOM == rectype)										\
				tcom_num++;											\
			else													\
				assert(IS_UUPD(rectype));									\
			assert(prefix->forwptr == REC_LEN_FROM_SUFFIX(temp_cb, prefix->forwptr));				\
			clen -= prefix->forwptr;										\
			temp_cb += prefix->forwptr;										\
		}														\
		assert(tupd_num == tcom_num); /* We better have balanced TSTART and TCOM */					\
		assert(tcom_num == num_participants); /* The num_participants field in the TCOM record better be reliable */	\
	}															\
}
#else
#define DBG_CHECK_IF_CONVBUFF_VALID(CONV_BUFF, CONV_BUFFLEN)
#endif

/* Initialize a V44 format jrec_suffix structure in the conversion buffer */
#define	INITIALIZE_V44_JREC_SUFFIX(cstart, cb, jstart, jb, conv_reclen)		\
{										\
	jrec_suffix	*suffix_ptr;						\
										\
	suffix_ptr = (jrec_suffix *)cb;						\
	suffix_ptr->backptr = conv_reclen;					\
	suffix_ptr->suffix_code = JNL_REC_SUFFIX_CODE;				\
	cb += JREC_SUFFIX_SIZE;							\
	jb += JREC_SUFFIX_SIZE;							\
}

#define INITIALIZE_V44_NULL_RECORD(PREFIX, CB, SEQNO, STRM_SEQNO)		\
{										\
	jrec_suffix		*suffix;					\
										\
	assert((void *)PREFIX == (void *)CB);					\
	(PREFIX)->jrec_type = JRT_NULL;						\
	(PREFIX)->forwptr = V44_NULL_RECLEN;					\
	/* pini_addr, time, checksum and tn fields of "jrec_prefix" are not	\
	 * used by update process so don't bother initializing them.		\
	 */									\
	CB += JREC_PREFIX_SIZE;							\
	/* Initialize "jnl_seqno" */						\
	*(seq_num *)(CB) = SEQNO;						\
	CB += SIZEOF(seq_num);							\
	/* Initialize "strm_seqno" */						\
	assert(IS_VALID_STRM_SEQNO(STRM_SEQNO));				\
	*(seq_num *)(CB) = STRM_SEQNO;						\
	CB += SIZEOF(seq_num);							\
	/* Set "salvaged" bit */						\
	((struct_jrec_null *)PREFIX)->bitmask.salvaged = FALSE;			\
	/* Skip the 31-bit filler */						\
	CB += SIZEOF(uint4);							\
	/* Initialize the suffix */						\
	suffix = (jrec_suffix *)(CB);						\
	suffix->backptr = V44_NULL_RECLEN;					\
	suffix->suffix_code = JNL_REC_SUFFIX_CODE;				\
	CB += JREC_SUFFIX_SIZE;							\
}

#define GET_JREC_UPD_TYPE(mumps_node_ptr, trigupd_type)			\
{									\
	jnl_string	*keystr;					\
									\
	keystr = (jnl_string *)(mumps_node_ptr);			\
	trigupd_type = NO_TRIG_JREC;					\
	if (IS_GVKEY_HASHT_GBLNAME(keystr->length, keystr->text))	\
		trigupd_type = HASHT_JREC;				\
	else if ((keystr->nodeflags & JS_NOT_REPLICATED_MASK))		\
		trigupd_type = NON_REPLIC_JREC_TRIG;			\
}

#define MORE_TO_TRANSFER -99
#define DUMMY_TCOMMIT_LENGTH 3	/* A dummy commit is 09\n */

#define REPL_FILTER_HALF_TIMEOUT	((ydb_repl_filter_timeout * (uint8)NANOSECS_IN_SEC) / 2)

/* repl_filter_recv receive state */

enum
{
	FIRST_RECV = 1,	/* indicates first time repl_filter_recv is called or it failed with MORE_TO_TRANSFER the previous time */
	FIRST_RECV_COMPLETE,	/* got a successful return from repl_filter_recv_line */
	NEED_TCOMMIT		/* got a tstart for the first read so get another line looking for tcommit */
};

enum
{
	NO_TRIG_JREC = 0,	/* Neither #t global nor triggered update nor an update that should NOT be replicated */
	HASHT_JREC,		/* #t global found in the journal record */
	NON_REPLIC_JREC_TRIG	/* This update was done inside of a trigger */
};

GBLDEF	intlfltr_t repl_filter_cur2old[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	IF_44TO22,	/* Convert from jnl format V44 to V22) */
	IF_44TO22,	/* Convert from jnl format V44 to V23) */
	IF_44TO24,	/* Convert from jnl format V44 to V24) */
	IF_44TO24,	/* Convert from jnl format V44 to V25) */
	IF_44TO24,	/* Convert from jnl format V44 to V26) */
	IF_44TO24,	/* Convert from jnl format V44 to V27) */
	IF_INVALID,	/* Convert from jnl format V44 to V28). IF_INVALID will be filled in when GT.M bumps jnl format to V28. */
	IF_INVALID,	/* Convert from jnl format V44 to V29). IF_INVALID will be filled in when GT.M bumps jnl format to V29. */
	IF_INVALID,	/* Convert from jnl format V44 to V30). IF_INVALID will be filled in when GT.M bumps jnl format to V30. */
	IF_INVALID,	/* Convert from jnl format V44 to V31). IF_INVALID will be filled in when GT.M bumps jnl format to V31. */
	IF_INVALID,	/* Convert from jnl format V44 to V32). IF_INVALID will be filled in when GT.M bumps jnl format to V32. */
	IF_INVALID,	/* Convert from jnl format V44 to V33). IF_INVALID will be filled in when GT.M bumps jnl format to V33. */
	IF_INVALID,	/* Convert from jnl format V44 to V34). IF_INVALID will be filled in when GT.M bumps jnl format to V34. */
	IF_INVALID,	/* Convert from jnl format V44 to V35). IF_INVALID will be filled in when GT.M bumps jnl format to V35. */
	IF_INVALID,	/* Convert from jnl format V44 to V36). IF_INVALID will be filled in when GT.M bumps jnl format to V36. */
	IF_INVALID,	/* Convert from jnl format V44 to V37). IF_INVALID will be filled in when GT.M bumps jnl format to V37. */
	IF_INVALID,	/* Convert from jnl format V44 to V38). IF_INVALID will be filled in when GT.M bumps jnl format to V38. */
	IF_INVALID,	/* Convert from jnl format V44 to V39). IF_INVALID will be filled in when GT.M bumps jnl format to V39. */
	IF_INVALID,	/* Convert from jnl format V44 to V40). IF_INVALID will be filled in when GT.M bumps jnl format to V40. */
	IF_INVALID,	/* Convert from jnl format V44 to V41). IF_INVALID will be filled in when GT.M bumps jnl format to V41. */
	IF_INVALID,	/* Convert from jnl format V44 to V42). IF_INVALID will be filled in when GT.M bumps jnl format to V42. */
	IF_INVALID,	/* Convert from jnl format V44 to V43). IF_INVALID will be filled in when GT.M bumps jnl format to V43. */
	IF_44TO44,	/* Convert from jnl format V44 to V44) */
};

GBLDEF	intlfltr_t repl_filter_old2cur[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	IF_22TO44,	/* Convert from jnl format V22 to V44) */
	IF_22TO44,	/* Convert from jnl format V23 to V44) */
	IF_24TO44,	/* Convert from jnl format V24 to V44) */
	IF_24TO44,	/* Convert from jnl format V25 to V44) */
	IF_24TO44,	/* Convert from jnl format V26 to V44) */
	IF_24TO44,	/* Convert from jnl format V27 to V44) */
	IF_INVALID,	/* Convert from jnl format V28 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V28. */
	IF_INVALID,	/* Convert from jnl format V29 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V29. */
	IF_INVALID,	/* Convert from jnl format V30 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V30. */
	IF_INVALID,	/* Convert from jnl format V31 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V31. */
	IF_INVALID,	/* Convert from jnl format V32 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V32. */
	IF_INVALID,	/* Convert from jnl format V33 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V33. */
	IF_INVALID,	/* Convert from jnl format V34 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V34. */
	IF_INVALID,	/* Convert from jnl format V35 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V35. */
	IF_INVALID,	/* Convert from jnl format V36 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V36. */
	IF_INVALID,	/* Convert from jnl format V37 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V37. */
	IF_INVALID,	/* Convert from jnl format V38 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V38. */
	IF_INVALID,	/* Convert from jnl format V39 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V39. */
	IF_INVALID,	/* Convert from jnl format V40 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V40. */
	IF_INVALID,	/* Convert from jnl format V41 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V41. */
	IF_INVALID,	/* Convert from jnl format V42 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V42. */
	IF_INVALID,	/* Convert from jnl format V43 to V44). IF_INVALID will be filled in when GT.M bumps jnl format to V43. */
	IF_44TO44,	/* Convert from jnl format V44 to V44) */
};

GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF	int4			gv_keysize;
GBLREF	gv_key			*gv_currkey, *gv_altkey; /* for jnl_extr_init() */
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	boolean_t		is_src_server, is_rcvr_server;
GBLREF	repl_conn_info_t	*this_side, *remote_side;
GBLREF	uint4			process_id;
GBLREF	boolean_t		err_same_as_out;
GBLREF	volatile boolean_t	timer_in_handler;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	int			gtmsource_filter;
GBLREF	int			gtmrecv_filter;
GBLREF	int			ydb_repl_filter_timeout;

LITREF	char			*trigger_subs[];

error_def(ERR_FILTERBADCONV);
error_def(ERR_FILTERCOMM);
error_def(ERR_FILTERNOTALIVE);
error_def(ERR_REPLFILTER);
error_def(ERR_REPLNOXENDIAN);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);
error_def(ERR_FILTERTIMEDOUT);

static	pid_t	repl_filter_pid = -1;
static	int	repl_srv_filter_fd[2] = {FD_INVALID, FD_INVALID};
static	int	repl_filter_srv_fd[2] = {FD_INVALID, FD_INVALID};
static	char	*extract_buff;
static	int	extract_bufsiz;
static	char	*recv_extract_buff;
static	int	recv_extract_bufsiz;
static	char	*extr_rec;
static	int	extr_bufsiz;
static	char	*srv_buff_start, *srv_buff_end, *srv_line_start, *srv_line_end, *srv_read_end;
static	int	recv_state;

static struct_jrec_null	null_jnlrec;

static seq_num		save_jnl_seqno;
static seq_num		save_strm_seqno;
static boolean_t	is_nontp, is_null, select_valid;

void jnl_extr_init(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Should be a non-filter related function. But for now,... Needs GBLREFs gv_currkey and transform */
	TREF(transform) = FALSE;      /* to avoid SIG-11 in "mval2subsc" as it expects gv_target to be set up and we don't set it */
	GVKEYSIZE_INIT_IF_NEEDED;
}

static void repl_filter_close_all_pipes(void)
{
	int close_res;

	if (FD_INVALID != repl_srv_filter_fd[READ_END])
	    F_CLOSE(repl_srv_filter_fd[READ_END], close_res);	/* resets "repl_srv_filter_fd[READ_END]" to FD_INVALID */
	if (FD_INVALID != repl_srv_filter_fd[WRITE_END])
	    F_CLOSE(repl_srv_filter_fd[WRITE_END], close_res); 	/* resets "_CLOSE(repl_srv_filter_fd[WRITE_END]" to FD_INVALID */
	if (FD_INVALID != repl_filter_srv_fd[READ_END])
	    F_CLOSE(repl_filter_srv_fd[READ_END], close_res);	/* resets "repl_filter_srv_fd[READ_END]" to FD_INVALID */
	if (FD_INVALID != repl_filter_srv_fd[WRITE_END])
	    F_CLOSE(repl_filter_srv_fd[WRITE_END], close_res);	/* resets "repl_filter_srv_fd[WRITE_END]" to FD_INVALID */

}

int repl_filter_init(char *filter_cmd)
{
	int		fcntl_res, status, argc, delim_count, close_res;
	char		cmd[4096], *delim_p, *strtokptr;
	char_ptr_t	arg_ptr, argv[MAX_FILTER_ARGS];

	REPL_DPRINT1("Initializing FILTER\n");
	repl_filter_close_all_pipes();
	/* Set up pipes for filter I/O */
	/* For Server -> Filter */
	OPEN_PIPE(repl_srv_filter_fd, status);
	if (0 > status)
	{
		repl_filter_close_all_pipes();
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not create pipe for Server->Filter I/O"), ERRNO);
		repl_errno = EREPL_FILTERSTART_PIPE;
		return FILTERSTART_ERR;
	}
	/* Our stdout is to the filter now, so if stderr and stdout were previously conjoined, they no longer are;
	 * note that the child will inherit this variable until exec is done.
	 */
	err_same_as_out = FALSE;
	/* For Filter -> Server */
	OPEN_PIPE(repl_filter_srv_fd, status);
	if (0 > status)
	{
		repl_filter_close_all_pipes();
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not create pipe for Server->Filter I/O"), ERRNO);
		repl_errno = EREPL_FILTERSTART_PIPE;
		return FILTERSTART_ERR;
	}
	/* Parse the filter_cmd */
	repl_log(stdout, FALSE, TRUE, "Filter command is %s\n", filter_cmd);
	strcpy(cmd, filter_cmd);
	if (NULL == (arg_ptr = STRTOK_R(cmd, FILTER_CMD_ARG_DELIM_TOKENS, &strtokptr)))
	{
		repl_filter_close_all_pipes();
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Null filter command specified"));
		repl_errno = EREPL_FILTERSTART_NULLCMD;
		return FILTERSTART_ERR;
	}
	argv[0] = arg_ptr;
	for (argc = 1; NULL != (arg_ptr = STRTOK_R(NULL, FILTER_CMD_ARG_DELIM_TOKENS, &strtokptr)); argc++)
		argv[argc] = arg_ptr;
	argv[argc] = NULL;
	REPL_DPRINT2("Arg %d is NULL\n", argc);
#	ifdef REPL_DEBUG
	{
		int index;
		for (index = 0; argv[index]; index++)
		{
			REPL_DPRINT3("Filter Arg %d : %s\n", index, argv[index]);
		}
		REPL_DPRINT2("Filter argc %d\n", index);
	}
#	endif
	FORK(repl_filter_pid);
	if (0 < repl_filter_pid)
	{	/* Server */
		F_CLOSE(repl_srv_filter_fd[READ_END], close_res); /* SERVER: WRITE only on server -> filter pipe;
								* also resets "repl_srv_filter_fd[READ_END]" to FD_INVALID */
		F_CLOSE(repl_filter_srv_fd[WRITE_END], close_res); /* SERVER: READ only on filter -> server pipe;
								* also resets "repl_srv_filter_fd[WRITE_END]" to FD_INVALID */
		/* Make sure the write-end of the pipe is set to non-blocking. This will make sure repl_filter_send gets a
		 * EAGAIN error in case the write side of the pipe is full and waiting for the filter process to read it.
		 * In that case, we need to switch to reading to see if the filter process has sent any data to process.
		 * Not setting to O_NONBLOCK will cause us (server) to effectively deadlock (since we will wait indefinitely
		 * for the write to succeed but the pipe is full and the filter process cannot clear the pipe (i.e. read data
		 * from the pipe) until we read data that it has already written to its write end of the pipe.
		 */
		FCNTL3(repl_srv_filter_fd[WRITE_END], F_SETFL, O_NONBLOCK, fcntl_res);
		if (0 > fcntl_res)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("fcntl : could not set non-blocking mode in write side of pipe"), ERRNO);
			repl_errno = EREPL_FILTERSTART_FORK;
			return FILTERSTART_ERR;
		}
		memset((char *)&null_jnlrec, 0, NULL_RECLEN);
		null_jnlrec.prefix.jrec_type = JRT_NULL;
		null_jnlrec.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		null_jnlrec.prefix.forwptr = null_jnlrec.suffix.backptr = NULL_RECLEN;
		null_jnlrec.bitmask.salvaged = FALSE;
		null_jnlrec.bitmask.filler = 0;
		assert(SIZEOF(uint4) == SIZEOF(null_jnlrec.bitmask));
		assert(0 == (*(uint4 *)&null_jnlrec.bitmask));
		assert(SIZEOF(uint4) == SIZEOF(null_jnlrec.bitmask));
		assert(NULL == extr_rec);
		jnl_extr_init();
		extr_bufsiz = (ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE) + 1); /* +1 to accommodate terminating null */
		extr_rec = malloc(extr_bufsiz);
		assert(MAX_ONE_JREC_EXTRACT_BUFSIZ > ZWR_EXP_RATIO(MAX_LOGI_JNL_REC_SIZE));
		/* extract_buff and recv_extract_buff holds N journal records in extract format. So they might need
		 * N * MAX_ONE_JREC_EXTRACT_BUFSIZ space at the most. Start with 2*MAX_ONE_JREC_EXTRACT_BUFSIZ and
		 * expand as needed. Ensure while appending one extract line to this buffer, we always have at least
		 * MAX_ONE_JREC_EXTRACT_BUFSIZ space left.
		 */
		extract_bufsiz = (2 * MAX_ONE_JREC_EXTRACT_BUFSIZ);
		extract_buff = malloc(extract_bufsiz);
		recv_extract_bufsiz = (2 * MAX_ONE_JREC_EXTRACT_BUFSIZ);
		recv_extract_buff = malloc(recv_extract_bufsiz);
		srv_line_start = srv_line_end = srv_read_end = srv_buff_start = malloc(MAX_ONE_JREC_EXTRACT_BUFSIZ);
		srv_buff_end = srv_buff_start + MAX_ONE_JREC_EXTRACT_BUFSIZ;
		return SS_NORMAL;
	}
	if (0 == repl_filter_pid)
	{	/* Filter */
		F_CLOSE(repl_srv_filter_fd[WRITE_END], close_res); /* FILTER: READ only on server -> filter pipe;
							* also resets "repl_srv_filter_fd[WRITE_END]" to FD_INVALID */
		F_CLOSE(repl_filter_srv_fd[READ_END], close_res); /* FILTER: WRITE only on filter -> server pipe;
							* also resets "repl_srv_filter_fd[READ_END]" to FD_INVALID */
		/* Make the server->filter pipe stdin for filter */
		DUP2(repl_srv_filter_fd[READ_END], 0, status);
		assertpro(0 <= status);
		/* Make the filter->server pipe stdout for filter */
		DUP2(repl_filter_srv_fd[WRITE_END], 1, status);
		assertpro(0 <= status);
		/* Start the filter */
		if (0 > EXECV(argv[0], argv))
		{	/* exec error, close all pipe fds */
			repl_filter_close_all_pipes();
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Could not exec filter"), ERRNO);
			repl_errno = EREPL_FILTERSTART_EXEC;
			UNDERSCORE_EXIT(FILTERSTART_ERR);
		}
	} else
	{	/* Error in fork */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLFILTER, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Could not fork filter"), ERRNO);
		repl_errno = EREPL_FILTERSTART_FORK;
		return FILTERSTART_ERR;
	}
	return -1; /* This should never get executed, added to make compiler happy */
}

static int repl_filter_send(seq_num tr_num, unsigned char *tr, int tr_len, boolean_t first_send)
{
	/* Send the transaction tr_num in buffer tr of len tr_len to the filter */
	ssize_t		extr_len, sent_len;
	static ssize_t	send_len, prev_sent_len;
	char		first_rectype, *extr_end;
	char		*send_ptr;

	if (TRUE == first_send)
	{
		if (QWNE(tr_num, seq_num_zero))
		{
			first_rectype = ((jnl_record *)tr)->prefix.jrec_type;
			is_nontp = !IS_FENCED(first_rectype);
			is_null = (JRT_NULL == first_rectype);
			save_jnl_seqno = GET_JNL_SEQNO(tr);
			save_strm_seqno = GET_STRM_SEQNO(tr);
			extr_end = jnl2extcvt((jnl_record *)tr, tr_len, &extract_buff, &extract_bufsiz);
			assertpro(NULL != extr_end);
			extr_len = extr_end - extract_buff;
			assert(extr_len < extract_bufsiz);
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
		send_ptr = extract_buff;
		send_len = extr_len;
		prev_sent_len = 0;
	} else
		send_ptr = extract_buff + prev_sent_len;
	do
	{
		sent_len = write(repl_srv_filter_fd[WRITE_END], send_ptr, send_len);
		if ((0 > sent_len) && (EINTR == errno))
		{
			eintr_handling_check();
			continue;
		}
		HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
		break;
	} while (TRUE);
	if (0 > sent_len)
	{
		if (EAGAIN == errno)
		{	/* write would block, so check for read side of pipe */
			return MORE_TO_TRANSFER;
		}
		repl_errno = (EPIPE == errno) ? EREPL_FILTERNOTALIVE : EREPL_FILTERSEND;
		return (ERRNO);
	}
	/* partial write if we get here, so let's go back and try recv */
	prev_sent_len += sent_len;
	send_len -= sent_len;
	if (send_len)
		return MORE_TO_TRANSFER;
	return SS_NORMAL;
}

STATICFNDEF int repl_filter_recv_line(char *line, int *line_len, int max_line_len, boolean_t send_done)
{ /* buffer input read from repl_filter_srv_fd[READ_END], return one line at a time; line separator is '\n' */

	int		save_errno, orig_heartbeat;
	int		status;
	ssize_t		l_len, r_len, buff_remaining ;
	muextract_type	exttype;
	fd_set		input_fds;
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
	long		poll_timeout;
	unsigned long	poll_nfds;
	struct pollfd	poll_fdlist[1];
#endif
	struct timeval	repl_filter_poll_interval;
	boolean_t	half_timeout_done, timedout;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (; ;)
	{
		for ( ; (srv_line_end < srv_read_end) && ('\n' != *srv_line_end); srv_line_end++)
			;
		if (srv_line_end < srv_read_end) /* newline found */
		{
			l_len = (ssize_t)(srv_line_end - srv_line_start + 1); /* include '\n' in length */
			assertpro((int)l_len <= max_line_len); /* allocated buffer should have been enough for ONE jnlrec */
			memcpy(line, srv_line_start, l_len);
			*line_len = (int4)l_len ;
			srv_line_start = ++srv_line_end; /* move past '\n' for next line */
			assert(srv_line_end <= srv_read_end);
			REPL_EXTRA_DPRINT3("repl_filter: newline found, srv_line_end: 0x%x srv_read_end: 0x%x\n",
						srv_line_end, srv_read_end);
			exttype = (muextract_type)MUEXTRACT_TYPE(line);
			/* First 2 bytes must be in valid journal extract format */
			if ((0 > (signed)exttype) || (MUEXT_MAX_TYPES <= (signed)exttype))
			{
				assert(WBTEST_EXTFILTER_INDUCE_ERROR == ydb_white_box_test_case_number);
				return (repl_errno = EREPL_FILTERBADCONV);
			} else
				return SS_NORMAL;
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
			/* since it is possible that this read could happen twice in this loop (no newline found) then
			 * do a select/poll unless this is the first time and we know the previous select/poll is still valid.
			 */
			if (FALSE == select_valid)
			{
				while(1) /* select/poll until ok to read or timeout */
				{
					repl_filter_poll_interval.tv_sec = 0;
					repl_filter_poll_interval.tv_usec = 1000;
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
					assertpro(FD_SETSIZE > repl_filter_srv_fd[READ_END]);
					FD_ZERO(&input_fds);
					FD_SET(repl_filter_srv_fd[READ_END], &input_fds);
#else
					poll_fdlist[0].fd = repl_filter_srv_fd[READ_END];
					poll_fdlist[0].events = POLLIN;
					poll_nfds = 1;
					poll_timeout = repl_filter_poll_interval.tv_usec / 1000;   /* convert to millisecs */
#endif
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
					status = select(repl_filter_srv_fd[READ_END] + 1, &input_fds, NULL,
							NULL, &repl_filter_poll_interval);
#else
					status = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
#endif
					if (-1 == status)
					{
						if (EINTR == errno) /* ignore interrupt and try again */
						{
							eintr_handling_check();
							continue;
						} else
						{
							HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
							repl_errno = EREPL_FILTERRECV;
							return errno;
						}
					} else
						HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
					if (0 == status) /* timeout */
					{
						return MORE_TO_TRANSFER;
					}
					break;
				}
			}
			/* Before starting the `read', note down the current heartbeat counter. This is used to break from the
			 * read if the filter program takes too long to send records back to us. Do it only if send_done is TRUE
			 * which indicates the end of a mini-transaction or a commit record for an actual transaction.
			 */
			assert(-1 != repl_filter_pid);
			half_timeout_done = FALSE;
			if (send_done)
				TIMEOUT_INIT(timedout, REPL_FILTER_HALF_TIMEOUT);
			do
			{
				r_len = read(repl_filter_srv_fd[READ_END], srv_read_end, buff_remaining);
				if (0 <= r_len)
				{
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
					break;
				}
				save_errno = errno;
				if ((ENOMEM != save_errno) && (EINTR != save_errno))
				{
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
					break;
				}
				if (EINTR == save_errno)
					eintr_handling_check();
				else
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
				/* EINTR/ENOMEM -- check if it's time to take the stack trace. */
				if (send_done)
				{
					if (!timedout)
						continue;
					if (!half_timeout_done)
					{	/* Half-timeout : take C-stack of the filter program. */
						half_timeout_done = TRUE;
						TIMEOUT_DONE(timedout);
						TIMEOUT_INIT(timedout, REPL_FILTER_HALF_TIMEOUT);
						GET_C_STACK_FROM_SCRIPT("FILTERTIMEDOUT_HALF_TIME", process_id, repl_filter_pid, 0);
					}
					assert(half_timeout_done);
					/* GET_C_STACK_FROM_SCRIPT calls gtm_system(BYPASSOK) with interrupts deferred. If the
					 * stack trace takes more than REPL_FILTER_HALF_TIMEOUT seconds, the next timeout
					 * interrupt is deferred until gtm_system(BYPASSOK) returns. At which point timedout is
					 * TRUE and there is no signal received by GT.M to interrupt the blocking read() at
					 * the begining of the loop.  So we handle the timeout now and skip the second stack trace.
					 */
					if (half_timeout_done && timedout)
					{	/* Full-timeout : take C-stack of the filter program. */
						GET_C_STACK_FROM_SCRIPT("FILTERTIMEDOUT_FULL_TIME", process_id, repl_filter_pid, 1);
						TIMEOUT_DONE(timedout);
						return (repl_errno = EREPL_FILTERTIMEDOUT);
					}
					continue;
				}
			} while (TRUE);
			if (send_done)
				TIMEOUT_DONE(timedout);
			if (0 < r_len) /* successful read */
			{
				/* if send is not done then need to do select/poll if we try read again */
				if (FALSE == send_done)
					select_valid = FALSE;
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
		assert(FALSE);
		/* srv_read_end == srv_buff_end => buffer is full but no new-line; since we allocated enough buffer for the largest
		 * possible logical record, this should be a bad conversion from the filter
		 */
		return (repl_errno = EREPL_FILTERBADCONV);
	}
}

STATICFNDEF int repl_filter_recv(seq_num tr_num, unsigned char **tr, int *tr_len, int *tr_bufsize, boolean_t send_done)
{	/* Receive the transaction tr_num into buffer tr. Return the length of the transaction received in tr_len */
	static int	firstrec_len, tcom_len, rec_cnt, extr_len, extr_reclen, unwrap_nontp;
	int		save_errno, status;
	char		*extr_ptr, *tmp;
	unsigned char	*tr_end, *tcom_ptr;

	assert(NULL != extr_rec);

	/* the select/poll need not be done for the first read in processing loop in repl_filter_recv_line() */
	select_valid = TRUE;
	if (FIRST_RECV_COMPLETE > recv_state)
	{
		unwrap_nontp = FALSE; /* If this is TRUE then the filter program made a non-tp a transaction */
		if (SS_NORMAL != (status = repl_filter_recv_line(extr_rec, &firstrec_len, extr_bufsiz, send_done)))
			return status;
		/* if send not done make sure we do a select before reading any more */
		if (FALSE == send_done)
			select_valid = FALSE;
		assert(recv_extract_bufsiz > firstrec_len);	/* assert initial allocation will always fit ONE extract line */
		memcpy(recv_extract_buff, extr_rec, firstrec_len); /* note: includes terminating null */
		extr_reclen = extr_len = firstrec_len;
		rec_cnt = 0;
		REPL_DEBUG_ONLY(extr_rec[extr_reclen] = '\0';)
			REPL_DPRINT6("Filter output for "INT8_FMT" :\nrec_cnt: %d\textr_reclen: %d\textr_len: %d\t%s",
				     INT8_PRINT(tr_num), rec_cnt, extr_reclen, extr_len, extr_rec);
		recv_state = FIRST_RECV_COMPLETE;

		/* if first record is a TSTART and it is a nontp and not null then change recv_state to NEED_TCOMMIT */
		if (is_nontp && !is_null && ('8' == extr_rec[1]))
		{
			is_nontp = FALSE;
			/* Assume it needs to be unwrapped until we decide if additional mini-transactions added */
			unwrap_nontp = TRUE;
			recv_state = NEED_TCOMMIT;
		}
	}
	/* if not NULL record */
	if ((NEED_TCOMMIT == recv_state) || (!is_nontp && !is_null && ('0' != extr_rec[0] || '0' != extr_rec[1])))
	{
		recv_state = NEED_TCOMMIT;
		while ('0' != extr_rec[0] || '9' != extr_rec[1]) /* while not TCOM record */
		{
			if (SS_NORMAL != (status = repl_filter_recv_line(extr_rec, &extr_reclen, extr_bufsiz, send_done)))
				return status;
			/* We don't want a null transaction inside a tp so get rid of it */
			if ('0' == extr_rec[0] && '0' == extr_rec[1])
				continue;
			if ((extr_len + extr_reclen) > recv_extract_bufsiz)
			{	/* Expand recv_extract_buff linearly */
				recv_extract_bufsiz += (2 * MAX_ONE_JREC_EXTRACT_BUFSIZ);
				assertpro(recv_extract_bufsiz > (extr_len + extr_reclen));
				tmp = malloc(recv_extract_bufsiz);
				memcpy(tmp, recv_extract_buff, extr_len);
				free(recv_extract_buff);
				recv_extract_buff = tmp;
			}
			assertpro(recv_extract_bufsiz >= (extr_len + extr_reclen));
			memcpy(recv_extract_buff + extr_len, extr_rec, extr_reclen);
			extr_len += extr_reclen;
			rec_cnt++;
			REPL_DEBUG_ONLY(extr_rec[extr_reclen] = '\0';)
			REPL_DPRINT5("rec_cnt: %d\textr_reclen: %d\textr_len: %d\t%s", rec_cnt, extr_reclen, extr_len, extr_rec);
		}
		tcom_len = extr_reclen;
		rec_cnt--;
	}
	/* If mini-transaction or fenced transaction not filtered out, then convert it and check for correct journal seq number.
	 * If it is filtered out, then take the else clause.
	 */
	if (is_nontp && (('0' != extr_rec[0]) || ('0' != extr_rec[1])))
		rec_cnt = 1;
	if (0 < rec_cnt)
	{
		extr_ptr = recv_extract_buff;
		/* if unwrap_nontp is TRUE and rec_cnt is one then remove the wrapper added by the filter */
		if (TRUE == unwrap_nontp)
		{
			if (1 == rec_cnt)
			{
				extr_ptr = recv_extract_buff + firstrec_len; /* Eliminate the dummy TSTART */
				extr_len -= firstrec_len;
				extr_len -= tcom_len; /* Eliminate the dummy TCOMMIT */
			} else	/* a dummy TCOMMIT not allowed for a created multi-line transaction */
			{
				if (DUMMY_TCOMMIT_LENGTH == tcom_len)
				{
					assert(WBTEST_EXTFILTER_INDUCE_ERROR == ydb_white_box_test_case_number);
					return (repl_errno = EREPL_FILTERBADCONV);
				}
			}
		}
		extr_ptr[extr_len] = '\0'; /* terminate with null for ext2jnlcvt */
		if ((NULL == (tr_end = ext2jnlcvt(extr_ptr, extr_len, tr, tr_bufsize, save_jnl_seqno, save_strm_seqno)))
				|| (save_jnl_seqno != GET_JNL_SEQNO(*tr)) || (save_strm_seqno != GET_STRM_SEQNO(*tr)))
		{
			assert(FALSE);
			return (repl_errno = EREPL_FILTERBADCONV);
		}
		assert((tr_end - *tr) <= *tr_bufsize);
		*tr_len = tr_end - *tr;
		/* TCOM record for non TP converted to TP must have the same seqno as the original non TP record */
		if (TRUE == unwrap_nontp && 1 < rec_cnt)
		{	/* tr_end points past the tcom record so need to back up the length of the tcom record */
			tcom_ptr = tr_end - TCOM_RECLEN;
			if (QWNE(save_jnl_seqno, ((struct_jrec_tcom *)tcom_ptr)->token_seq.jnl_seqno) ||
				QWNE(save_strm_seqno, ((struct_jrec_tcom *)tcom_ptr)->strm_seqno))
			{
				assert(FALSE);
				return (repl_errno = EREPL_FILTERBADCONV);
			}
		}
	} else /* 0 == rec_cnt */
	{	/* Transaction filtered out, put a JRT_NULL record; the prefix.{pini_addr,time,tn} fields are not filled in as they
		 * are not relevant on the secondary
		 */
		QWASSIGN(null_jnlrec.jnl_seqno, save_jnl_seqno);
		QWASSIGN(null_jnlrec.strm_seqno, save_strm_seqno);
		memcpy(*tr, (char *)&null_jnlrec, NULL_RECLEN);
		*tr_len = NULL_RECLEN;
		/* Reset read pointers to avoid the subsequent records from being wrongly interpreted */
		assert(srv_line_end <= srv_read_end);
		assert(srv_line_start <= srv_read_end);
		srv_line_end = srv_line_start = srv_read_end;
	}
	return SS_NORMAL;
}

int repl_filter(seq_num tr_num, unsigned char **tr, int *tr_len, int *tr_bufsize)
{
	int		status;
	boolean_t	try_send = TRUE;
	boolean_t	try_recv = TRUE;
	boolean_t	send_done = FALSE;
	boolean_t	recv_done = FALSE;
	boolean_t	first_send = TRUE;
	fd_set		output_fds;
	fd_set		input_fds;
#ifdef GTM_USE_POLL_FOR_SUBSECOND_SELECT
	long		poll_timeout;
	unsigned long	poll_nfds;
	struct pollfd	poll_fdlist[1];
#endif
	struct timeval	repl_filter_poll_interval;

	assert(*tr_len <= *tr_bufsize);
	recv_state = FIRST_RECV;
	while ((FALSE == send_done) || (FALSE == recv_done))
	{
		if ((FALSE == send_done) && (TRUE == try_send))
		{
			/* don't have to do the select for the first send */
			if (FALSE == first_send)
			{
				repl_filter_poll_interval.tv_sec = 0;
				repl_filter_poll_interval.tv_usec = 1000;
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
				assertpro(FD_SETSIZE > repl_srv_filter_fd[WRITE_END]);
				FD_ZERO(&output_fds);
				FD_SET(repl_srv_filter_fd[WRITE_END], &output_fds);
#else
				poll_fdlist[0].fd = repl_srv_filter_fd[WRITE_END];
				poll_fdlist[0].events = POLLOUT;
				poll_nfds = 1;
				poll_timeout = repl_filter_poll_interval.tv_usec / 1000;   /* convert to millisecs */
#endif
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
				status = select(repl_srv_filter_fd[WRITE_END] + 1, NULL, &output_fds, NULL,
						&repl_filter_poll_interval);
#else
				status = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
#endif
				if (-1 == status)
				{
					if (EINTR == errno) /* ignore interrupt and try again */
					{
						eintr_handling_check();
						continue;
					} else
					{
						HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
						repl_errno = EREPL_FILTERSEND;
						return errno;
					}
				} else
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
				if (0 == status) /* timeout */
				{
					try_recv = TRUE;
					try_send = FALSE;
					continue; /* when select is on receive then try it */
				}
			}
			status = repl_filter_send(tr_num, *tr, *tr_len, first_send);
			first_send = FALSE;
			if (MORE_TO_TRANSFER == status)
			{
				try_recv = TRUE;
				try_send = FALSE;
				continue;
			}
			if (SS_NORMAL != status)
				return (status);
			else
			{	/* send completed successfully */
				send_done = TRUE;
				try_recv = TRUE;
			}
		}
		if ((FALSE == recv_done) && (TRUE == try_recv))
		{	/* if send is not done then do a select first */
			if (FALSE == send_done)
			{
				repl_filter_poll_interval.tv_sec = 0;
				repl_filter_poll_interval.tv_usec = 1000;
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
				assertpro(FD_SETSIZE > repl_filter_srv_fd[READ_END]);
				FD_ZERO(&input_fds);
				FD_SET(repl_filter_srv_fd[READ_END], &input_fds);
#else
				poll_fdlist[0].fd = repl_filter_srv_fd[READ_END];
				poll_fdlist[0].events = POLLIN;
				poll_nfds = 1;
				poll_timeout = repl_filter_poll_interval.tv_usec / 1000;   /* convert to millisecs */
#endif
#ifndef GTM_USE_POLL_FOR_SUBSECOND_SELECT
				status = select(repl_filter_srv_fd[READ_END] + 1, &input_fds, NULL, NULL,
					&repl_filter_poll_interval);
#else
				status = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
#endif
				if (-1 == status)
				{
					if (EINTR == errno) /* ignore interrupt and try again */
					{
						eintr_handling_check();
						continue;
					} else
					{
						HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
						repl_errno = EREPL_FILTERRECV;
						return errno;
					}
				} else
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
				if (0 == status) /* timeout */
				{
					try_send = TRUE;
					try_recv = FALSE;
					continue; /* try select again */
				}
			}

			status = repl_filter_recv(tr_num, tr, tr_len, tr_bufsize, send_done);
			if (MORE_TO_TRANSFER == status)
			{
				if (FALSE == send_done)
				{
					try_send = TRUE;
					try_recv = FALSE;
				}
				continue;
			}
			if (SS_NORMAL != status)
				return (status);
			else
				recv_done = TRUE;
		}
	}
	return SS_NORMAL;
}

int repl_stop_filter(void)
{	/* Send a special record to indicate stop */
	int	filter_exit_status, waitpid_res;

	REPL_DPRINT1("Stopping filter in repl_stop_filter\n");
	repl_filter_send(seq_num_zero, NULL, 0, TRUE);
	repl_filter_close_all_pipes();
	free(extr_rec);
	free(extract_buff);
	free(recv_extract_buff);
	free(srv_buff_start);
	extr_rec = extract_buff = recv_extract_buff = srv_buff_start = NULL;
	repl_log(stdout, TRUE, TRUE, "Waiting for Filter to Stop\n");
	WAITPID(repl_filter_pid, &filter_exit_status, 0, waitpid_res); /* Release the defunct filter */
	repl_log(stdout, TRUE, TRUE, "Filter Stopped\n");
	return (SS_NORMAL);
}

void repl_filter_error(seq_num filter_seqno, int why)
{
	assert(is_src_server || is_rcvr_server);
	if (is_src_server)
		STOP_EXTERNAL_FILTER_IF_NEEDED(gtmsource_filter, gtmsource_log_fp, "REPL_FILTER_ERROR");
	else
		STOP_EXTERNAL_FILTER_IF_NEEDED(gtmrecv_filter, gtmrecv_log_fp, "REPL_FILTER_ERROR");
	switch (repl_errno)
	{
		case EREPL_FILTERNOTALIVE :
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FILTERNOTALIVE, 1, &filter_seqno);
			break;
		case EREPL_FILTERSEND :
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FILTERCOMM, 1, &filter_seqno, why);
			break;
		case EREPL_FILTERBADCONV :
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FILTERBADCONV, 1, &filter_seqno);
			break;
		case EREPL_FILTERRECV :
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FILTERCOMM, 1, &filter_seqno, why);
			break;
		case EREPL_FILTERTIMEDOUT :
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FILTERTIMEDOUT, 1, &filter_seqno);
			break;
		default :
			assertpro(repl_errno != repl_errno);
	}
	return;
}

/* Issue error if the replication cannot continue. Possible reason is Remote side (Primary or Secondary)
 * version (YottaDB or GT.M) is too old to be supported by current YottaDB version.
 */
void repl_check_jnlver_compat(boolean_t same_endianness)
{	/* see comment in repl_filter.h about list of filter-formats, jnl-formats and GT.M versions */

	assert(is_src_server || is_rcvr_server);
	if (JNL_VER_EARLIEST_REPL > REMOTE_JNL_VER)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
			LEN_AND_LIT("Replication not supported between these two GT.M versions"));
}

/* The following code defines the functions that convert one jnl format to another.
 * The only replicated records we expect to see here are *SET* or *KILL* or TCOM or NULL records.
 * These fall under the following 3 structure types each of which is described for the different jnl formats we handle.
 *
 * V22/V23/V24 format
 * -------------------
 *  struct_jrec_upd layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0032 [0x0020]      size = 0008 [0x0008]    ----> strm_seqno
 *	offset = 0040 [0x0028]      size = 0004 [0x0004]    ----> update_num
 *	offset = 0044 [0x002c]      size = 0002 [0x0002]    ----> filler_short
 *	offset = 0046 [0x002e]      size = 0002 [0x0002]    ----> num_participants
 *	offset = 0048 [0x0030]      size = 0008 [0x0008]    ----> mumps_node
 * struct_jrec_tcom layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0032 [0x0020]      size = 0008 [0x0008]    ----> strm_seqno
 *	offset = 0040 [0x0028]      size = 0002 [0x0002]    ----> filler_short
 *	offset = 0042 [0x002a]      size = 0002 [0x0002]    ----> num_participants
 *	offset = 0044 [0x002c]      size = 0008 [0x0008]    ----> jnl_tid
 *	offset = 0052 [0x0034]      size = 0004 [0x0004]    ----> suffix
 * struct_jrec_null layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> jnl_seqno
 *	offset = 0032 [0x0020]      size = 0008 [0x0008]    ----> strm_seqno
 *	offset = 0040 [0x0028]      size = 0004 [0x0004]    ----> filler
 *	offset = 0044 [0x002c]      size = 0004 [0x0004]    ----> suffix
 *
 * V44 format
 * -----------
 * struct_jrec_upd layout is same as V22/V23/V24
 * struct_jrec_tcom layout is same as V22/V23/V24
 * struct_jrec_null layout is as follows (slight different from V22/V23/V24)
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> jnl_seqno
 *	offset = 0032 [0x0020]      size = 0008 [0x0008]    ----> strm_seqno
 *	offset = 0040 [0x0028]      size = 0004 [0x0004]    ----> 1-bit   : salvaged
 *	offset = 0040 [0x0028]      size = 0004 [0x0004]    ----> 31-bits : filler
 *	offset = 0044 [0x002c]      size = 0004 [0x0004]    ----> suffix
 */

/* Convert a transaction from jnl version V22/V23 (V5.5-000 thru V6.1-000) to V44 (r1.24 onwards).
 * (a) If null-subscript collation is different between the primary and the secondary
 * (b) Filter out jnl records that should not be replicated (i.e. updates done inside a trigger or ^#t records).
 * (c) Issue error if ^#t records are found as those are not allowed in	the replication stream from V62001 onwards.
 * (d) If no conversion occurred (out-of-design), EREPL_INTLFILTER_NOCONVERT return error code.
 */
int jnl_v22TOv44(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(is_rcvr_server);
	assert(LOCAL_TRIGGER_SUPPORT);	/* A lot of the below code has been simplified because of this assumption */
	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
		reclen = prefix->forwptr;
		BREAK_IF_BADREC(prefix, status); /* check if we encountered a bad record */
		BREAK_IF_INCMPLREC(reclen, jlen, status); /* check if this record is incomplete */
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V22 >= rectype);
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		conv_reclen = prefix->forwptr ;
		BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
		if (IS_SET_KILL_ZKILL_ZTWORM_ZTRIG(rectype))
		{
			assert((jb == jstart) && (cb == cstart));
			GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
			if (NON_REPLIC_JREC_TRIG == trigupd_type)
			{	/* This is a jnl record that should not be replicated. Filter it out of the transaction. */
				if (IS_TUPD(rectype))
					promote_uupd_to_tupd = TRUE;
				assert((cb == cstart) && (jb == jstart));
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			} else if (HASHT_JREC == trigupd_type)
			{	/* Journal record has a #t global. #t records are no longer allowed to V44, only LGTRIG records are.
				 * But since the source version does not support LGTRIG records, issue error.
				 */
				repl_errno = EREPL_INTLFILTER_PRILESSTHANV62;
				status = -1;
				break;
			}
			memcpy(cb, jb, conv_reclen);
			mumps_node_ptr = cb + FIXED_UPD_RECLEN;
			NULLSUBSC_TRANSFORM_IF_NEEDED(rectype, mumps_node_ptr);
			if (IS_TUPD(rectype))
				tupd_num++;
			else if (IS_UUPD(rectype) && promote_uupd_to_tupd)
			{
				((jrec_prefix *)(cb))->jrec_type--;
				assert(IS_TUPD(((jrec_prefix *)(cb))->jrec_type));
				promote_uupd_to_tupd = FALSE;
				tupd_num++;
			}
		} else if (JRT_TCOM == rectype)
		{
			assert((jb == jstart) && (cb == cstart));
			tcom_num++;
			if (tcom_num > tupd_num)
			{	/* This is a case where all updates to one region in the TP transaction were
				 * inside a trigger which means that region did not count towards tupd_num
				 * in which case we should skip tcom_num too for that region.
				 */
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			((struct_jrec_tcom *)(cb))->num_participants = tupd_num;
		} else
		{
			assert(JRT_NULL == rectype);
			assert((cb == cstart) && (jb == jstart));
			memcpy(cb, jb, conv_reclen);
			((struct_jrec_null *)cb)->bitmask.salvaged = FALSE; /* Set "salvaged" bit in NULL record (new to V44) */
		}
		cb = cb + conv_reclen;
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if (-1 != status)
	{
		if (cb == conv_buff)
		{	/* No conversion happened. Currently this is NOT possible */
			assert(FALSE);
			repl_errno = EREPL_INTLFILTER_NOCONVERT;
			status = -1;
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return status;
}

/* Convert a transaction from jnl version V44 (r1.24 onwards) to V22/V23 (V5.5-000 thru V6.1-000).
 * (a) If null-subscript collation is different between the primary and the secondary
 * (b) Filter out jnl records that should not be replicated (i.e. updates done inside a trigger or ^#t records).
 * (c) If the remote side does NOT support triggers, then skip ^#t/ZTWORM/ZTRIG journal records & reset nodeflags (if set).
 *	Note that V22 did not support LGTRIG records so issue an error.
 * (d) If remote side does support triggers, then skip LGTRIG journal records as they are not known to the older journal format.
 * (e) If the entire transaction consists of skipped records, send a NULL record instead.
 */
int jnl_v44TOv22(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	seq_num			this_upd_seqno, this_strm_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd, receiver_supports_triggers, hasht_seen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	this_upd_seqno = seq_num_zero;
	assert(is_src_server);
	/* Since this filter function is invoked only on the source side, the check for whether the receiver
	 * supports triggers is equal to checking whether the REMOTE side supports triggers.
	 */
	receiver_supports_triggers = REMOTE_TRIGGER_SUPPORT;
	hasht_seen = FALSE;
	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
		reclen = prefix->forwptr;
		BREAK_IF_BADREC(prefix, status); /* check if we encountered a bad record */
		BREAK_IF_INCMPLREC(reclen, jlen, status); /* check if this record is incomplete */
		if (this_upd_seqno == seq_num_zero)
		{
			this_upd_seqno = GET_JNL_SEQNO(jb);
			this_strm_seqno = GET_STRM_SEQNO(jb);
		}
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V44 >= rectype);
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		if (IS_LGTRIG(rectype) || (!receiver_supports_triggers && (IS_ZTRIG(rectype) || IS_ZTWORM(rectype))))
		{	/* $ZTWORMHOLE or ZTRIG jnl record cannot be handled by secondary which does not support triggers
			 * AND LGTRIG jnl record cannot be handled even by a secondary that supports triggers so skip converting.
			 * If this is a TUPD rectype (actually JRT_TZTWORM/JRT_TZTRIG) then the next UUPD has to be promoted to a
			 * TUPD type to account for the balance in TUPD and TCOM records
			 */
			assert((cb == cstart) && (jb == jstart)); /* No conversions yet */
			if (IS_TUPD(rectype))
			{
				assert((JRT_TZTWORM == rectype) || (JRT_TLGTRIG == rectype) || (JRT_TZTRIG == rectype));
				promote_uupd_to_tupd = TRUE;
			}
		} else
		{
			conv_reclen = prefix->forwptr ;
			BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
			if (IS_SET_KILL_ZKILL_ZTWORM_ZTRIG(rectype))
			{
				assert((jb == jstart) && (cb == cstart));
				GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
				if (receiver_supports_triggers)
				{
					if (HASHT_JREC == trigupd_type)
					{	/* Journal record has a #t global. #t records are no longer allowed from V44,
						 * only LGTRIG records are. But since the receiver version does not support
						 * LGTRIG records, issue error.
						 */
						repl_errno = EREPL_INTLFILTER_SECLESSTHANV62;
						status = -1;
						break;
					}
					if (NON_REPLIC_JREC_TRIG == trigupd_type)
					{	/* This is a jnl record that should not be replicated.
						 * Filter it out of the transaction.
						 */
						if (IS_TUPD(rectype))
							promote_uupd_to_tupd = TRUE;
						assert((cb == cstart) && (jb == jstart));
						jb = jb + reclen;
						jlen -= reclen;
						continue;
					}
				} else if (HASHT_JREC == trigupd_type)
				{	/* Journal record has a #t global. #t records are not replicated if the secondary does not
					 * support triggers. However, $ZTRIGGER() usages within TP can cause ^#t records to be
					 * generated in the middle of a TP transaction. Hence skip the ^#t records. However, if this
					 * ^#t record is a TUPD record, then note it down so that we promote the next UUPD record to
					 * a TUPD record.
					 */
					if (IS_TUPD(rectype))
						promote_uupd_to_tupd = TRUE;
					assert((cb == cstart) && (jb == jstart));
					jb = jb + reclen;
					jlen -= reclen;
					hasht_seen = TRUE;
					continue;
				}
				memcpy(cb, jb, conv_reclen);
				mumps_node_ptr = cb + FIXED_UPD_RECLEN;
				if (!receiver_supports_triggers)
					((jnl_string *)mumps_node_ptr)->nodeflags = 0;
				NULLSUBSC_TRANSFORM_IF_NEEDED(rectype, mumps_node_ptr);
				if (IS_TUPD(rectype))
					tupd_num++;
				else if (IS_UUPD(rectype) && promote_uupd_to_tupd)
				{
					((jrec_prefix *)(cb))->jrec_type--;
					assert(IS_TUPD(((jrec_prefix *)(cb))->jrec_type));
					promote_uupd_to_tupd = FALSE;
					tupd_num++;
				}
			} else if (JRT_TCOM == rectype)
			{
				assert((jb == jstart) && (cb == cstart));
				tcom_num++;
				if (tcom_num > tupd_num)
				{	/* This is a case where all updates to one region in the TP transaction were
					 * inside a trigger which means that region did not count towards tupd_num
					 * in which case we should skip tcom_num too for that region.
					 */
					jb = jb + reclen;
					jlen -= reclen;
					continue;
				}
				memcpy(cb, jb, conv_reclen);
				((struct_jrec_tcom *)(cb))->num_participants = tupd_num;
			} else
			{
				assert(JRT_NULL == rectype);
				assert((cb == cstart) && (jb == jstart));
				memcpy(cb, jb, conv_reclen);
				/* "salvaged" bit in JRT_NULL record in V44 is part of "filler" in V22 format so send it as is */
			}
			cb = cb + conv_reclen;
		}
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if (-1 != status)
	{
		if (hasht_seen && !(TREF(replgbl)).trig_replic_suspect_seqno)
			(TREF(replgbl)).trig_replic_suspect_seqno = this_upd_seqno;
		if (cb == conv_buff)
		{	/* No conversion happened. Currently this is possible ONLY if the receiver side does not support triggers
			 * and all the records are trigger related records (^#t/ZTRIG/ZTWORM/LGTRIG).
			 * Need to send NULL record instead.
			 */
			assert(!receiver_supports_triggers);
			prefix = (jrec_prefix *)(cb);
			if (NULL_RECLEN > conv_bufsiz)
			{
				repl_errno = EREPL_INTLFILTER_NOSPC;
				status = -1;
			} else
			{
				INITIALIZE_V44_NULL_RECORD(prefix, cb, this_upd_seqno, this_strm_seqno); /* Note : cb is updated */
				/* Note that the above macro sets the "salvaged" bit in JRT_NULL record (new to V44 format)
				 * but is part of "filler" in V22 format and is ignored by that version anyway so ok to
				 * send it as is without unsetting that field.
				 */
			}
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return status;
}

/* Convert a transaction from jnl format V24 (V6.2-000 to V6.3-005 AND r1.00 to r1.22) to V44 (r1.24 onwards)
 * (a) If null-subscript collation is different between the primary and the secondary
 * (b) Filter out jnl records that should not be replicated (i.e. updates done inside a trigger or ^#t records).
 * (c) If NULL record, initialize "salvaged" bit.
 * (d) If no conversion occurred (out-of-design), EREPL_INTLFILTER_NOCONVERT return error code.
 *
 * Note: V24 source server already knows to send only LGTRIG record (and not ^#t records) so no special logic needed to
 *	filter/skip ^#t or updates-within-a-trigger records like "jnl_v22TOv44" has.
 */
int jnl_v24TOv44(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(is_rcvr_server);
	assert(LOCAL_TRIGGER_SUPPORT);	/* A lot of the below code has been simplified because of this assumption */
	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
		reclen = prefix->forwptr;
		BREAK_IF_BADREC(prefix, status); /* check if we encountered a bad record */
		BREAK_IF_INCMPLREC(reclen, jlen, status); /* check if this record is incomplete */
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V24 >= rectype);
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		conv_reclen = prefix->forwptr ;
		BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
		if (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype))
		{
			assert((jb == jstart) && (cb == cstart));
			GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
			if ((HASHT_JREC == trigupd_type) || (NON_REPLIC_JREC_TRIG == trigupd_type))
			{	/* Journal record has a #t global or is a non-#t global but that should not be replicated
				 * (i.e. update done inside of a trigger). Filter both out of the transaction.
				 * However, if this record is a TUPD record, note it down so we promote next UUPD record to a TUPD.
				 */
				if (IS_TUPD(rectype))
					promote_uupd_to_tupd = TRUE;
				assert((cb == cstart) && (jb == jstart));
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			mumps_node_ptr = cb + FIXED_UPD_RECLEN;
			NULLSUBSC_TRANSFORM_IF_NEEDED(rectype, mumps_node_ptr);
			if (IS_TUPD(rectype))
				tupd_num++;
			else if (IS_UUPD(rectype) && promote_uupd_to_tupd)
			{
				((jrec_prefix *)(cb))->jrec_type--;
				assert(IS_TUPD(((jrec_prefix *)(cb))->jrec_type));
				promote_uupd_to_tupd = FALSE;
				tupd_num++;
			}
		} else if (JRT_TCOM == rectype)
		{
			assert((jb == jstart) && (cb == cstart));
			tcom_num++;
			if (tcom_num > tupd_num)
			{	/* This is a case where all updates to one region in the TP transaction were
				 * inside a trigger which means that region did not count towards tupd_num
				 * in which case we should skip tcom_num too for that region.
				 */
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			((struct_jrec_tcom *)(cb))->num_participants = tupd_num;
		} else
		{
			assert(JRT_NULL == rectype);
			assert((cb == cstart) && (jb == jstart));
			memcpy(cb, jb, conv_reclen);
			((struct_jrec_null *)cb)->bitmask.salvaged = FALSE;	/* Set "salvaged" bit (new to V44) */
		}
		cb = cb + conv_reclen;
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if (-1 != status)
	{
		if (cb == conv_buff)
		{	/* No conversion happened. Currently this is NOT possible */
			assert(FALSE);
			repl_errno = EREPL_INTLFILTER_NOCONVERT;
			status = -1;
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return status;
}

/* Convert a transaction from jnl format V44 (r1.24 onwards) to V24 (V6.2-000 to V6.3-005 AND r1.00 to r1.22)
 * (a) If null-subscript collation is different between the primary and the secondary
 * (b) Filter out jnl records that should not be replicated (i.e. updates done inside a trigger or ^#t records).
 * (c) If no conversion occurred (out-of-design), EREPL_INTLFILTER_NOCONVERT return error code.
 *
 * Note: V24 receiver server already knows to expect only LGTRIG record (and not ^#t records) so no special logic needed to
 *	filter/skip ^#t records like "jnl_v44TOv22" has.
 */
int jnl_v44TOv24(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(is_src_server);
	assert(REMOTE_TRIGGER_SUPPORT);	/* A lot of the below code has been simplified because of this assumption */
	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
		reclen = prefix->forwptr;
		BREAK_IF_BADREC(prefix, status); /* check if we encountered a bad record */
		BREAK_IF_INCMPLREC(reclen, jlen, status); /* check if this record is incomplete */
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V44 >= rectype);
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		conv_reclen = prefix->forwptr ;
		BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
		if (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype))
		{
			assert((jb == jstart) && (cb == cstart));
			GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
			if ((HASHT_JREC == trigupd_type) || (NON_REPLIC_JREC_TRIG == trigupd_type))
			{	/* Journal record has a #t global or is a non-#t global but that should not be replicated
				 * (i.e. update done inside of a trigger). Filter both out of the transaction.
				 * However, if this record is a TUPD record, note it down so we promote next UUPD record to a TUPD.
				 */
				if (IS_TUPD(rectype))
					promote_uupd_to_tupd = TRUE;
				assert((cb == cstart) && (jb == jstart));
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			mumps_node_ptr = cb + FIXED_UPD_RECLEN;
			NULLSUBSC_TRANSFORM_IF_NEEDED(rectype, mumps_node_ptr);
			if (IS_TUPD(rectype))
				tupd_num++;
			else if (IS_UUPD(rectype) && promote_uupd_to_tupd)
			{
				((jrec_prefix *)(cb))->jrec_type--;
				assert(IS_TUPD(((jrec_prefix *)(cb))->jrec_type));
				promote_uupd_to_tupd = FALSE;
				tupd_num++;
			}
		} else if (JRT_TCOM == rectype)
		{
			assert((jb == jstart) && (cb == cstart));
			tcom_num++;
			if (tcom_num > tupd_num)
			{	/* This is a case where all updates to one region in the TP transaction were
				 * inside a trigger which means that region did not count towards tupd_num
				 * in which case we should skip tcom_num too for that region.
				 */
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			((struct_jrec_tcom *)(cb))->num_participants = tupd_num;
		} else
		{
			assert(JRT_NULL == rectype);
			assert((cb == cstart) && (jb == jstart));
			memcpy(cb, jb, conv_reclen);
			/* "salvaged" bit in JRT_NULL record in V44 is part of "filler" in V24 format so send it as is */
		}
		cb = cb + conv_reclen;
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if (-1 != status)
	{
		if (cb == conv_buff)
		{	/* No conversion happened. Currently this is NOT possible */
			assert(FALSE);
			repl_errno = EREPL_INTLFILTER_NOCONVERT;
			status = -1;
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return status;
}

/* Convert a transaction from filter format V44 (r1.24 onwards) to V44 (r1.24 onwards).
 * (a) If null-subscript collation is different between the primary and the secondary
 * (b) Filter out jnl records that should not be replicated (i.e. updates done inside a trigger or ^#t records).
 * (c) If no conversion occurred (out-of-design), EREPL_INTLFILTER_NOCONVERT return error code.
 */
int jnl_v44TOv44(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	/* The below comment was valid when V24 was the current filter format. But it is no longer valid now because V44
	 * is the current filter format and there is only ONE journal version corresponding to it so the assert is re-enabled.
	 * It might need to be disabled once we have a jnl format bump without a corresponding filter format bump.
	 * ----------------------
	 * Since filter format V24 corresponds to journal formats V24, V25, or v26, in case of a V24 source and V2{5,6} receiver,
	 * the source server will not do any filter transformations (because receiver jnl ver is higher). This means
	 * jnl_v24TOv24 filter conversion function is invoked on the receiver side to do V24 to V2{5,6} jnl format conversion.
	 * Therefore we cannot do an assert(is_src_server) which we otherwise would have had in case the latest filter
	 * version corresponds to only ONE journal version.
	 * ----------------------
	 */
	assert(is_src_server);
	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
		reclen = prefix->forwptr;
		BREAK_IF_BADREC(prefix, status); /* check if we encountered a bad record */
		BREAK_IF_INCMPLREC(reclen, jlen, status); /* check if this record is incomplete */
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V44 >= rectype);
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		conv_reclen = prefix->forwptr ;
		BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
		if (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype))
		{
			assert((jb == jstart) && (cb == cstart));
			GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
			if ((HASHT_JREC == trigupd_type) || (NON_REPLIC_JREC_TRIG == trigupd_type))
			{	/* Journal record has a #t global or is a non-#t global but that should not be replicated
				 * (i.e. update done inside of a trigger). Filter both out of the transaction.
				 * However, if this record is a TUPD record, note it down so we promote next UUPD record to a TUPD.
				 */
				if (IS_TUPD(rectype))
					promote_uupd_to_tupd = TRUE;
				assert((cb == cstart) && (jb == jstart));
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			mumps_node_ptr = cb + FIXED_UPD_RECLEN;
			NULLSUBSC_TRANSFORM_IF_NEEDED(rectype, mumps_node_ptr);
			if (IS_TUPD(rectype))
				tupd_num++;
			else if (IS_UUPD(rectype) && promote_uupd_to_tupd)
			{
				((jrec_prefix *)(cb))->jrec_type--;
				assert(IS_TUPD(((jrec_prefix *)(cb))->jrec_type));
				promote_uupd_to_tupd = FALSE;
				tupd_num++;
			}
		} else if (JRT_TCOM == rectype)
		{
			assert((jb == jstart) && (cb == cstart));
			tcom_num++;
			if (tcom_num > tupd_num)
			{	/* This is a case where all updates to one region in the TP transaction were
				 * inside a trigger which means that region did not count towards tupd_num
				 * in which case we should skip tcom_num too for that region.
				 */
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			((struct_jrec_tcom *)(cb))->num_participants = tupd_num;
		} else
		{
			assert(JRT_NULL == rectype);
			assert((cb == cstart) && (jb == jstart));
			memcpy(cb, jb, conv_reclen);
		}
		cb = cb + conv_reclen;
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if ((-1 != status) && (cb == conv_buff))
	{	/* No conversion happened. Currently this is not possible */
		assert(FALSE);
		prefix = (jrec_prefix *)(cb);
		repl_errno = EREPL_INTLFILTER_NOCONVERT;
		status = -1;
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return status;
}
