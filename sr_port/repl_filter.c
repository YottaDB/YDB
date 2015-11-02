/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#ifdef UNIX
#include "gtm_ipc.h"
#endif

#ifdef UNIX
#include <sys/mman.h>
#include <sys/shm.h>
#elif defined(VMS)
#include <descrip.h>
#endif
#include <stddef.h>
#include <errno.h>
#include <sys/wait.h>

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
#include "collseq.h"
#include "jnl_typedef.h"
#include "gv_trigger_common.h" /* for HASHT* macros */
#include "replgbl.h"

#define V15_NULL_RECLEN		SIZEOF(v15_jrec_prefix) + SIZEOF(seq_num) + SIZEOF(uint4) + SIZEOF(jrec_suffix)

#define NULLSUBSC_TRANSFORM_IF_NEEDED(PTR)					\
{										\
	int			keylen;						\
	uchar_ptr_t		lclptr;						\
	DCL_THREADGBL_ACCESS;							\
										\
	SETUP_THREADGBL_ACCESS;							\
	if ((TREF(replgbl)).null_subs_xform)					\
	{									\
		assert(SIZEOF(jnl_str_len_t) == SIZEOF(uint4));			\
		keylen = *((jnl_str_len_t *)(PTR));				\
		lclptr = PTR + SIZEOF(jnl_str_len_t);				\
		if (STDNULL_TO_GTMNULL_COLL == (TREF(replgbl)).null_subs_xform)	\
		{								\
			STD2GTMNULLCOLL(lclptr, keylen);			\
		} else								\
		{								\
			GTM2STDNULLCOLL(lclptr, keylen);			\
		}								\
	}									\
}										\

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

#define INITIALIZE_UPDATE_NUM(cstart, cb, jstart, jb, tset_num, update_num, rectype)		\
{												\
	assert((0 != tset_num) || (0 == update_num));						\
	if (IS_TP(rectype))									\
	{											\
		if (IS_TUPD(rectype))								\
			tset_num++;								\
		update_num++;									\
	}											\
	assert(SIZEOF(uint4) == SIZEOF(((struct_jrec_upd *)NULL)->update_num));			\
	assert((cb - cstart) == OFFSETOF(struct_jrec_upd, update_num));				\
	*((uint4 *)cb) = update_num;								\
	cb += SIZEOF(uint4);									\
}

#define INITIALIZE_MUMPS_NODE(cstart, cb, jstart, jb, tail_minus_suffix_len, from_v15)					\
{															\
	uint4			nodelen;										\
															\
	/* Get past the filler_short and num_participants. It is okay not to have them 					\
	 * initialized as they will never be used by the source server or the receiver server and is confined 		\
	 * only to the journal recovery. 										\
	 */														\
	assert((cb - cstart) == OFFSETOF(struct_jrec_upd, filler_short));						\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_upd *)NULL)->filler_short));				\
	cb += (SIZEOF(unsigned short));											\
	assert((cb - cstart) == OFFSETOF(struct_jrec_upd, num_participants));						\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_upd *)NULL)->num_participants));				\
	cb += (SIZEOF(unsigned short));											\
	assert((cb - cstart) == OFFSETOF(struct_jrec_upd, mumps_node));							\
	assert(SIZEOF(uint4) == SIZEOF(jnl_str_len_t));									\
	nodelen = *((uint4 *)jb);											\
	assert(tail_minus_suffix_len >= (SIZEOF(jnl_str_len_t) + nodelen));						\
	memcpy(cb, jb, tail_minus_suffix_len);										\
	NULLSUBSC_TRANSFORM_IF_NEEDED(cb + SIZEOF(jnl_str_len_t));							\
	jb += tail_minus_suffix_len;											\
	cb += tail_minus_suffix_len;											\
}

#define INITIALIZE_V15_V17_MUMPS_NODE(cstart, cb, jstart, jb, trigupd_type, to_v15)				\
{														\
	uint4			tail_minus_suffix_len;								\
	unsigned char		*ptr;										\
														\
	assert((jb - jstart) == OFFSETOF(struct_jrec_upd, update_num));						\
	/* Skip the update_num field from jb since it is not a part of V17/V15 journal record */		\
	assert(SIZEOF(uint4) == SIZEOF(((struct_jrec_upd *)NULL)->update_num));					\
	jb += SIZEOF(uint4);											\
	/* Skip the filler_short, num_participants fields as they are not a part of V15/V17 journal record */	\
	assert((jb - jstart) == OFFSETOF(struct_jrec_upd, filler_short));					\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_upd *)NULL)->filler_short));			\
	jb += (SIZEOF(unsigned short));										\
	assert((jb - jstart) == OFFSETOF(struct_jrec_upd, num_participants));					\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_upd *)NULL)->num_participants));			\
	jb += (SIZEOF(unsigned short));										\
	/* Initialize "mumps_node" member */									\
	tail_minus_suffix_len = (uint4)(jstart + reclen - jb - JREC_SUFFIX_SIZE);				\
	assert(0 < tail_minus_suffix_len);									\
	/* Initialize "mumps_node" member */									\
	memcpy(cb, jb, tail_minus_suffix_len);									\
	/* Check if bits 24-31 of "length" member (nodeflags field) of "mumps_node" field are			\
	 * set to a non-zero value. If so they need to be cleared as they are v21 format specific.		\
	 * Instead of checking, clear it unconditionally.							\
	 */													\
	((jnl_string *)cb)->nodeflags = 0;									\
	GET_JREC_UPD_TYPE(jb, trigupd_type);									\
	NULLSUBSC_TRANSFORM_IF_NEEDED(cb + SIZEOF(jnl_str_len_t));						\
	jb += tail_minus_suffix_len;										\
	cb += tail_minus_suffix_len;										\
}

#define INITIALIZE_TCOM(cstart, cb, jstart, jb, tcom_num, tset_num, update_num)					\
{														\
	uint4		num_participants;									\
	char		tmp_jnl_tid[TID_STR_SIZE];								\
														\
	/* Take copy of V15/V17's jnl_tid */									\
	memcpy(tmp_jnl_tid, jb, SIZEOF(((struct_jrec_tcom *)NULL)->jnl_tid));					\
	jb += SIZEOF(((struct_jrec_tcom *)NULL)->jnl_tid);							\
	/* Skip "filler_short" member */									\
	assert((cb - cstart) == OFFSETOF(struct_jrec_tcom, filler_short));					\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_tcom *)NULL)->filler_short));			\
	cb += SIZEOF(unsigned short);										\
	/* Initialize "num_participants" member */								\
	num_participants = *((uint4 *)jb);									\
	jb += SIZEOF(uint4);											\
	assert((cb - cstart) == OFFSETOF(struct_jrec_tcom, num_participants));					\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_tcom *)NULL)->num_participants));			\
	/* Below is a case of loss of precision but that's okay since we don't expect num_participants		\
	 * to exceed the unsigned short limit */								\
	*((unsigned short *)cb) = num_participants;								\
	cb += SIZEOF(unsigned short);										\
	/* Initialize jnl_tid from tmp_jnl_tid */								\
	assert((cb - cstart) == OFFSETOF(struct_jrec_tcom, jnl_tid[0]));					\
	memcpy(cb, tmp_jnl_tid, SIZEOF(((struct_jrec_tcom *)NULL)->jnl_tid));					\
	cb += SIZEOF(((struct_jrec_tcom *)NULL)->jnl_tid);							\
	/* Do some "update_num" related book-keeping */								\
	tcom_num++;												\
	if (tset_num == tcom_num)										\
	{													\
		tset_num = tcom_num = 0;									\
		update_num = 0;											\
	}													\
}

