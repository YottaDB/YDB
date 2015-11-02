/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#ifdef GTM_TRIGGER
#include "rtnhdr.h"	/* for rtn_tabent in gv_trigger.h */
#include "gv_trigger.h"	/* For HASHT* macros */
#endif

#ifndef GTM_TRIGGER
#	define	GTM_TRIGGER	/* enable GTM_TRIGGER temporarily to define all macros (e.g. IS_ZTWORM) even for VMS */
#	include "jnl_typedef.h"
#	undef	GTM_TRIGGER
#endif

#define V15_NULL_RECLEN		SIZEOF(v15_jrec_prefix) + SIZEOF(seq_num) + SIZEOF(uint4) + SIZEOF(jrec_suffix)

#define INITIALIZE_V19_UPDATE_NUM(cstart, cb, jstart, jb, tset_num, update_num, rectype)	\
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
#define INITIALIZE_V19_JRT_SET_KILL_ZKILL(cstart, cb, jstart, jb, tail_minus_suffix_len, from_v15)			\
{															\
	uint4			nodelen;										\
	unsigned char		*ptr;											\
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
	if (null_subs_xform)												\
	{														\
		ptr = cb + SIZEOF(jnl_str_len_t);									\
		if (from_v15)												\
		{	 /* Prior to V16, GT.M supports only GTM NULL collation */					\
			assert(GTMNULL_TO_STDNULL_COLL == null_subs_xform);						\
			GTM2STDNULLCOLL(ptr, nodelen);									\
		} else													\
		{	/* Check whether null subscripts transformation is needed */					\
			if (STDNULL_TO_GTMNULL_COLL == null_subs_xform)							\
			{												\
				STD2GTMNULLCOLL(ptr, *((jnl_str_len_t *)(cb + FIXED_UPD_RECLEN)));			\
			} else												\
			{												\
				GTM2STDNULLCOLL(ptr, *((jnl_str_len_t *)(cb + FIXED_UPD_RECLEN)));			\
			}												\
		}													\
	}														\
	jb += tail_minus_suffix_len;											\
	cb += tail_minus_suffix_len;											\
}
#define INITIALIZE_V15_V17_JRT_SET_KILL_ZKILL(cstart, cb, jstart, jb, align_fill_size, trigupd_type, to_v15)	\
{														\
	GBLREF boolean_t	trig_replic_warning_issued;							\
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
	 * set to a non-zero value. If so they need to be cleared as they are v19 format specific.		\
	 * Instead of checking, clear it unconditionally.							\
	 */													\
	((jnl_string *)cb)->nodeflags = 0;									\
	GET_JREC_UPD_TYPE(jb, trigupd_type);									\
	/* Check whether null subscripts transformation is needed */						\
	if (null_subs_xform)											\
	{													\
		ptr = cb + SIZEOF(jnl_str_len_t);								\
		if (to_v15)											\
		{												\
			/* Prior to V16, GT.M supports only GTM NULL collation */				\
			assert(STDNULL_TO_GTMNULL_COLL == null_subs_xform);					\
			STD2GTMNULLCOLL(ptr, *((jnl_str_len_t *)cb));						\
		} else												\
		{												\
			if (STDNULL_TO_GTMNULL_COLL == null_subs_xform)						\
			{											\
				STD2GTMNULLCOLL(ptr, *((jnl_str_len_t *)(cb)));					\
			} else											\
			{											\
				GTM2STDNULLCOLL(ptr, *((jnl_str_len_t *)(cb)));					\
			}											\
		}												\
	}													\
	jb += tail_minus_suffix_len;										\
	cb += tail_minus_suffix_len;										\
	if (0 != align_fill_size)										\
	{													\
		(*(uint4 *)cb) = 0;										\
		cb += SIZEOF(uint4);										\
	}													\
}
#define INITIALIZE_V19_JRT_TCOM(cstart, cb, jstart, jb, tcom_num, tset_num, update_num)				\
{														\
	uint4		num_participants;									\
	char		tmp_jnl_tid[TID_STR_SIZE];								\
														\
	/* We better have initialized the "prefix" and "token_seq" */						\
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
/* Assumes that the latest filter version is V19. */
#define INITIALIZE_V15_V17_JRT_TCOM(cstart, cb, jstart, jb)							\
{														\
	uint4		num_participants;									\
														\
	assert((jb - jstart) == OFFSETOF(struct_jrec_tcom, filler_short));					\
	/* Skip the "filler_short" in jb */									\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_tcom *)NULL)->filler_short));			\
	jb += SIZEOF(unsigned short);										\
	assert((jb - jstart) == OFFSETOF(struct_jrec_tcom, num_participants));					\
	assert(SIZEOF(unsigned short) == SIZEOF(((struct_jrec_tcom *)NULL)->num_participants));			\
	/* Take a copy of the num_participants field from V19's TCOM record */					\
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
/* For V19 to V15 or V17, the conv_reclen calculation is not straightforward as V19 journal update records
 * has additional fields -- update_num, filler_short and num_participants.
 */