#define INITIALIZE_V15_V17_TCOM(cstart, cb, jstart, jb)								\
{														\
	uint4		num_participants;									\
														\
	assert((jb - jstart) == OFFSETOF(struct_jrec_tcom, filler_short));					\
	/* Skip the "filler_short" in jb */									\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_tcom *)NULL)->filler_short));			\
	jb += SIZEOF(unsigned short);										\
	assert((jb - jstart) == OFFSETOF(struct_jrec_tcom, num_participants));					\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_tcom *)NULL)->num_participants));			\
	/* Take a copy of the num_participants field from V21's TCOM record */					\
	num_participants = *((unsigned short *)jb);								\
	jb += SIZEOF(unsigned short);										\
	/* Initialize "jnl_tid" member */									\
	assert((jb - jstart) == OFFSETOF(struct_jrec_tcom, jnl_tid[0]));					\
	memcpy(cb, jb, SIZEOF(((struct_jrec_tcom *)NULL)->jnl_tid));						\
	jb += SIZEOF(((struct_jrec_tcom *)NULL)->jnl_tid);							\
	cb += SIZEOF(((struct_jrec_tcom *)NULL)->jnl_tid);							\
	/* Initialize "num_participants" for V17/V15 */								\
	*((uint4 *)cb) = num_participants;									\
	cb += SIZEOF(uint4);											\
}

#define	INITIALIZE_JREC_SUFFIX(cstart, cb, jstart, jb, conv_reclen)		\
{										\
	jrec_suffix	*suffix_ptr;						\
										\
	suffix_ptr = (jrec_suffix *)cb;						\
	suffix_ptr->backptr = conv_reclen;					\
	suffix_ptr->suffix_code = JNL_REC_SUFFIX_CODE;				\
	cb += JREC_SUFFIX_SIZE;							\
	jb += JREC_SUFFIX_SIZE;							\
}

#define INITIALIZE_NULL_RECORD(PREFIX, CB, SEQNO, TO_V15)		\
{									\
	jrec_suffix		*suffix;				\
									\
	(PREFIX)->jrec_type = JRT_NULL;					\
	(PREFIX)->forwptr = (TO_V15 ? V15_NULL_RECLEN : NULL_RECLEN);	\
	CB += (TO_V15 ? SIZEOF(v15_jrec_prefix) : JREC_PREFIX_SIZE);	\
	/* Initialize the sequence number */				\
	*(seq_num *)(CB) = SEQNO;					\
	CB += SIZEOF(seq_num);						\
	/* Skip the filler */						\
	CB += SIZEOF(uint4);						\
	/* Initialize the suffix */					\
	suffix = (jrec_suffix *)(CB);					\
	suffix->backptr = TO_V15 ? V15_NULL_RECLEN : NULL_RECLEN;	\
	suffix->suffix_code = JNL_REC_SUFFIX_CODE;			\
	CB += JREC_SUFFIX_SIZE;						\
}

#define GET_JREC_UPD_TYPE(mumps_node_ptr, trigupd_type)						\
{												\
	jnl_string	*keystr;								\
												\
	keystr = (jnl_string *)(mumps_node_ptr);						\
	trigupd_type = NO_TRIG_JREC;								\
	if(IS_GVKEY_HASHT_GBLNAME(keystr->length, keystr->text))				\
		trigupd_type = HASHT_JREC;							\
	else if ((keystr->nodeflags & JS_NOT_REPLICATED_MASK))					\
		trigupd_type = NON_REPLIC_JREC_TRIG;						\
}

enum
{
	NO_TRIG_JREC = 0,	/* Neither #t global nor triggered update nor an update that should NOT be replicated */
	HASHT_JREC,		/* #t global found in the journal record */
	NON_REPLIC_JREC_TRIG	/* This update was done inside of a trigger */
};

GBLDEF	intlfltr_t repl_filter_cur2old[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	IF_21TO15,
	IF_NONE,
	IF_21TO17,
	IF_21TO17,
	IF_21TO19,
	IF_21TO19,
	IF_21TO21
};

GBLDEF	intlfltr_t repl_filter_old2cur[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	IF_15TO21,
	IF_NONE,
	IF_17TO21,
	IF_17TO21,
	IF_19TO21,
	IF_19TO21,
	IF_21TO21
};

GBLREF	unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char	jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	seq_num		seq_num_zero, seq_num_one;
GBLREF	int4		gv_keysize;
GBLREF	gv_key  	*gv_currkey, *gv_altkey; /* for jnl_extr_init() */
GBLREF	boolean_t	primary_side_trigger_support;
GBLREF	boolean_t	secondary_side_trigger_support;
GBLREF	uchar_ptr_t	repl_filter_buff;
GBLREF	int		repl_filter_bufsiz;
GBLREF	unsigned char	jnl_ver, remote_jnl_ver;
GBLREF	boolean_t	is_src_server, is_rcvr_server;

error_def(ERR_FILTERBADCONV);
error_def(ERR_FILTERCOMM);
error_def(ERR_FILTERNOTALIVE);
error_def(ERR_REPLFILTER);
error_def(ERR_REPLNOXENDIAN);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);

static	pid_t	repl_filter_pid = -1;
static	int	repl_srv_filter_fd[2] = {FD_INVALID, FD_INVALID};
static	int	repl_filter_srv_fd[2] = {FD_INVALID, FD_INVALID};
static	char	*extract_buff;
static	char	*extr_rec;
static	char	*srv_buff_start, *srv_buff_end, *srv_line_start, *srv_line_end, *srv_read_end;

static struct_jrec_null	null_jnlrec;

static seq_num		save_jnl_seqno;
static boolean_t	is_nontp, is_null;
VMS_ONLY(int decc$set_child_standard_streams(int, int, int);)

void jnl_extr_init(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Should be a non-filter related function. But for now,... Needs GBLREFs gv_currkey and transform */
	TREF(transform) = FALSE;      /* to avoid the assert in mval2subsc() */
	GVKEYSIZE_INCREASE_IF_NEEDED(DBKEYSIZE(MAX_KEY_SZ));
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
	{	/* Server */
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
	{	/* Filter */
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
		{	/* exec error, close all pipe fds */
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
	{	/* Error in fork */
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
		 * possible logical record, this should be a bad conversion from the filter
		 */
		return (repl_errno = EREPL_FILTERBADCONV);
	}
}

static int repl_filter_recv(seq_num tr_num, unsigned char *tr, int *tr_len)
{	/* Receive the transaction tr_num into buffer tr. Return the length of the transaction received in tr_len */
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
	   * not relevant on the secondary
	   */
		QWASSIGN(null_jnlrec.jnl_seqno, save_jnl_seqno);
		memcpy(tr, (char *)&null_jnlrec, NULL_RECLEN);
		*tr_len = NULL_RECLEN;
		/* Reset read pointers to avoid the subsequent records from being wrongly interpreted */
		assert(srv_line_end <= srv_read_end);
		assert(srv_line_start <= srv_read_end);
		srv_line_end = srv_line_start = srv_read_end;
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
{	/* Send a special record to indicate stop */
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

/* Issue error if the replication cannot continue. Possible reasons:
 * (a) Remote side (Primary or Secondary) GT.M version is less than V4.4-002
 * (b) If the replication servers does not share the same endianness then GT.M version on each side should be at least V5.3-003
 * Check for (b) is not needed if the source server is VMS since cross-endian replication does not happen on VMS. In such a case,
 * the receiver server will do the appropriate check and shutdown replication if needed.
 */
void repl_check_jnlver_compat(UNIX_ONLY(boolean_t same_endianness))
{	/* see comment in repl_filter.h about list of filter-formats, jnl-formats and GT.M versions */
	UNIX_ONLY(const char	*other_side;)

	assert(is_src_server || is_rcvr_server);
	assert(JNL_VER_EARLIEST_REPL <= remote_jnl_ver);
	if (JNL_VER_EARLIEST_REPL > remote_jnl_ver)
		rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Dual/Multi site replication not supported between these two GT.M versions"));
#	ifdef UNIX
	else if ((V18_JNL_VER > remote_jnl_ver) && !same_endianness)
	{	/* cross-endian replication is supported only from V5.3-003 onwards. Issue error and shutdown. */
		if (is_src_server)
			other_side = "Replicating";
		else if (is_rcvr_server)
			other_side = "Originating";
		else
			GTMASSERT; /* repl_check_jnlver_compat is called only from source server and receiver server */
		rts_error(VARLSTCNT(6) ERR_REPLNOXENDIAN, 4, LEN_AND_STR(other_side), LEN_AND_STR(other_side));
	}
#	endif
}

static void upgrd_hasht_xecute_string(uchar_ptr_t jb, uchar_ptr_t cb, jnl_string *keystr, char *xecute_val_ptr,
					uint4 *conv_reclen)
{	/* Upgrade the ^#t("GBL",1,"XECUTE") node's value to the xecute string format supported in V5.4-002 (and later).
	 * Before V5.4-002, the xecute string is stored as-is in the database. But, from V5.4-002, the xecute string is
	 * stored differently based on whether it is a single-line trigger xecute string or a multi-line trigger xecute
	 * string. If former, the xecute string is prefixed with a <SPACE> character. If latter, no <SPACE> is prefixed.
	 * Since, V5.4-001 does NOT support multi-line triggers, all we need is to prefix the xecute string with <SPACE>
	 */
	 uint4			tmp_jrec_size, jrec_size, align_fill_size;
	 uchar_ptr_t		srcptr;
	 int			valstrlen, new_valstrlen;

	/* An update-type journal record is formatted as follows:
	 *
	 * [FIXED_UPD_RECLEN][KEYLEN][NODEFLAGS][KEY][\0][VALUE_LEN][VALUE][PADDING_ZEROS][JREC_SUFFIX_SIZE]
	 *
	 * The input parameter 'xecute_val_ptr' points to the beginning of VALUE_LEN in the above journal format. Since
	 * <SPACE> is added to the xecute string, this will affect the VALUE_LEN, PADDING_ZEROS, record length.
	 * Note: 'cb' already has a copy of 'jb' and 'xecute_val_ptr' is a pointer inside 'cb'
	 */
	GET_MSTR_LEN(valstrlen, xecute_val_ptr); 	/* get the length of the value */
	new_valstrlen = valstrlen + 1;
	PUT_MSTR_LEN(xecute_val_ptr, new_valstrlen);
	/* new record size and padding byte calculations */
	tmp_jrec_size = FIXED_UPD_RECLEN
				+ SIZEOF(jnl_str_len_t) + keystr->length
				+ SIZEOF(mstr_len_t) + (valstrlen + 1)
				+ JREC_SUFFIX_SIZE;
	jrec_size = ROUND_UP2(tmp_jrec_size, JNL_REC_START_BNDRY); /* new record size (aligned to 8 byte boundary) */
	align_fill_size = jrec_size - tmp_jrec_size; /* necessary padding bytes */
	/* add <SPACE> and copy the xecute string */
	xecute_val_ptr += SIZEOF(mstr_len_t);		/* move to the beginning of the 'value' */
	srcptr = jb + ((uchar_ptr_t)xecute_val_ptr - cb);	/* pointer to the xecute string from the source buffer */
	*xecute_val_ptr++ = ' ';			/* add the much needed <SPACE> */
	memcpy(xecute_val_ptr, srcptr, valstrlen);	/* copy the actual xecute string */
	xecute_val_ptr += valstrlen;
	memset(xecute_val_ptr, 0, align_fill_size);	/* padding zeros for 8 byte alignment */
	xecute_val_ptr += align_fill_size;		/* points to suffix */
	/* fill suffix */
	((jrec_suffix *)(xecute_val_ptr))->backptr = jrec_size;
	((jrec_suffix *)(xecute_val_ptr))->suffix_code = JNL_REC_SUFFIX_CODE;
	assert((0 != *conv_reclen) && !(*conv_reclen % JNL_REC_START_BNDRY));
	if (*conv_reclen != jrec_size)
		((jrec_prefix *)(cb))->forwptr = *conv_reclen = jrec_size;
}

/* The following are the functions that convert one jnl format to another.
 * The only replicated records we expect to see here are *SET* or *KILL* or TCOM or NULL records.
 * These fall under the following 3 structure types each of which is described for the different jnl formats we handle.
 * V15 format
 * -----------
 *  struct_jrec_upd layout is as follows.
 *	offset = 0000 [0x0000]      size = 0016 [0x0010]    ----> prefix
 *	offset = 0016 [0x0010]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> mumps_node
 * struct_jrec_tcom layout is as follows.
 *	offset = 0000 [0x0000]      size = 0016 [0x0010]    ----> prefix
 *	offset = 0016 [0x0010]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> jnl_tid
 *	offset = 0032 [0x0020]      size = 0004 [0x0004]    ----> participants
 *	offset = 0036 [0x0024]      size = 0004 [0x0004]    ----> suffix
 * struct_jrec_null layout is as follows.
 *	offset = 0000 [0x0000]      size = 0016 [0x0010]    ----> prefix
 *	offset = 0016 [0x0010]      size = 0008 [0x0008]    ----> jnl_seqno
 *	offset = 0024 [0x0018]      size = 0004 [0x0004]    ----> filler
 *	offset = 0028 [0x001c]      size = 0004 [0x0004]    ----> suffix
 *
 * V17 format
 * -----------
 *  struct_jrec_upd layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0032 [0x0020]      size = 0008 [0x0008]    ----> mumps_node
 * struct_jrec_tcom layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0032 [0x0020]      size = 0008 [0x0008]    ----> jnl_tid
 *	offset = 0040 [0x0028]      size = 0004 [0x0004]    ----> participants
 *	offset = 0044 [0x002c]      size = 0004 [0x0004]    ----> suffix
 * struct_jrec_null layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> jnl_seqno
 *	offset = 0032 [0x0020]      size = 0004 [0x0004]    ----> filler
 *	offset = 0036 [0x0024]      size = 0004 [0x0004]    ----> suffix
 *
 * V19 format
 * -----------
 *  struct_jrec_upd layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0032 [0x0020]      size = 0004 [0x0004]    ----> update_num
 *	offset = 0036 [0x0024]      size = 0002 [0x0002]    ----> filler_short
 *	offset = 0038 [0x0026]      size = 0002 [0x0002]    ----> num_participants
 *	offset = 0040 [0x0028]      size = 0008 [0x0008]    ----> mumps_node
 * struct_jrec_tcom layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> token_seq
 *	offset = 0032 [0x0020]      size = 0002 [0x0002]    ----> filler_short
 *	offset = 0034 [0x0022]      size = 0002 [0x0002]    ----> num_participants
 *	offset = 0036 [0x0024]      size = 0008 [0x0008]    ----> jnl_tid
 *	offset = 0044 [0x002c]      size = 0004 [0x0004]    ----> suffix
 * struct_jrec_null layout is as follows.
 *	offset = 0000 [0x0000]      size = 0024 [0x0018]    ----> prefix
 *	offset = 0024 [0x0018]      size = 0008 [0x0008]    ----> jnl_seqno
 *	offset = 0032 [0x0020]      size = 0004 [0x0004]    ----> filler
 *	offset = 0036 [0x0024]      size = 0004 [0x0004]    ----> suffix
 *
 * V21 format
 * -----------
 * Same as V19 except JNL_ZTRIG records are allowed
 */

/* Convert a transaction from jnl version 15 (V4.4-002 through V4.4-004) to 21 (V5.4-002 onwards).
 * Differences between the two versions:
 * ------------------------------------
 * (a) V21 prefix size is 8 bytes more than V15 prefix size (owing to the TN field being 8 bytes instead of 4 bytes and
 *     a new filed checksum of 4 bytes)
 * (b) Update record is 8 bytes more than V15 (owing to the 4 byte update_num and 2 byte num_participants field). Since
 *     every jnl record is 8-byte aligned, the difference is 8 bytes.
 * This means, we need to have 16 more bytes in the conversion buffer for SET/KILL type of records and 8 more bytes for
 * (c) If the null collation is different between primary and secondary (null_subs_xform) then appropriate conversion
 *     is needed
 * TCOM/NULL type of records. Reformat accordingly.
 */
int jnl_v15TOv21(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tail_minus_suffix_len;
	jrec_prefix 		*prefix;
	v15_jrec_prefix		*v15_prefix;
	boolean_t		is_set_kill_zkill_ztrig;
	static uint4		update_num, tset_num, tcom_num;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
	while (SIZEOF(v15_jrec_prefix) <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		v15_prefix = (v15_jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)v15_prefix->jrec_type;
		if (IS_ZTP(rectype))
			GTMASSERT;	/* ZTP not supported */
		cstart = cb;
		jstart = jb;
		reclen = v15_prefix->forwptr;
		BREAK_IF_BADREC(v15_prefix, status); /* check if we encountered a bad record */
		BREAK_IF_INCMPLREC(reclen, jlen, status); /* check if this record is incomplete */
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V15 >= rectype);
		is_set_kill_zkill_ztrig = IS_SET_KILL_ZKILL_ZTRIG(rectype);
		assert(is_set_kill_zkill_ztrig || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
		assert(v15_prefix->forwptr > SIZEOF(v15_jrec_prefix));
		conv_reclen = v15_prefix->forwptr + (is_set_kill_zkill_ztrig ? 16 : 8);
		BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
		/* Initialize "prefix" member */
		prefix = (jrec_prefix *)cb;
		prefix->jrec_type = rectype;
		prefix->forwptr = conv_reclen;
		prefix->pini_addr = 0;
		prefix->time = 0;
		prefix->tn = 0;
		cb += JREC_PREFIX_SIZE;
		jb += SIZEOF(v15_jrec_prefix);
		/* Initialize "token_seq" or "jnl_seqno" member */
		assert((cb - cstart) == OFFSETOF(struct_jrec_upd, token_seq));
		assert(OFFSETOF(struct_jrec_null, jnl_seqno) == OFFSETOF(struct_jrec_upd, token_seq));
		assert(OFFSETOF(struct_jrec_tcom, token_seq) == OFFSETOF(struct_jrec_upd, token_seq));
		*((token_seq_t *)cb) = *(token_seq_t *)jb;
		cb += SIZEOF(token_seq_t);
		jb += SIZEOF(token_seq_t);
		tail_minus_suffix_len = (uint4)(jstart + reclen - jb - JREC_SUFFIX_SIZE);
		assert(0 < tail_minus_suffix_len);
		if (is_set_kill_zkill_ztrig)
		{	/* Initialize "update_num" member */
			INITIALIZE_UPDATE_NUM(cstart, cb, jstart, jb, tset_num, update_num, rectype);
			/* Initialize "mumps_node" member */
			INITIALIZE_MUMPS_NODE(cstart, cb, jstart, jb, tail_minus_suffix_len, TRUE);
		} else if (JRT_TCOM == rectype)
		{
			assert((jb - jstart) == (SIZEOF(v15_jrec_prefix) + SIZEOF(token_seq_t)));
			/* side-effect: cb and jb pointers incremented */
			INITIALIZE_TCOM(cstart, cb, jstart, jb, tcom_num, tset_num, update_num);
		} else
		{	/* NULL record : only "filler" member remains so no need to do any copy */
			cb += tail_minus_suffix_len;
			jb += tail_minus_suffix_len;
		}
		/* assert that we have just the suffix to be written */
		assert((cb - cstart) == (conv_reclen - JREC_SUFFIX_SIZE));
		assert((jb - jstart) == (reclen - JREC_SUFFIX_SIZE));
		/* Initialize "suffix" member */
		INITIALIZE_JREC_SUFFIX(cstart, cb, jstart, jb, conv_reclen); /* side-effect: cb and jb pointers incremented */
		/* Nothing more to be initialized for this record. Assert that. */
		assert(ROUND_UP2(conv_reclen, JNL_REC_START_BNDRY) == conv_reclen);
		assert(cb == cstart + conv_reclen);
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((*jnl_len == (jb - jnl_buff)) || (-1 == status));
	*conv_len =(uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return(status);
}

/* Convert a transaction from jnl version 17 (V5.0-000 through V5.3-004A) to 21 (V5.4-002 onwards)
 * Differences between the two versions:
 * ------------------------------------
 * (a) Update record is 8 bytes more than V17 (owing to the 4 byte update_num and 2 byte num_participants field). Since
 *     every jnl record is 8-byte aligned, the difference is 8 bytes.
 * This means, we need to have 16 more bytes in the conversion buffer for SET/KILL type of records and 8 more bytes for
 * (b) If the null collation is different between primary and secondary (null_subs_xform) then appropriate conversion
 *     is needed
 * Reformat accordingly.
 */
int jnl_v17TOv21(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen, t_len;
	uint4			jlen, tail_minus_suffix_len;
	jrec_prefix 		*prefix;
	boolean_t		is_set_kill_zkill_ztrig;
	static uint4		update_num, tset_num, tcom_num;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		if (IS_ZTP(rectype))
			GTMASSERT;	/* ZTP not supported */
		cstart = cb;
		jstart = jb;
		reclen = prefix->forwptr;
		BREAK_IF_BADREC(prefix, status); /* check if we encountered a bad record */
		BREAK_IF_INCMPLREC(reclen, jlen, status); /* check if this record is incomplete */
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V17 >= rectype);
		is_set_kill_zkill_ztrig = IS_SET_KILL_ZKILL_ZTRIG(rectype);
		assert(is_set_kill_zkill_ztrig || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
		assert(prefix->forwptr > SIZEOF(jrec_prefix));
		conv_reclen = prefix->forwptr;
		if (is_set_kill_zkill_ztrig)
			conv_reclen += 8; /* Due to update_num (4-byte) added in V19 and extra 4-byte for 8 byte alignment */
		BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
		/* Initialize "prefix" and "token_seq" or "jnl_seqno" members */
		assert(OFFSETOF(struct_jrec_null, jnl_seqno) == OFFSETOF(struct_jrec_upd, token_seq));
		assert(OFFSETOF(struct_jrec_tcom, token_seq) == OFFSETOF(struct_jrec_upd, token_seq));
		assert(SIZEOF(token_seq_t) == SIZEOF(seq_num));
		t_len = (JREC_PREFIX_SIZE + SIZEOF(token_seq_t));
		memcpy(cb, jb, t_len);
		((jrec_prefix *)cb)->forwptr = conv_reclen; /* forwptr will be different between V17 and V21 due to update_num */
		cb += t_len;
		jb += t_len;
		tail_minus_suffix_len = (uint4)(jstart + reclen - jb - JREC_SUFFIX_SIZE);
		assert(0 < tail_minus_suffix_len);
		if (is_set_kill_zkill_ztrig)
		{	/* Initialize "update_num" member */
			INITIALIZE_UPDATE_NUM(cstart, cb, jstart, jb, tset_num, update_num, rectype);
			/* Initialize "mumps_node" member */
			INITIALIZE_MUMPS_NODE(cstart, cb, jstart, jb, tail_minus_suffix_len, FALSE);
		} else if (JRT_TCOM == rectype)
		{
			assert((jb - jstart) == (OFFSETOF(struct_jrec_tcom, token_seq) + SIZEOF(token_seq_t)));
			INITIALIZE_TCOM(cstart, cb, jstart, jb, tcom_num, tset_num, update_num);
		} else
		{	/* NULL record : only "filler" member remains so no need to do any copy */
			cb += tail_minus_suffix_len;
			jb += tail_minus_suffix_len;
		}
		/* assert that we have just the suffix to be written */
		assert((cb - cstart) == (conv_reclen - JREC_SUFFIX_SIZE));
		assert((jb - jstart) == (reclen - JREC_SUFFIX_SIZE));
		/* Initialize "suffix" member */
		INITIALIZE_JREC_SUFFIX(cstart, cb, jstart, jb, conv_reclen); /* side-effect: cb and jb pointers incremented */
		/* Nothing more to be initialized for this record */
		assert(ROUND_UP2(conv_reclen, JNL_REC_START_BNDRY) == conv_reclen);
		assert(cb == cstart + conv_reclen);
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	assert(0 == jlen || -1 == status);
	assert((*jnl_len == (jb - jnl_buff)) || (-1 == status));
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return(status);
}

/* Convert a transaction from jnl version 21 (V5.4-002 onwards) to 15 (V4.4-002 through V4.4-004)
 * For differences between the two versions, see the comment in jnl_v15TOv21. In addition, the following things should be
 * taken care of:
 * (a) Key size was limited to 8 characters in pre V5 versions. If the journal record has a key whose size is greater than
 *     8, we don't do anymore conversion and set the repl_errno to EREPL_INTLFILTER_REPLGBL2LONG
 * (b) If a ^#t journal record is found, then send a NULL record instead.
 * (c) Handle ZTWORM and ZTRIG journal records by skipping them *
 * NOTE: Although (b) and (c) are trigger specific, the logic should be available for trigger non-supporting platorms as well to
 * handle replication scenarios like V5.4-001 (TS) -> V5.4-002 (NTS) -> V4.4-004 (NTS) where TS indicates a trigger supporting
 * platform and NTS indicates either a version without trigger support OR is running on trigger non-supporting platform.
 */
int jnl_v21TOv15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tail_minus_suffix_len, tupd_num = 0, tcom_num = 0;
	boolean_t		is_set_kill_zkill_ztrig, promote_uupd_to_tupd;
	jrec_prefix 		*prefix;
	v15_jrec_prefix		*v15_prefix;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;
	DEBUG_ONLY(boolean_t	non_ztrig_rec_found = FALSE;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(!secondary_side_trigger_support);
	assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
	QWASSIGN(this_upd_seqno, seq_num_zero);
	promote_uupd_to_tupd = FALSE;
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
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		assert(IS_REPLICATED(rectype));
		if (!IS_ZTWORM(rectype) && !IS_ZTRIG(rectype))
		{
			if (IS_ZTP(rectype))
				GTMASSERT;	/* ZTP not supported */
			is_set_kill_zkill_ztrig = IS_SET_KILL_ZKILL_ZTRIG(rectype);
			assert(is_set_kill_zkill_ztrig || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
			conv_reclen = prefix->forwptr - (is_set_kill_zkill_ztrig ? 16 : 8);
			BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
			/* Pre V5 versions did not support global names longer than 8 characters excluding NULL characters
			 * Check if this is the case and if so, no point proceeding with the conversion.
			 */
			if (is_set_kill_zkill_ztrig)
			{
				assert((jb == jstart) && (cb == cstart));
				ptr = jb + FIXED_UPD_RECLEN + SIZEOF(jnl_str_len_t);
				if (strlen((char *)ptr) > PRE_V5_MAX_MIDENT_LEN)
				{
					repl_errno = EREPL_INTLFILTER_REPLGBL2LONG;
					assert(FALSE);
					status = -1;
					break;
				}
			}
			/* Initialize V15 "prefix" */
			v15_prefix = (v15_jrec_prefix *)cb;
			v15_prefix->jrec_type = rectype;
			v15_prefix->forwptr = conv_reclen;
			/* It's okay to leave the other fields uninitialized as they are not used by the receiever server */
			cb = cb + SIZEOF(v15_jrec_prefix);
			jb = jb + JREC_PREFIX_SIZE;
			/* Initialize "token_seq" or "jnl_seqno" */
			assert(SIZEOF(token_seq_t) == SIZEOF(seq_num));
			assert((jb - jstart) == OFFSETOF(struct_jrec_upd, token_seq));
			*((token_seq_t *)cb) = (*(token_seq_t *)jb);
			cb += SIZEOF(token_seq_t);
			jb += SIZEOF(token_seq_t);
			if (is_set_kill_zkill_ztrig)
			{
				DEBUG_ONLY(
					if (!IS_ZTRIG(rectype))
						non_ztrig_rec_found = TRUE;
				)
				/* side-effect: increments cb and jb and GTM Null Collation or Standard Null Collation applied */
				INITIALIZE_V15_V17_MUMPS_NODE(cstart, cb, jstart, jb, trigupd_type, TRUE);
				if (HASHT_JREC == trigupd_type)
				{	/* Journal record has a ^#t global. ^#t records are not replicated if the secondary does not
					 * support triggers. However, $ZTRIGGER() usages within TP can cause ^#t records to be
					 * generated in the middle of a TP transaction. Hence skip the ^#t records. However, if this
					 * ^#t record is a TUPD record, then note it down so that we promote the next UUPD record to
					 * a TUPD record.
					 */
					jb = jstart + reclen;
					cb = cstart;
					jlen -= reclen;
					if (IS_TUPD(rectype))
						promote_uupd_to_tupd = TRUE;
					continue;
				}
				if (IS_TUPD(rectype))
					tupd_num++;
				else if (IS_UUPD(rectype) && promote_uupd_to_tupd)
				{
					/* The previous TUPD record was not replicated since it was a TZTWORM record and hence
					 * promote this UUPD to TUPD. Since the update process on the secondary will not care
					 * about the num_participants field in the TUPD records (it cares about the num_participants
					 * field only in the TCOM record), it is okay not to initialize the num_participants field
					 * of the promoted UUPD record
					 */
					v15_prefix->jrec_type--;
					assert(IS_TUPD(v15_prefix->jrec_type));
					promote_uupd_to_tupd = FALSE;
					tupd_num++;
				}
			} else if (JRT_TCOM == rectype)
			{
				tcom_num++;
				if (tcom_num > tupd_num)
				{	/* TCOM records already balanced with the existing TSTART records. Skip further records. */
					jb = jstart + reclen;
					jlen -= reclen;
					cb = cstart;
					continue;
				}
				/* We better have initialized the "prefix" and "token_seq" */
				assert((cb - cstart) == (SIZEOF(v15_jrec_prefix) + SIZEOF(token_seq_t)));
				INITIALIZE_V15_V17_TCOM(cstart, cb, jstart, jb); /* side-effect: increments cb and jb */
			} else
			{
				assert (JRT_NULL == rectype);
				tail_minus_suffix_len = (uint4)(jstart + reclen - jb - JREC_SUFFIX_SIZE);
				jb += tail_minus_suffix_len;
				cb += tail_minus_suffix_len;
			}
			/* assert that we have just the suffix to be written */
			assert((cb - cstart) == (conv_reclen - JREC_SUFFIX_SIZE));
			assert((jb - jstart) == (reclen - JREC_SUFFIX_SIZE));
			/* Initialize "suffix" member */
			INITIALIZE_JREC_SUFFIX(cstart, cb, jstart, jb, conv_reclen);
			assert(ROUND_UP2(conv_reclen, JNL_REC_START_BNDRY) == conv_reclen);
			assert(cb == cstart + conv_reclen);
		} else
		{	/* $ZTWORMHOLE jnl record does not exist in previous V15 so skip converting it */
			assert((cb == cstart) && (jb == jstart)); /* No conversions yet */
			jb = jstart + reclen;
			/* If this is a TUPD rectype (actually JRT_TZTWORM/JRT_TZTRIG) then the next UUPD has to be promoted
			 * to a TUPD type to account for the balance in TUPD and TCOM records
			 */
			if (IS_TUPD(rectype))
			{
				assert((JRT_TZTWORM == rectype) || (JRT_TZTRIG == rectype));
				promote_uupd_to_tupd = TRUE;
			}
		}
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if ((-1 != status) && (cb == conv_buff))
	{	/* No conversion happened. Currently this is possible if
		 * (a) All the records are ^#t.
		 * (b) If the only records in a transaction are ZTRIG records and a TCOM record.
		 * In both the above cases we need to send a NULL record instead.
		 */
		assert((HASHT_JREC == trigupd_type) || (FALSE == non_ztrig_rec_found));
		GTMTRIG_ONLY(
			if (!(TREF(replgbl)).trig_replic_suspect_seqno)
				(TREF(replgbl)).trig_replic_suspect_seqno = this_upd_seqno;
		)
		v15_prefix = (v15_jrec_prefix *)(cb);
		if (V15_NULL_RECLEN > conv_bufsiz)
		{
			repl_errno = EREPL_INTLFILTER_NOSPC;
			status = -1;
		} else
		{
			/* Side-effect: cb is incremented by V15_NULL_RECLEN */
			INITIALIZE_NULL_RECORD(v15_prefix, cb, this_upd_seqno, TRUE);
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	return(status);
}

/* Convert a transaction from jnl version 21 (V5.4-002 onwards) to 17 (V5.0-000 through V5.3-004A)
 * For differences between the two versions, see the comment in jnl_v17TOv21. In addition, the following things should be
 * taken care of:
 * (a) If a ^#t journal record is found, then send a NULL record instead.
 * (b) Handle ZTWORM and ZTRIG journal records by skipping them
 *
 * Note: Although (a) and (b) are trigger specific, the logic should be available for trigger non-supporting platorms as well to
 * handle replication scenarios like V5.4-001 (TS) -> V5.4-002 (NTS) -> V4.4-004 (NTS) where TS indicates a trigger supporting
 * platform and NTS indicates either a version without trigger support OR is running on trigger non-supporting platform.
 */
int jnl_v21TOv17(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, t_len, tail_minus_suffix_len, tupd_num = 0, tcom_num = 0;
	boolean_t		is_set_kill_zkill_ztrig, promote_uupd_to_tupd;
	jrec_prefix 		*prefix;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;
	DEBUG_ONLY(boolean_t	non_ztrig_rec_found = FALSE;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
	QWASSIGN(this_upd_seqno, seq_num_zero);
	promote_uupd_to_tupd = FALSE;
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
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		assert(IS_REPLICATED(rectype));
		if (!IS_ZTWORM(rectype) && !IS_ZTRIG(rectype))
		{
			if (IS_ZTP(rectype))
				GTMASSERT;	/* ZTP not supported */
			is_set_kill_zkill_ztrig = IS_SET_KILL_ZKILL_ZTRIG(rectype);
			assert(is_set_kill_zkill_ztrig || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
			conv_reclen = prefix->forwptr - (is_set_kill_zkill_ztrig ? 8 : 0);
			BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
			/* Initialize "prefix" and "token_seq" or "jnl_seqno" members */
			assert(OFFSETOF(struct_jrec_null, jnl_seqno) == OFFSETOF(struct_jrec_upd, token_seq));
			assert(OFFSETOF(struct_jrec_tcom, token_seq) == OFFSETOF(struct_jrec_upd, token_seq));
			assert(SIZEOF(token_seq_t) == SIZEOF(seq_num));
			t_len = (JREC_PREFIX_SIZE + SIZEOF(token_seq_t));
			memcpy(cb, jb, t_len);
			((jrec_prefix *)cb)->forwptr = conv_reclen; /* forwptr will be different between V17 and V21
								     * due to update_num */
			cb += t_len;
			jb += t_len;
			if (is_set_kill_zkill_ztrig)
			{
				DEBUG_ONLY(
					if (!IS_ZTRIG(rectype))
						non_ztrig_rec_found = TRUE;
				)
				assert((cb - cstart) == (OFFSETOF(struct_jrec_upd, token_seq) + SIZEOF(token_seq_t)));
				/* side-effect: increments cb and jb and GTM Null Collation or Standard Null Collation applied */
				INITIALIZE_V15_V17_MUMPS_NODE(cstart, cb, jstart, jb, trigupd_type, FALSE);
				if (HASHT_JREC == trigupd_type)
				{	/* Journal record has a #t global. #t records are not replicated if the secondary does not
					 * support triggers. However, $ZTRIGGER() usages within TP can cause ^#t records to be
					 * generated in the middle of a TP transaction. Hence skip the ^#t records. However, if this
					 * ^#t record is a TUPD record, then note it down so that we promote the next UUPD record to
					 * a TUPD record.
					 */
					jb = jstart + reclen;
					cb = cstart;
					jlen -= reclen;
					if (IS_TUPD(rectype))
						promote_uupd_to_tupd = TRUE;
					continue;
				}
				if (IS_TUPD(rectype))
					tupd_num++;
				else if (IS_UUPD(rectype) && promote_uupd_to_tupd)
				{
					/* The previous TUPD record was not replicated since it was a TZTWORM/TZTRIG record and
					 * hence promote this UUPD to TUPD. Since the update process on the secondary will not care
					 * about the num_participants field in the TUPD records (it cares about the num_participants
					 * field only in the TCOM record), it is okay not to initialize the num_participants field
					 * of the promoted UUPD record. However, since 'cb' is incremented at this point, use
					 * 'cstart' which points to the beginning of this journal record.
					 */
					((jrec_prefix *)cstart)->jrec_type--;
					assert(IS_TUPD(((jrec_prefix *)cstart)->jrec_type));
					promote_uupd_to_tupd = FALSE;
					tupd_num++;
				}
			} else if (JRT_TCOM == rectype)
			{
				tcom_num++;
				if (tcom_num > tupd_num)
				{	/* TCOM records already balanced with the existing TSTART records. Skip further records. */
					jb = jstart + reclen;
					jlen -= reclen;
					cb = cstart;
					continue;
				}
				/* We better have initialized the "prefix" and "token_seq" */
				assert((cb - cstart) == (OFFSETOF(struct_jrec_tcom, token_seq) + SIZEOF(token_seq_t)));
				INITIALIZE_V15_V17_TCOM(cstart, cb, jstart, jb); /* side-effect: increments cb and jb */
			} else
			{
				assert (JRT_NULL == rectype);
				tail_minus_suffix_len = (uint4)(jstart + reclen - jb - JREC_SUFFIX_SIZE);
				jb += tail_minus_suffix_len;
				cb += tail_minus_suffix_len;
			}
			/* assert that we have just the suffix to be written */
			assert((cb - cstart) == (conv_reclen - JREC_SUFFIX_SIZE));
			assert((jb - jstart) == (reclen - JREC_SUFFIX_SIZE));
			/* Initialize "suffix" member */
			INITIALIZE_JREC_SUFFIX(cstart, cb, jstart, jb, conv_reclen);
			assert(ROUND_UP2(conv_reclen, JNL_REC_START_BNDRY) == conv_reclen);
			assert(cb == cstart + conv_reclen);
		} else
		{	/* $ZTWORMHOLE jnl record does not exist in previous V15 so skip converting it */
			assert((cb == cstart) && (jb == jstart)); /* No conversions yet */
			jb = jstart + reclen;
			/* If this is a TUPD rectype (actually JRT_TZTWORM) then the next UUPD has to be promoted to a TUPD type
			 * to account for the balance in TUPD and TCOM records
			 */
			if (IS_TUPD(rectype))
			{
				assert((JRT_TZTWORM == rectype) || (JRT_TZTRIG == rectype));
				promote_uupd_to_tupd = TRUE;
			}
		}
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if ((-1 != status) && (cb == conv_buff))
	{	/* No conversion happened. Currently this is possible if
		 * (a) All the records are ^#t.
		 * (b) If the only records in a transaction are ZTRIG records and a TCOM record.
		 * In both the above cases we need to send a NULL record instead.
		 */
		assert((HASHT_JREC == trigupd_type) || (FALSE == non_ztrig_rec_found));
		GTMTRIG_ONLY(
			if (!(TREF(replgbl)).trig_replic_suspect_seqno)
				(TREF(replgbl)).trig_replic_suspect_seqno = this_upd_seqno;
		)
		prefix = (jrec_prefix *)(cb);
		if (NULL_RECLEN > conv_bufsiz)
		{
			repl_errno = EREPL_INTLFILTER_NOSPC;
			status = -1;
		} else
		{
			/* Side-effect: cb is incremented by NULL_RECLEN */
			INITIALIZE_NULL_RECORD(prefix, cb, this_upd_seqno, FALSE);
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	return(status);
}

/* Convert a transaction from jnl version 19 (V5.4-000 through V5.4-001) to 21 (V5.4-002 onwards)
 * In cases where the secondary does NOT support triggers, the following filtering is done:
 * (a) If a ^#t journal record is found, then send a NULL record instead.
 * (b) Skip ZTWORM journal records as the secondary does NOT understand them
 */
int jnl_v19TOv21(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *mumps_node_ptr;
	char			*keyend, *ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, vallen;
	uint4			conv_reclen, jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd;
	jnl_string		*keystr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	QWASSIGN(this_upd_seqno, seq_num_zero);
	promote_uupd_to_tupd = FALSE;
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
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		assert(IS_REPLICATED(rectype));
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		if (!IS_ZTWORM(rectype) || secondary_side_trigger_support)
		{
			if (IS_ZTP(rectype))
				GTMASSERT;	/* ZTP not supported */
			conv_reclen = prefix->forwptr;
			BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
			if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
			{
				GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
				if (secondary_side_trigger_support)
				{
					if (NON_REPLIC_JREC_TRIG == trigupd_type)
					{
						if (IS_TUPD(rectype))
							promote_uupd_to_tupd = TRUE;
						assert((cb == cstart) && (jb == jstart));
						jb = jb + reclen;
						jlen -= reclen;
						continue;
					}
				} else if (HASHT_JREC == trigupd_type)
				{	/* Journal record has a #t global. #t records are not replicated if the secondary does not
					 * support triggers. Instead a NULL record needs to be sent
					 */
					assert((jb == jnl_buff) && (cb == conv_buff)); /* if ^#t, we better see it as the first
											* journal record */
					break;
				}
				memcpy(cb, jb, conv_reclen);
				mumps_node_ptr = cb + FIXED_UPD_RECLEN;
				if ((HASHT_JREC == trigupd_type) && secondary_side_trigger_support)
				{	/* ^#t record */
					keystr = (jnl_string *)mumps_node_ptr;
					keyend = &keystr->text[keystr->length - 1];
					if (!MEMCMP_LIT((keyend - LITERAL_HASHLABEL_LEN), LITERAL_HASHLABEL))
					{	/* ^#t("GBL","#LABEL") found. Adjust the value of this node to be equal to
						 * HASHT_GBL_CURLABEL as the secondary is the latest trigger supported version.
						 */
						assert(IS_SET(rectype)); /* This better be a SET record */
						ptr = keyend + SIZEOF(mstr_len_t) + 1; /* '+ 1' to account for '- 1' done above */
						assert(0 == MEMCMP_LIT(ptr, V19_HASHT_GBL_LABEL));
						assert(STR_LIT_LEN(HASHT_GBL_CURLABEL) == STR_LIT_LEN(V19_HASHT_GBL_LABEL));
						MEMCPY_LIT(ptr, HASHT_GBL_CURLABEL);
					} else if (!MEMCMP_LIT(keyend - LITERAL_XECUTE_LEN, LITERAL_XECUTE))
					{	/* ^#t("GBL",1,"XECUTE") found. */
						assert(IS_SET(rectype)); /* This better be a SET record */
						upgrd_hasht_xecute_string(jb, cb, keystr, keyend + 1, &conv_reclen);
						assert(0 == conv_reclen % JNL_REC_START_BNDRY);
					}
				}
				if (!secondary_side_trigger_support)
					((jnl_string *)mumps_node_ptr)->nodeflags = 0;
				NULLSUBSC_TRANSFORM_IF_NEEDED(mumps_node_ptr);
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
				tcom_num++;
				assert((cb == cstart) && (jb == jstart));
				if (tcom_num > tupd_num)
				{
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
		} else
		{	/* $ZTWORMHOLE jnl record cannot be handled by secondary which does not support triggers so skip converting.
			 * If this is a TUPD rectype (actually JRT_TZTWORM) then the next UUPD has to be promoted to a TUPD type to
			 * account for the balance in TUPD and TCOM records
			 */
			assert((cb == cstart) && (jb == jstart));
			if (IS_TUPD(rectype))
			{
				assert(JRT_TZTWORM == rectype);
				promote_uupd_to_tupd = TRUE;
			}
		}
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status) || (HASHT_JREC == trigupd_type));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status) || (HASHT_JREC == trigupd_type));
	if ((-1 != status) && (cb == conv_buff))
	{	/* No conversion happened. Currently this is possible ONLY if all the records are ^#t records. Need to send
		 * NULL record
		 */
		assert(!secondary_side_trigger_support && (HASHT_JREC == trigupd_type));
		GTMTRIG_ONLY(
			if (!(TREF(replgbl)).trig_replic_suspect_seqno)
				(TREF(replgbl)).trig_replic_suspect_seqno = this_upd_seqno;
		)
		prefix = (jrec_prefix *)(cb);
		if (NULL_RECLEN > conv_bufsiz)
		{
			repl_errno = EREPL_INTLFILTER_NOSPC;
			status = -1;
		} else
		{
			INITIALIZE_NULL_RECORD(prefix, cb, this_upd_seqno, FALSE); /* Side-effect: cb is incremented */
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return(status);
}

/* Convert a transaction from jnl version 21 (V5.4-002 onwards) to 19 (V5.4-000 through V5.4-001)
 * If the receiver server does NOT support triggers, then
 * (a) If a ^#t journal record is found, then send a NULL record instead.
 * (b) Handle ZTWORM and ZTRIG journal records by skipping them
 */
int jnl_v21TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *mumps_node_ptr;
	char			*keyend, *ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd, hasht_update_found, normal_update_found;
	DEBUG_ONLY(boolean_t	non_ztrig_rec_found = FALSE;)
	DEBUG_ONLY(int		valstrlen;)
	jnl_string		*keystr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	QWASSIGN(this_upd_seqno, seq_num_zero);
	promote_uupd_to_tupd = hasht_update_found = normal_update_found = FALSE;
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
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		assert(IS_REPLICATED(rectype));
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		if (IS_ZTRIG(rectype) || (IS_ZTWORM(rectype) && !secondary_side_trigger_support))
		{	/* $ZTWORMHOLE jnl record cannot be handled by secondary which does not support triggers so skip converting.
			 * If this is a TUPD rectype (actually JRT_TZTWORM/JRT_TZTRIG) then the next UUPD has to be promoted to a
			 * TUPD type to account for the balance in TUPD and TCOM records
			 */
			assert((cb == cstart) && (jb == jstart)); /* No conversions yet */
			if (IS_TUPD(rectype))
			{
				assert((JRT_TZTWORM == rectype) || (JRT_TZTRIG == rectype));
				promote_uupd_to_tupd = TRUE;
			}
		} else
		{
			if (IS_ZTP(rectype))
				GTMASSERT;	/* ZTP not supported */
			conv_reclen = prefix->forwptr;
			BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
			if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
			{
				assert((cb == cstart) && (jb == jstart));
				DEBUG_ONLY(
					if (!IS_ZTRIG(rectype))
						non_ztrig_rec_found = TRUE;
				)
				GET_JREC_UPD_TYPE(jb + FIXED_UPD_RECLEN, trigupd_type);
				if (secondary_side_trigger_support)
				{
					if (!normal_update_found)
						normal_update_found = (HASHT_JREC != trigupd_type);
					if (NON_REPLIC_JREC_TRIG == trigupd_type)
					{
						if (IS_TUPD(rectype))
							promote_uupd_to_tupd = TRUE;
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
					jb = jb + reclen;
					jlen -= reclen;
					continue;
				}
				memcpy(cb, jb, conv_reclen);
				mumps_node_ptr = (cb + FIXED_UPD_RECLEN);
				if ((HASHT_JREC == trigupd_type) && secondary_side_trigger_support)
				{
					if (!hasht_update_found)
						hasht_update_found = TRUE;
					/* Since the secondary side is running on an older trigger-supporting version than the
					 * primary, the ^#t("GBL","#LABEL") value needs to be adjusted to have the value that the
					 * secondary will understand. Not doing so will cause the secondary to issue TRIGDEFBAD
					 * error.
					 */
					keystr = (jnl_string *)mumps_node_ptr;
					keyend = &keystr->text[keystr->length - 1];
					assert('\0' == *keyend); /* we better have a null terminator at the end of the key */
					if (!MEMCMP_LIT((keyend - LITERAL_HASHLABEL_LEN), LITERAL_HASHLABEL))
					{	/* ^#t("GBL","#LABEL") found. For details on the update record layout, see the
						 * comment in upgrd_hasht_xecute_string
						 */
						ptr = keyend + SIZEOF(mstr_len_t) + 1; /* '+ 1' to account for '- 1' done above */
						assert(0 == MEMCMP_LIT(ptr, HASHT_GBL_CURLABEL));
						assert(STR_LIT_LEN(HASHT_GBL_CURLABEL) == STR_LIT_LEN(V19_HASHT_GBL_LABEL));
						MEMCPY_LIT(ptr, V19_HASHT_GBL_LABEL);
					} else if (0x80 == (unsigned char)(*(keyend - 1)))
					{	/* last subscript is zero. Check if the preceding subscript is "XECUTE" */
						ptr = keyend - 2; /* -1 for 0x80 and -1 for null terminator */
						if (!MEMCMP_LIT((ptr - LITERAL_XECUTE_LEN), LITERAL_XECUTE))
						{	/* preceding subscript is "XECUTE". Ensure the preceding character is \0 */
							ptr -= (LITERAL_XECUTE_LEN + 1); /* +1 for the leading \0 */
							assert(STR_SUB_PREFIX == ((unsigned char)(*ptr)));
							if (STR_SUB_PREFIX == ((unsigned char)(*ptr)))
							{	/* found ^#t("GBL",1,"XECUTE",0) which V5.4-001 secondary does
								 * NOT understand
								 */
								repl_errno = EREPL_INTLFILTER_MULTILINEXECUTE;
								status = -1;
								break;
							}
						}
					}
				}
				if (!secondary_side_trigger_support)
					((jnl_string *)mumps_node_ptr)->nodeflags = 0;
				else if (hasht_update_found && normal_update_found)
				{	/* A mix of ^#t updates and normal updates. Secondary (V5.4-000 to V5.4-001) does not
					 * understand mix of such updates. Set error status and break. Replication cannot
					 * continue unless $ztrigger() in TP (along with other updates) is eliminated.
					 */
					repl_errno = EREPL_INTLFILTER_SECNODZTRIGINTP;
					status = -1;
					break;
				}
				NULLSUBSC_TRANSFORM_IF_NEEDED(cb + FIXED_UPD_RECLEN);
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
				tcom_num++;
				assert((cb == cstart) && (jb == jstart));
				if (tcom_num > tupd_num)
				{
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
		}
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status));
	if ((-1 != status) && (cb == conv_buff))
	{	/* No conversion happened. Currently this is possible if
		 * (a) All the records are ^#t.
		 * (b) If the only records in a transaction are ZTRIG records and a TCOM record.
		 * In both the above cases we need to send a NULL record instead.
		 */
		assert((HASHT_JREC == trigupd_type) || (FALSE == non_ztrig_rec_found));
		GTMTRIG_ONLY(
			if (!(TREF(replgbl)).trig_replic_suspect_seqno)
				(TREF(replgbl)).trig_replic_suspect_seqno = this_upd_seqno;
		)
		prefix = (jrec_prefix *)(cb);
		if (NULL_RECLEN > conv_bufsiz)
		{
			repl_errno = EREPL_INTLFILTER_NOSPC;
			status = -1;
		} else
		{	/* Side-effect: cb is incremented by NULL_RECLEN */
			INITIALIZE_NULL_RECORD(prefix, cb, this_upd_seqno, FALSE);
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

/* Same version filters are needed for the following reasons:
 * (a) If null-subscript collation is different between the primary and the secondary
 * (b) If the secondary side does NOT support triggers, then:
 *	^#t records should be skipped and a NULL record should be sent instead
 *	ZTWORM and ZTRIG records should be skipped
 *	nodeflags if set, should be reset to zero
 */
int jnl_v21TOv21(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd;
	DEBUG_ONLY(boolean_t	non_ztrig_rec_found = FALSE;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	QWASSIGN(this_upd_seqno, seq_num_zero);
	promote_uupd_to_tupd = FALSE;
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
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		assert(IS_REPLICATED(rectype));
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		if ((!IS_ZTWORM(rectype) && !IS_ZTRIG(rectype)) || secondary_side_trigger_support)
		{
			conv_reclen = prefix->forwptr ;
			BREAK_IF_NOSPC((cb - conv_buff + conv_reclen), conv_bufsiz, status);	/* check for available space */
			if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
			{
				assert((jb == jstart) && (cb == cstart));
				DEBUG_ONLY(
					if (!IS_ZTRIG(rectype))
						non_ztrig_rec_found = TRUE;
				)
				GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
				if (secondary_side_trigger_support)
				{
					if (NON_REPLIC_JREC_TRIG == trigupd_type)
					{
						if (IS_TUPD(rectype))
							promote_uupd_to_tupd = TRUE;
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
					jb = jb + reclen;
					jlen -= reclen;
					continue;
				}
				memcpy(cb, jb, conv_reclen);
				mumps_node_ptr = cb + FIXED_UPD_RECLEN;
				if (!secondary_side_trigger_support)
					((jnl_string *)mumps_node_ptr)->nodeflags = 0;
				NULLSUBSC_TRANSFORM_IF_NEEDED(mumps_node_ptr + SIZEOF(jnl_str_len_t));
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
				{
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
		} else
		{	/* $ZTWORMHOLE jnl record cannot be handled by secondary which does not support triggers so skip converting.
			 * If this is a TUPD rectype (actually JRT_TZTWORM/JRT_TZTRIG) then the next UUPD has to be promoted to a
			 * TUPD type to account for the balance in TUPD and TCOM records
			 */
			assert((cb == cstart) && (jb == jstart)); /* No conversions yet */
			if (IS_TUPD(rectype))
			{
				assert((JRT_TZTWORM == rectype) || (JRT_TZTRIG == rectype));
				promote_uupd_to_tupd = TRUE;
			}
		}
		jb = jb + reclen;
		jlen -= reclen;
	}
	assert((0 == jlen) || (-1 == status) || (HASHT_JREC == trigupd_type));
	assert((jb == (jnl_buff + *jnl_len)) || (-1 == status) || (HASHT_JREC == trigupd_type));
	if ((-1 != status) && (cb == conv_buff))
	{	/* No conversion happened. Currently this is possible if
		 * (a) All the records are ^#t.
		 * (b) If the only records in a transaction are ZTRIG records and a TCOM record.
		 * In both the above cases we need to send a NULL record instead.
		 */
		assert(!secondary_side_trigger_support
			&& ((HASHT_JREC == trigupd_type) || (FALSE == non_ztrig_rec_found)));
		GTMTRIG_ONLY(
			if (!(TREF(replgbl)).trig_replic_suspect_seqno)
				(TREF(replgbl)).trig_replic_suspect_seqno = this_upd_seqno;
		)
		prefix = (jrec_prefix *)(cb);
		if (NULL_RECLEN > conv_bufsiz)
		{
			repl_errno = EREPL_INTLFILTER_NOSPC;
			status = -1;
		} else
		{
			/* Side-effect: cb is incremented by NULL_RECLEN */
			INITIALIZE_NULL_RECORD(prefix, cb, this_upd_seqno, FALSE);
		}
	}
	*conv_len = (uint4)(cb - conv_buff);
	assert((0 < *conv_len) || (-1 == status));
	DEBUG_ONLY(
		if (-1 != status)
			DBG_CHECK_IF_CONVBUFF_VALID(conv_buff, *conv_len);
	)
	return(status);
}