#define GET_V19_JRT_SET_KILL_ZKILL_CONV_RECLEN(dst_fmt_prefix_len, reclen, conv_reclen, align_fill_size)		\
{															\
	uint4		mumps_node_length, tmp_conv_reclen;								\
															\
	mumps_node_length = (reclen - FIXED_UPD_RECLEN - JREC_SUFFIX_SIZE);						\
	tmp_conv_reclen = (dst_fmt_prefix_len + SIZEOF(token_seq_t) + mumps_node_length + JREC_SUFFIX_SIZE);		\
	conv_reclen = ROUND_UP(tmp_conv_reclen, JNL_REC_START_BNDRY);							\
	align_fill_size = (conv_reclen - tmp_conv_reclen);								\
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
#ifdef GTM_TRIGGER
#define INITIALIZE_NULL_RECORD(cb, conv_bufsiz, status, seqno, to_v15)	\
{									\
	v15_jrec_prefix		*v15_prefix_ptr;			\
	jrec_prefix		*prefix;				\
	jrec_suffix		*suffix;				\
									\
	/* Ensure we have enough space for NULL record in the 		\
	 * conversion buffer */						\
	if (V15_NULL_RECLEN > conv_bufsiz)				\
	{								\
		repl_errno = EREPL_INTLFILTER_NOSPC;			\
		assert(FALSE);						\
		status = -1;						\
	}								\
	if (to_v15)							\
	{								\
		v15_prefix_ptr = (v15_jrec_prefix *)(cb);		\
		v15_prefix_ptr->jrec_type = JRT_NULL;			\
		v15_prefix_ptr->forwptr = V15_NULL_RECLEN;		\
		cb += SIZEOF(v15_jrec_prefix);				\
	} else								\
	{								\
		prefix = (jrec_prefix *)(cb);				\
		prefix->jrec_type = JRT_NULL;				\
		prefix->forwptr = NULL_RECLEN;				\
		cb += JREC_PREFIX_SIZE;					\
	}								\
	*(seq_num *)(cb) = seqno;					\
	cb += SIZEOF(seq_num);						\
	/* Skip the filler */						\
	cb += SIZEOF(uint4);						\
	suffix = (jrec_suffix *)(cb);					\
	suffix->backptr = to_v15 ? V15_NULL_RECLEN : NULL_RECLEN;	\
	suffix->suffix_code = JNL_REC_SUFFIX_CODE;			\
	cb += SIZEOF(jrec_suffix);					\
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
#else
#define INITIALIZE_NULL_RECORD(cb, conv_bufsiz, status, seqno, to_v15)
#define GET_JREC_UPD_TYPE(mumps_node_ptr, trigupd_type)
#endif

enum
{
	NO_TRIG_JREC = 0,	/* Neither #t global nor triggered update nor an update that should NOT be replicated */
	HASHT_JREC,		/* #t global found in the journal record */
	NON_REPLIC_JREC_TRIG	/* This update was done inside of a trigger */
};

GBLDEF	int	jnl2filterfmt[REPL_JNL_MAX + 1] =
{
	REPL_FILTER_V15,	/* filter version for REPL_JNL_V15. GT.M V4.4-002 through V4.4-004* so supported */
	REPL_FILTER_VNONE,	/* filter version for REPL_JNL_V16. GT.M V5.0-FT01 so not supported anymore */
	REPL_FILTER_V17,	/* filter version for REPL_JNL_V17 */
	REPL_FILTER_V17,	/* filter version for REPL_JNL_V18 */
	REPL_FILTER_V19,	/* filter version for REPL_JNL_V19 */
	-1,			/* filter version for REPL_JNL_MAX */
};

GBLDEF	intlfltr_t repl_filter_cur2old[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	IF_19TO15,
	IF_NONE,
	IF_19TO17,
	IF_19TO17,
	IF_19TO19
};

GBLDEF	intlfltr_t repl_filter_old2cur[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1] =
{
	IF_15TO19,
	IF_NONE,
	IF_17TO19,
	IF_17TO19,
	IF_19TO19
};

GBLREF	unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char	jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	seq_num		seq_num_zero, seq_num_one;
GBLREF	int4		gv_keysize;
GBLREF	gv_key  	*gv_currkey, *gv_altkey; /* for jnl_extr_init() */
GBLREF	bool    	transform; /* for jnl_extr_init() */
GBLREF	boolean_t	null_subs_xform;
GBLREF	boolean_t	primary_side_trigger_support;
GBLREF	boolean_t	secondary_side_trigger_support;
GBLREF	uchar_ptr_t	repl_filter_buff;
GBLREF	int		repl_filter_bufsiz;
GBLREF	unsigned char	jnl_ver, remote_jnl_ver;
GBLREF	boolean_t	is_src_server;
GBLREF	boolean_t	is_rcvr_server;
GBLREF	seq_num		trig_replic_suspect_seqno;

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
	/* Should be a non-filter related function. But for now,... Needs GBLREFs gv_currkey and transform */
	transform = FALSE;      /* to avoid the assert in mval2subsc() */
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
	assert(JNL_VER_EARLIEST_REPL <= remote_jnl_ver);
	if (JNL_VER_EARLIEST_REPL > remote_jnl_ver)
		rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Dual/Multi site replication not supported between these two GT.M versions"));
}

/* The following are the functions that convert one jnl format to another.
 * The only replicated records we expect to see here are *SET* or *KILL* or TCOM or NULL records.
 * These fall under the following 3 structure types each of which is described for the different jnl formats we handle.
 *
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
 */

/* Convert a transaction from jnl version 15 (V4.4-002 through V4.4-004) to 19 (V5.4-000 onwards).
 * Compared to V15 prefix, V19 prefix size is 8 bytes more (tn field is 8 bytes instead of 4 bytes
 * 	and a new field checksum of 4 bytes).
 * Also 4-byte update_num is new to V19 struct_jrec_upd. So total of 12 more bytes. Rounded up
 * 	to 8-bytes (every jnl record is 8-byte aligned), this means we need to have 16 more bytes in
 * 	the conversion buffer for SET/KILL type of records and 8 more bytes for TCOM/NULL type of records.
 * Reformat accordingly.
 */
int jnl_v15TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tail_minus_suffix_len;
	jrec_prefix 		*prefix;
	v15_jrec_prefix		*v15_jrec_prefix_ptr;
	boolean_t		is_set_kill_zkill;
	static uint4		update_num, tset_num, tcom_num;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
	while (SIZEOF(v15_jrec_prefix) <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		v15_jrec_prefix_ptr = (v15_jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)v15_jrec_prefix_ptr->jrec_type;
		cstart = cb;
		jstart = jb;
		if (0 == (reclen = v15_jrec_prefix_ptr->forwptr))
		{
			repl_errno = EREPL_INTLFILTER_BADREC;
			assert(FALSE);
			status = -1;
			break;
		}
		if (reclen > jlen)
		{
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			assert(FALSE);
			status = -1;
			break;
		}
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V15 >= rectype);
		is_set_kill_zkill = IS_SET_KILL_ZKILL(rectype);
		assert(is_set_kill_zkill || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
		assert(v15_jrec_prefix_ptr->forwptr > SIZEOF(v15_jrec_prefix));
		conv_reclen = v15_jrec_prefix_ptr->forwptr + (is_set_kill_zkill ? 16 : 8);
		if (cb - conv_buff + conv_reclen > conv_bufsiz)
		{
			repl_errno = EREPL_INTLFILTER_NOSPC;
			status = -1;
			break;
		}
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
		if (is_set_kill_zkill)
		{	/* Initialize "update_num" member */
			INITIALIZE_V19_UPDATE_NUM(cstart, cb, jstart, jb, tset_num, update_num, rectype);
			/* Initialize "mumps_node" member */
			INITIALIZE_V19_JRT_SET_KILL_ZKILL(cstart, cb, jstart, jb, tail_minus_suffix_len, TRUE);
		} else if (JRT_TCOM == rectype)
		{	/* side-effect: cb and jb pointers incremented */
			assert((jb - jstart) == (SIZEOF(v15_jrec_prefix) + SIZEOF(token_seq_t)));
			INITIALIZE_V19_JRT_TCOM(cstart, cb, jstart, jb, tcom_num, tset_num, update_num);
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
	if ((-1 != status) && (0 != jlen))
	{
		repl_errno = EREPL_INTLFILTER_INCMPLREC;
		assert(FALSE);
		status = -1;
	}
	assert(0 == jlen || -1 == status);
	*jnl_len = (uint4)(jb - jnl_buff);
	*conv_len =(uint4)(cb - conv_buff);
	return(status);
}

/* Convert a transaction from jnl version 17 (V5.0-000 through V5.3-004A) to 19 (V5.4-000 onwards)
 * V17 and V19 prefix are both the same size.
 * TCOM record has two 2-byte participant fields (in V19) instead of one 4-byte participant field (in V17).
 * 	So no change of size there.
 * NULL record has no change at all between V17 and V19.
 * SET/KILL records have a new 4-byte update_num in V19 struct_jrec_upd. Rounded up to 8-bytes (every jnl record
 * 	is 8-byte aligned), this means we need to have 8 more bytes in the conversion buffer for SET/KILL type of records
 * 	and no extra bytes for TCOM/NULL type of records.
 * Also null subscripts transformation may be needed.
 * Reformat accordingly.
 */
int jnl_v17TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen, t_len;
	uint4			jlen, tail_minus_suffix_len;
	jrec_prefix 		*prefix;
	boolean_t		is_set_kill_zkill;
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
		cstart = cb;
		jstart = jb;
   		if (0 == (reclen = prefix->forwptr))
		{
			repl_errno = EREPL_INTLFILTER_BADREC;
			assert(FALSE);
			status = -1;
			break;
		}
		if (reclen > jlen)
		{
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			assert(FALSE);
			status = -1;
			break;
		}
		assert(IS_REPLICATED(rectype));
		assert(JRT_MAX_V17 >= rectype);
		is_set_kill_zkill = IS_SET_KILL_ZKILL(rectype);
		assert(is_set_kill_zkill || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
		assert(prefix->forwptr > SIZEOF(jrec_prefix));
		if (IS_ZTP(rectype))
			GTMASSERT;	/* ZTP not supported */
		conv_reclen = prefix->forwptr;
		if (is_set_kill_zkill)
			conv_reclen += 8; /* Due to update_num (4-byte) added in V19 and extra 4-byte for 8 byte alignment */
		if (cb - conv_buff + conv_reclen > conv_bufsiz)
		{
			repl_errno = EREPL_INTLFILTER_NOSPC;
			status = -1;
			break;
		}
		/* Initialize "prefix" and "token_seq" or "jnl_seqno" members */
		assert(OFFSETOF(struct_jrec_null, jnl_seqno) == OFFSETOF(struct_jrec_upd, token_seq));
		assert(OFFSETOF(struct_jrec_tcom, token_seq) == OFFSETOF(struct_jrec_upd, token_seq));
		assert(SIZEOF(token_seq_t) == SIZEOF(seq_num));
		t_len = (JREC_PREFIX_SIZE + SIZEOF(token_seq_t));
		memcpy(cb, jb, t_len);
		((jrec_prefix *)cb)->forwptr = conv_reclen; /* forwptr will be different between V17 and V19 due to update_num */
		cb += t_len;
		jb += t_len;
		tail_minus_suffix_len = (uint4)(jstart + reclen - jb - JREC_SUFFIX_SIZE);
		assert(0 < tail_minus_suffix_len);
		if (is_set_kill_zkill)
		{	/* Initialize "update_num" member */
			INITIALIZE_V19_UPDATE_NUM(cstart, cb, jstart, jb, tset_num, update_num, rectype);
			/* Initialize "mumps_node" member */
			INITIALIZE_V19_JRT_SET_KILL_ZKILL(cstart, cb, jstart, jb, tail_minus_suffix_len, FALSE);
		} else if (JRT_TCOM == rectype)
		{
			assert((jb - jstart) == (OFFSETOF(struct_jrec_tcom, token_seq) + SIZEOF(token_seq_t)));
			INITIALIZE_V19_JRT_TCOM(cstart, cb, jstart, jb, tcom_num, tset_num, update_num);
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
	if ((-1 != status) && (0 != jlen))
	{
		repl_errno = EREPL_INTLFILTER_INCMPLREC;
		assert(FALSE);
		status = -1;
	}
	assert(0 == jlen || -1 == status);
	*jnl_len = (uint4)(jb - jnl_buff);
	*conv_len = (uint4)(cb - conv_buff);
	return(status);
}

/* Convert a transaction from jnl version 19 (V5.4-000 onwards) to 15 (V4.4-002 through V4.4-004)
 * Null susbcripts & nodeflags transformation may be needed and need to check whether any global name length > 8
 */
int jnl_v19TOv15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tail_minus_suffix_len, align_fill_size;
	boolean_t		is_set_kill_zkill, promote_uupd_to_tupd;
	jrec_prefix 		*prefix;
	v15_jrec_prefix		*v15_jrec_prefix_ptr;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	assert(-1 != status);
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
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		if (0 == (reclen = prefix->forwptr))
		{
			repl_errno = EREPL_INTLFILTER_BADREC;
			assert(FALSE);
			status = -1;
			break;
		}
		if (reclen > jlen)
		{
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			assert(FALSE);
			status = -1;
			break;
		}
		assert(IS_REPLICATED(rectype));
		if (!IS_ZTWORM(rectype))
		{
			if (IS_ZTP(rectype))
				GTMASSERT;	/* ZTP not supported */
			is_set_kill_zkill = IS_SET_KILL_ZKILL(rectype);
			assert(is_set_kill_zkill || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
			if (is_set_kill_zkill)
			{	/* Compute conv_reclen and align_fill_size */
				GET_V19_JRT_SET_KILL_ZKILL_CONV_RECLEN((SIZEOF(v15_jrec_prefix)),
									reclen, conv_reclen, align_fill_size);
			} else
				conv_reclen = prefix->forwptr - 8;
			if (cb - conv_buff + conv_reclen > conv_bufsiz)
			{
				repl_errno = EREPL_INTLFILTER_NOSPC;
				status = -1;
				break;
			}
			/* Check whether the record contains long name */
			if (is_set_kill_zkill)
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
			v15_jrec_prefix_ptr = (v15_jrec_prefix *)cb;
			NON_GTMTRIG_ONLY(assert(!promote_uupd_to_tupd);)
			if (IS_UUPD(rectype) && promote_uupd_to_tupd)
			{
				/* The previous TUPD record was not replicated since it was a TZTWORM record and hence
				 * promote this UUPD to TUPD. Since the update process on the secondary will not care
				 * about the num_participants field in the TUPD records (it cares about the num_participants
				 * field only in the TCOM record), it is okay not to initialize the num_participants field
				 * of the promoted UUPD record
				 */
				v15_jrec_prefix_ptr->jrec_type = rectype - 1;
				promote_uupd_to_tupd = FALSE;
			}
			else
				v15_jrec_prefix_ptr->jrec_type = rectype;
			v15_jrec_prefix_ptr->forwptr = conv_reclen;
			v15_jrec_prefix_ptr->pini_addr = 0;
			v15_jrec_prefix_ptr->time = 0;
			v15_jrec_prefix_ptr->tn = 0;
			cb = cb + SIZEOF(v15_jrec_prefix);
			jb = jb + JREC_PREFIX_SIZE;
			/* Initialize "token_seq" or "jnl_seqno" */
			assert(SIZEOF(token_seq_t) == SIZEOF(seq_num));
			assert((jb - jstart) == OFFSETOF(struct_jrec_upd, token_seq));
			*((token_seq_t *)cb) = (*(token_seq_t *)jb);
			cb += SIZEOF(token_seq_t);
			jb += SIZEOF(token_seq_t);
			if (is_set_kill_zkill)
			{
				assert((jb - jstart) == OFFSETOF(struct_jrec_upd, update_num));
				/* side-effect: increments cb and jb and GTM Null Collation or Standard Null Collation applied */
				INITIALIZE_V15_V17_JRT_SET_KILL_ZKILL(cstart, cb, jstart, jb, align_fill_size, trigupd_type, TRUE);
				if (HASHT_JREC == trigupd_type)
				{	/* Journal record has a #t global. #t records are not replicated if the secondary does not
					 * support triggers. Instead a NULL record has to be sent across the pipe. So, no point
					 * continuing with the loop.
					 */
					break;
				}
			} else if (JRT_TCOM == rectype)
			{
				/* We better have initialized the "prefix" and "token_seq" */
				assert((cb - cstart) == (SIZEOF(v15_jrec_prefix) + SIZEOF(token_seq_t)));
				INITIALIZE_V15_V17_JRT_TCOM(cstart, cb, jstart, jb); /* side-effect: increments cb and jb */
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
			NON_GTMTRIG_ONLY(assert(FALSE);)
			assert(NO_TRIG_JREC == trigupd_type);
			jb = jstart + reclen;
			assert(cb == cstart);
			/* If this is a TUPD rectype (actually JRT_TZTWORM) then the next UUPD has to be promoted to a TUPD type
			 * to account for the balance in TUPD and TCOM records
			 */
			if (IS_TUPD(rectype))
				promote_uupd_to_tupd = TRUE;
		}
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	if ((-1 != status) && (0 != jlen) && (HASHT_JREC != trigupd_type))
	{
		repl_errno = EREPL_INTLFILTER_INCMPLREC;
		assert(FALSE);
		status = -1;
	}
	assert((jb == (jnl_buff + *jnl_len)) || (HASHT_JREC == trigupd_type));
	NON_GTMTRIG_ONLY(assert(NO_TRIG_JREC == trigupd_type);)
	if (HASHT_JREC == trigupd_type)
	{
		QWASSIGN(trig_replic_suspect_seqno, this_upd_seqno);
		/* Reset cb to conv_buff as we want a NULL record in the place of the entire #t transaction */
		cb = conv_buff;
		/* Side-effect: cb is incremented by V15_NULL_RECLEN */
		INITIALIZE_NULL_RECORD(cb, conv_bufsiz, status, this_upd_seqno, TRUE);
	} else
		*jnl_len = (uint4)(jb - jnl_buff);
	assert((0 == jlen) || (-1 == status) || (HASHT_JREC == trigupd_type));
	*conv_len = (uint4)(cb - conv_buff);
	return(status);
}

/* Convert a transaction from jnl version 19 (V5.4-000 onwards) to 17 (V5.0-000 through V5.3-004A)
 * V17 and V19 prefix are both the same size.
 * TCOM record has two 2-byte participant fields (in V19) instead of one 4-byte participant field (in V17).
 * 	So no change of size there. However, the 4-byte participant field of V17 TCOM record should be set
 *	to num_participants
 * NULL record has no change at all between V17 and V19.
 * SET/KILL records have a new 4-byte update_num in V19 struct_jrec_upd. Rounded up to 8-bytes (every jnl record
 	is 8-byte aligned). While converting to V17 format, we need to ignore the update_num field.
 * Null susbcripts & nodeflags transformation may be needed.
 */
int jnl_v19TOv17(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, t_len, tail_minus_suffix_len, align_fill_size;
	boolean_t		is_set_kill_zkill, promote_uupd_to_tupd;
	jrec_prefix 		*prefix;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;

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
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		if (0 == (reclen = prefix->forwptr))
		{
			repl_errno = EREPL_INTLFILTER_BADREC;
			assert(FALSE);
			status = -1;
			break;
		}
		if (reclen > jlen)
		{
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			assert(FALSE);
			status = -1;
			break;
		}
		assert(IS_REPLICATED(rectype));
		if (!IS_ZTWORM(rectype))
		{
			if (IS_ZTP(rectype))
				GTMASSERT;	/* ZTP not supported */
			is_set_kill_zkill = IS_SET_KILL_ZKILL(rectype);
			assert(is_set_kill_zkill || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
			conv_reclen = prefix->forwptr;
			if (is_set_kill_zkill)
			{	 /* Compute conv_reclen and align_fill_size */
				GET_V19_JRT_SET_KILL_ZKILL_CONV_RECLEN((SIZEOF(jrec_prefix)), reclen, conv_reclen, align_fill_size);
			}
			if (cb - conv_buff + conv_reclen > conv_bufsiz)
			{
				repl_errno = EREPL_INTLFILTER_NOSPC;
				status = -1;
				break;
			}
			/* Initialize "prefix" and "token_seq" or "jnl_seqno" members */
			assert(OFFSETOF(struct_jrec_null, jnl_seqno) == OFFSETOF(struct_jrec_upd, token_seq));
			assert(OFFSETOF(struct_jrec_tcom, token_seq) == OFFSETOF(struct_jrec_upd, token_seq));
			assert(SIZEOF(token_seq_t) == SIZEOF(seq_num));
			t_len = (JREC_PREFIX_SIZE + SIZEOF(token_seq_t));
			memcpy(cb, jb, t_len);
			((jrec_prefix *)cb)->forwptr = conv_reclen; /* forwptr will be different between V17 and V19
								     * due to update_num */
			NON_GTMTRIG_ONLY(assert(!promote_uupd_to_tupd);)
			if (IS_UUPD(rectype) && promote_uupd_to_tupd)
			{
				/* The previous TUPD record was not replicated since it was a TZTWORM record and hence
				 * promote this UUPD to TUPD. Since the update process on the secondary will not care
				 * about the num_participants field in the TUPD records (it cares about the num_participants
				 * field only in the TCOM record), it is okay not to initialize the num_participants field
				 * of the promoted UUPD record
				 */
				((jrec_prefix *)cb)->jrec_type--;
				promote_uupd_to_tupd = FALSE;
			}
			cb += t_len;
			jb += t_len;
			if (is_set_kill_zkill)
			{
				assert((cb - cstart) == (OFFSETOF(struct_jrec_upd, token_seq) + SIZEOF(token_seq_t)));
				/* side-effect: increments cb and jb and GTM Null Collation or Standard Null Collation applied */
				INITIALIZE_V15_V17_JRT_SET_KILL_ZKILL(cstart, cb, jstart, jb, align_fill_size, trigupd_type,
									FALSE);
				if (HASHT_JREC == trigupd_type)
				{	/* Journal record has a #t global. #t records are not replicated if the secondary does not
					 * support triggers. Instead a NULL record has to be sent across the pipe. So, no point
					 * continuing with the loop.
					 */
					break;
				}
			} else if (JRT_TCOM == rectype)
			{
				/* We better have initialized the "prefix" and "token_seq" */
				assert((cb - cstart) == (OFFSETOF(struct_jrec_tcom, token_seq) + SIZEOF(token_seq_t)));
				INITIALIZE_V15_V17_JRT_TCOM(cstart, cb, jstart, jb); /* side-effect: increments cb and jb */
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
			NON_GTMTRIG_ONLY(assert(FALSE);)
			jb = jstart + reclen;
			assert(cb == cstart);
			/* If this is a TUPD rectype (actually JRT_TZTWORM) then the next UUPD has to be promoted to a TUPD type
			 * to account for the balance in TUPD and TCOM records
			 */
			if (IS_TUPD(rectype))
				promote_uupd_to_tupd = TRUE;
		}
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	if ((-1 != status) && (0 != jlen) && (HASHT_JREC != trigupd_type))
	{
		repl_errno = EREPL_INTLFILTER_INCMPLREC;
		assert(FALSE);
		status = -1;
	}
	assert(jb == (jnl_buff + *jnl_len) || (HASHT_JREC == trigupd_type));
	NON_GTMTRIG_ONLY(assert(NO_TRIG_JREC == trigupd_type);)
	if (HASHT_JREC == trigupd_type)
	{
		QWASSIGN(trig_replic_suspect_seqno, this_upd_seqno);
		/* Reset cb to conv_buff as we want a NULL record in the place of the entire #t transaction */
		cb = conv_buff;
		/* Side-effect: cb is incremented by NULL_RECLEN */
		INITIALIZE_NULL_RECORD(cb, conv_bufsiz, status, this_upd_seqno, FALSE);
	} else
		*jnl_len = (uint4)(jb - jnl_buff);
	assert((0 == jlen) || (-1 == status) || (HASHT_JREC == trigupd_type));
	*conv_len = (uint4)(cb - conv_buff);
	return(status);
}

/* Convert a transaction from jnl version 19 (V5.4-000 onwards) to 19 (V5.4-000 onwards)
 * Currently we check following
 * 	a) whether null subscripts transformation is needed.
 * 	b) whether nodeflags & ZTWORMHOLE transformation is needed (in case primary and secondary differ in trigger support)
 */
int jnl_v19TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr;
	enum jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen, tcom_num = 0, tupd_num = 0;
	jrec_prefix 		*prefix;
	seq_num			this_upd_seqno;
	uint4 			trigupd_type = NO_TRIG_JREC;
	boolean_t		promote_uupd_to_tupd;
	DEBUG_ONLY(uint4	num_participants;)
	DEBUG_ONLY(uint4	clen;)

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	/* Assert that same version conversion (in this case v19 to v19) should always be done by source server.
	 * Prior to V19, this was being always done at the receiver server side.
	 */
	assert(is_src_server);
	assert(!is_rcvr_server);
	QWASSIGN(this_upd_seqno, seq_num_zero);
	promote_uupd_to_tupd = FALSE;
   	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
		if (QWEQ(this_upd_seqno, seq_num_zero))
			QWASSIGN(this_upd_seqno, GET_JNL_SEQNO(jb));
		if (0 == (reclen = prefix->forwptr))
		{
			repl_errno = EREPL_INTLFILTER_BADREC;
			assert(FALSE);
			status = -1;
			break;
		}
		if (reclen > jlen)
		{
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			assert(FALSE);
			status = -1;
			break;
		}
		assert(IS_REPLICATED(rectype));
		if (IS_TUPD(rectype))
			promote_uupd_to_tupd = FALSE;
		if (!IS_ZTWORM(rectype) || secondary_side_trigger_support)
		{
			if (IS_ZTP(rectype))
				GTMASSERT;	/* ZTP not supported */
			if (JRT_TCOM == rectype)
				tcom_num++;
			conv_reclen = prefix->forwptr ;
			if (cb - conv_buff + conv_reclen > conv_bufsiz)
			{
				repl_errno = EREPL_INTLFILTER_NOSPC;
				status = -1;
				break;
			}
			if (IS_SET_KILL_ZKILL_ZTWORM(rectype))
			{
				GET_JREC_UPD_TYPE((jb + FIXED_UPD_RECLEN), trigupd_type);
				NON_GTMTRIG_ONLY(assert(NO_TRIG_JREC == trigupd_type);)
				assert((HASHT_JREC != trigupd_type) || primary_side_trigger_support);
				if (!secondary_side_trigger_support && (HASHT_JREC == trigupd_type))
				{	/* Journal record has a #t global. #t records are not replicated. Instead a NULL record
					 * has to be sent across the pipe. So, no point continuing with the loop.
					 */
					break;
				} else if (secondary_side_trigger_support && (NON_REPLIC_JREC_TRIG == trigupd_type))
				{	/* This journal record corresponds to an update that happened inside of trigger and
					 * the secondary supports triggers. In that case, we don't want to replicate this
					 * update as we know the secondary side will have the same trigger definitions and
					 * will be invoking this update anyways. This way, the data size sent in the replication
					 * pipe is minimized.
					 */
					assert((jb == jstart) && (cb == cstart));
					/* Note down if this journal record was either TSET/TKILL/TZKILL. This is needed
					 * as we would be discarding a TSET record and possibly any future USET/UKILL/UZKILL
					 * could be discarded as they too happened inside trigger. The first USET/UKILL/UZKILL
					 * that did NOT happen inside trigger will be promoted to a TUPD type record to account
					 * for balance between TUPD and TCOM.
					 */
					if (IS_TUPD(rectype))
						promote_uupd_to_tupd = TRUE;
					jb = jb + reclen;
					jlen -= reclen;
					continue;
				}
				if (IS_TUPD(rectype))
					tupd_num++;
			} else if ((JRT_TCOM == rectype) && (tcom_num > tupd_num))
			{	/* This condition ensures that we always have a balance of TUPD and TCOM. If one or more regions
				 * had all their updates done inside of trigger (all of which will not be replicated) then we
				 * should have that many less TCOM records in the conversion buffer (replicated stream).
				 */
				jb = jb + reclen;
				jlen -= reclen;
				continue;
			}
			memcpy(cb, jb, conv_reclen);
			if (IS_SET_KILL_ZKILL(rectype))
			{
				/* If secondary does NOT support triggers and if bits 24-31 of "length" member (nodeflags field)
				 * of "mumps_node" field are set to a non-zero value, clear them as they are non-zero only
				 * on trigger-supported platforms. Instead of checking, clear it unconditionally.
				 */
				ptr = cb + FIXED_UPD_RECLEN;
				if (!secondary_side_trigger_support)
					((jnl_string *)ptr)->nodeflags = 0;
				if (null_subs_xform)
				{
					ptr = ptr + SIZEOF(jnl_str_len_t);
					if (STDNULL_TO_GTMNULL_COLL == null_subs_xform)
					{
						STD2GTMNULLCOLL(ptr, *((jnl_str_len_t *)(cb + FIXED_UPD_RECLEN)));
					} else
					{
						GTM2STDNULLCOLL(ptr, *((jnl_str_len_t *)(cb + FIXED_UPD_RECLEN)));
					}
				}
			} else if (JRT_TCOM == rectype)
			{
				assert(cb == cstart);
				((struct_jrec_tcom *)(cb))->num_participants = tupd_num;
			}
			NON_GTMTRIG_ONLY(assert(!promote_uupd_to_tupd);)
			if (IS_UUPD(rectype) && promote_uupd_to_tupd)
			{
				/* The previous TUPD record was not replicated since it was a TZTWORM record and hence
				 * promote this UUPD to TUPD. Since the update process on the secondary will not care
				 * about the num_participants field in the TUPD records (it cares about the num_participants
				 * field only in the TCOM record), it is okay not to initialize the num_participants field
				 * of the promoted UUPD record
				 */
				((jrec_prefix *)(cb))->jrec_type--;
				assert(IS_TUPD(((jrec_prefix *)(cb))->jrec_type));
				promote_uupd_to_tupd = FALSE;
				tupd_num++;
			}
			cb = cb + conv_reclen ;
			assert(cb == cstart + conv_reclen);
		} else
		{	/* $ZTWORMHOLE jnl record cannot be handled by secondary which does not support triggers
			 * so skip converting it */
			NON_GTMTRIG_ONLY(assert(FALSE);)
			if (IS_TUPD(rectype))
				promote_uupd_to_tupd = TRUE;
			assert(cb == cstart);
		}
		jb = jb + reclen;
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	if ((-1 != status) && (0 != jlen) && (HASHT_JREC != trigupd_type))
	{
		repl_errno = EREPL_INTLFILTER_INCMPLREC;
		assert(FALSE);
		status = -1;
	}
	assert((jb == (jnl_buff + *jnl_len)) || (HASHT_JREC == trigupd_type));
	NON_GTMTRIG_ONLY(assert(NO_TRIG_JREC == trigupd_type);)
	if ((HASHT_JREC == trigupd_type) && !secondary_side_trigger_support)
	{
		QWASSIGN(trig_replic_suspect_seqno, this_upd_seqno);
		/* Reset cb to conv_buff as we want a NULL record in the place of the entire #t transaction */
		cb = conv_buff;
		/* Side-effect: cb is incremented by NULL_RECLEN */
		INITIALIZE_NULL_RECORD(cb, conv_bufsiz, status, this_upd_seqno, FALSE);
	} else
		*jnl_len = (uint4)(jb - jnl_buff);
	assert((0 == jlen) || (-1 == status) || (HASHT_JREC == trigupd_type));
	*conv_len = (uint4)(cb - conv_buff);
	assert(0 != *conv_len);
#	ifdef DEBUG
	cb = conv_buff;
	clen = *conv_len;
	tcom_num = tupd_num = 0;
	num_participants = 0;
	while (JREC_PREFIX_SIZE <= clen)
	{
		prefix = (jrec_prefix *)(cb);
		rectype = prefix->jrec_type;
		reclen = prefix->forwptr;
		if (IS_TUPD(rectype))
			tupd_num++;
		else if (JRT_TCOM == rectype)
		{
			num_participants = ((struct_jrec_tcom *)(cb))->num_participants;
			tcom_num++;
		}
		clen -= reclen;
		cb += reclen;
	}
	assert(tupd_num == tcom_num);
	assert(tupd_num == num_participants);
#	endif
	return(status);
}
