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

#if defined(__MVS__) && !defined(_ISOC99_SOURCE)
#define _ISOC99_SOURCE
#endif

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
#include "iotcpdef.h"
#include "memcoherency.h"
#include "have_crit.h"			/* needed for ZLIB_UNCOMPRESS */
#include "deferred_signal_handler.h"	/* needed for ZLIB_UNCOMPRESS */
#include "gtm_zlib.h"
#include "wbox_test_init.h"
#ifdef GTM_TRIGGER
#include "repl_sort_tr_buff.h"
#endif
#include "replgbl.h"
#include "gtmio.h"
#include "repl_inst_dump.h"		/* for "repl_dump_histinfo" prototype */
#include "gv_trigger_common.h"
#include "anticipatory_freeze.h"

#define	GTM_ZLIB_UNCMP_ERR_STR		"error from zlib uncompress function "
#define	GTM_ZLIB_Z_MEM_ERROR_STR	"Out-of-memory " GTM_ZLIB_UNCMP_ERR_STR
#define	GTM_ZLIB_Z_BUF_ERROR_STR	"Insufficient output buffer " GTM_ZLIB_UNCMP_ERR_STR
#define	GTM_ZLIB_Z_DATA_ERROR_STR	"Input-data-incomplete-or-corrupt " GTM_ZLIB_UNCMP_ERR_STR
#define	GTM_ZLIB_UNCMPLEN_ERROR_STR	"Decompressed message data length %d is not equal to precompressed length %d "
#define	GTM_ZLIB_UNCMP_ERR_SEQNO_STR	"at seqno "INT8_FMT" "INT8_FMTX"\n"
#define	GTM_ZLIB_UNCMP_ERR_SOLVE_STR	"before sending REPL_CMP_SOLVE message\n"
#define	GTM_ZLIB_UNCMPTRANSITION_STR	"Defaulting to NO decompression\n"

#define RECVBUFF_REPLMSGLEN_FACTOR 		8

#define GTMRECV_WAIT_FOR_STARTJNLSEQNO		100 /* ms */

#define GTMRECV_WAIT_FOR_UPD_PROGRESS		100 /* ms */

/* By having different high and low watermarks, we can reduce the # of XOFF/XON exchanges */
#define RECVPOOL_HIGH_WATERMARK_PCTG		90	/* Send XOFF when %age of receive pool space occupied goes beyond this */
#define RECVPOOL_LOW_WATERMARK_PCTG		80	/* Send XON when %age of receive pool space occupied falls below this */
#define RECVPOOL_XON_TRIGGER_SIZE		(1 * 1024 * 1024) /* Keep the low water mark within this amount of high water mark
								   * so that we don't wait too long to send XON */

#define GTMRECV_XOFF_LOG_CNT			100

#define GTMRECV_HEARTBEAT_PERIOD		10	/* seconds, timer that goes off every this period is the time keeper for
							 * receiver server; used to reduce calls to time related systemc calls */

#define ONLN_RLBK_CMD_MAXLEN			1024
#define MUPIP_DIST_STR				"$gtm_dist/mupip "
#define	ONLN_RLBK_CMD				"journal "
#define ONLN_RLBK_VERBOSE			"-verbose "
#define	ONLN_RLBK_QUALIFIERS			"-online -rollback -backward \"*\" -fetchresync=" /* port# will be filled later */

#if defined(__hpux) && !defined(__hppa) || defined(_AIX)
#define KEEPALIVE_PROTO_LEVEL IPPROTO_TCP
#define KEEPALIVE_TIME		5
#define KEEPALIVE_INTVL		5
#define KEEPALIVE_PROBES	5
#elif defined(__linux__)
#define KEEPALIVE_PROTO_LEVEL SOL_TCP
#define KEEPALIVE_TIME		5
#define KEEPALIVE_INTVL		5
#define KEEPALIVE_PROBES	5
#endif

GBLDEF	repl_msg_ptr_t		gtmrecv_msgp;
GBLDEF	int			gtmrecv_max_repl_msglen;
GBLDEF	int			gtmrecv_sock_fd = FD_INVALID;
GBLDEF	boolean_t		repl_connection_reset = TRUE;
GBLDEF	boolean_t		gtmrecv_wait_for_jnl_seqno = FALSE;
GBLDEF	boolean_t		gtmrecv_bad_trans_sent = FALSE;
GBLDEF	boolean_t		gtmrecv_send_cmp2uncmp = FALSE;

GBLDEF	qw_num			repl_recv_data_recvd = 0;
GBLDEF	qw_num			repl_recv_data_processed = 0;
GBLDEF	qw_num			repl_recv_postfltr_data_procd = 0;
GBLDEF	qw_num			repl_recv_lastlog_data_recvd = 0;
GBLDEF	qw_num			repl_recv_lastlog_data_procd = 0;

GBLDEF	time_t			repl_recv_prev_log_time;
GBLDEF	time_t			repl_recv_this_log_time;
GBLDEF	volatile time_t		gtmrecv_now = 0;

STATICDEF uchar_ptr_t		gtmrecv_cmpmsgp;
STATICDEF int			gtmrecv_cur_cmpmsglen;
STATICDEF int			gtmrecv_max_repl_cmpmsglen;
STATICDEF uchar_ptr_t		gtmrecv_uncmpmsgp;
STATICDEF int			gtmrecv_max_repl_uncmpmsglen;
STATICDEF int			gtmrecv_repl_cmpmsglen;
STATICDEF int			gtmrecv_repl_uncmpmsglen;

STATICFNDCL	void	gtmrecv_repl_send_loop_error(int status, char *msgtypestr);
STATICFNDCL	int	repl_tr_endian_convert(unsigned char remote_jnl_ver, uchar_ptr_t jnl_buff, uint4 jnl_len);
STATICFNDCL	void	do_flow_control(uint4 write_pos);
STATICFNDCL	int	gtmrecv_est_conn(void);
STATICFNDCL	int	gtmrecv_start_onln_rlbk(void);
STATICFNDCL	void	prepare_recvpool_for_write(int datalen, int pre_filter_write_len);
STATICFNDCL	void	copy_to_recvpool(uchar_ptr_t databuff, int datalen);
STATICFNDCL	void	wait_for_updproc_to_clear_backlog(void);
STATICFNDCL	void	process_tr_buff(int msg_type);
STATICFNDCL	void	gtmrecv_updresync_histinfo_find_seqno(seq_num input_seqno, int4 strm_num, repl_histinfo *histinfo);
STATICFNDCL	void	gtmrecv_updresync_histinfo_get(int4 index, repl_histinfo *histinfo);
STATICFNDCL	void	gtmrecv_process_need_strminfo_msg(repl_needstrminfo_msg_ptr_t need_strminfo_msg);
STATICFNDCL	void	gtmrecv_process_need_histinfo_msg(repl_needhistinfo_msg_ptr_t need_histinfo_msg, repl_histinfo *histinfo);
STATICFNDCL	void	do_main_loop(boolean_t crash_restart);
STATICFNDCL	void	gtmrecv_heartbeat_timer(TID tid, int4 interval_len, int *interval_ptr);
STATICFNDCL	void	gtmrecv_main_loop(boolean_t crash_restart);

GBLREF	gtmrecv_options_t	gtmrecv_options;
GBLREF	int			gtmrecv_listen_sock_fd;
GBLREF	recvpool_addrs		recvpool;
GBLREF	boolean_t		gtmrecv_logstats;
GBLREF	int			gtmrecv_filter;
GBLREF	int			gtmrecv_log_fd;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	seq_num			seq_num_zero, seq_num_one, seq_num_minus_one;
GBLREF	unsigned char		*repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;
GBLREF	qw_num			trans_recvd_cnt, last_log_tr_recvd_cnt;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	mur_gbls_t		murgbl;
GBLREF	repl_conn_info_t	*this_side, *remote_side;
GBLREF	int4			strm_index;

error_def(ERR_INSNOTJOINED);
error_def(ERR_INSROLECHANGE);
error_def(ERR_INSUNKNOWN);
error_def(ERR_JNLNEWREC);
error_def(ERR_JNLRECFMT);
error_def(ERR_JNLSETDATA2LONG);
error_def(ERR_NOSUPPLSUPPL);
error_def(ERR_PRIMARYNOTROOT);
error_def(ERR_RCVRMANYSTRMS);
error_def(ERR_REPL2OLD);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLGBL2LONG);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_REPLINSTREAD);
error_def(ERR_REPLTRANS2BIG);
error_def(ERR_REPLXENDIANFAIL);
error_def(ERR_RESUMESTRMNUM);
error_def(ERR_REUSEINSTNAME);
error_def(ERR_SECNODZTRIGINTP);
error_def(ERR_SECONDAHEAD);
error_def(ERR_STRMNUMIS);
error_def(ERR_SUPRCVRNEEDSSUPSRC);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);
error_def(ERR_UPDSYNCINSTFILE);

typedef enum
{
	GTM_RECV_POOL,
	GTM_RECV_CMPBUFF
} gtmrecv_buff_t;

static	unsigned char	*buffp, *buff_start, *msgbuff, *filterbuff;
static	int		buff_unprocessed;
static	int		buffered_data_len;
static	int		max_recv_bufsiz;
static	int		data_len;
static	int		exp_data_len;
static	boolean_t	xoff_sent;
static	repl_msg_t	xon_msg, xoff_msg;
static	int		xoff_msg_log_cnt = 0;
static	long		recvpool_high_watermark, recvpool_low_watermark;
static	uint4		write_loc, write_wrap;
static	uint4		write_off;
static	double		time_elapsed;
static	int		recvpool_size;
static	int		heartbeat_period;
#ifdef REPL_CMP_SOLVE_TESTING
static	boolean_t	repl_cmp_solve_timer_set;
#endif

#define ISSUE_REPLCOMM_ERROR(REASON, SAVE_ERRNO)									\
{															\
	if (0 != SAVE_ERRNO)												\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_LIT(REASON), SAVE_ERRNO);\
	else														\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_LIT(REASON));		\
}

#define	GTMRECV_EXPAND_CMPBUFF_IF_NEEDED(cmpmsglen)			\
{									\
	int	lclcmpmsglen;						\
									\
	lclcmpmsglen = ROUND_UP2(cmpmsglen, REPL_MSG_ALIGN);		\
	if (lclcmpmsglen > gtmrecv_max_repl_cmpmsglen)			\
	{								\
		if (NULL != gtmrecv_cmpmsgp)				\
			free(gtmrecv_cmpmsgp);				\
		gtmrecv_cmpmsgp = (uchar_ptr_t)malloc(lclcmpmsglen);	\
		gtmrecv_max_repl_cmpmsglen = (lclcmpmsglen);		\
	}								\
	assert(0 == (gtmrecv_max_repl_cmpmsglen % REPL_MSG_ALIGN));	\
}

#define	GTMRECV_EXPAND_UNCMPBUFF_IF_NEEDED(uncmpmsglen)			\
{									\
	if (uncmpmsglen > gtmrecv_max_repl_uncmpmsglen)			\
	{								\
		if (NULL != gtmrecv_uncmpmsgp)				\
			free(gtmrecv_uncmpmsgp);			\
		gtmrecv_uncmpmsgp = (uchar_ptr_t)malloc(uncmpmsglen);	\
		gtmrecv_max_repl_uncmpmsglen = (uncmpmsglen);		\
	}								\
	assert(0 == (gtmrecv_max_repl_uncmpmsglen % REPL_MSG_ALIGN));	\
}

/* Set an alternate buffer as the target to hold the incoming compressed journal records.
 * This will then be uncompressed into yet another buffer from where the records will be
 * transferred to the receive pool one transaction at a time.
 */
#define	GTMRECV_SET_BUFF_TARGET_CMPBUFF(cmplen, uncmplen, curcmplen)	\
{									\
	GTMRECV_EXPAND_CMPBUFF_IF_NEEDED(cmplen);			\
	GTMRECV_EXPAND_UNCMPBUFF_IF_NEEDED(uncmplen);			\
	curcmplen = 0;							\
}

/* Wrap the "prepare_recvpool_for_write", "copy_to_recvpool", "do_flow_control" and "gtmrecv_poll_actions" functions in macros.
 * This is needed because every invocation of these functions (except for a few gtmrecv_poll_actions invocations) should check
 * a few global variables to determine if there was an error and in that case return from the caller.
 * All callers of these functions should use the macro and not invoke the function directly.
 * This avoids duplication of the error check logic.
 */
#define	PREPARE_RECVPOOL_FOR_WRITE(curdatalen, prefilterdatalen)		\
{										\
	prepare_recvpool_for_write(curdatalen, prefilterdatalen);		\
		/* could update "recvpool_ctl->write" and "write_loc" */	\
	if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)		\
		return;								\
}
#define	COPY_TO_RECVPOOL(ptr, datalen)						\
{										\
	copy_to_recvpool(ptr, datalen); /* uses and updates "write_loc" */	\
	if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)		\
		return;								\
}

#define	DO_FLOW_CONTROL(write_loc)						\
{										\
	do_flow_control(write_loc);						\
	if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)		\
		return;								\
}

#define	GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp)			\
{										\
	gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);		\
	if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)		\
		return;								\
}

/* The below macro is used (within this module) to check for errors (and issue appropriate message) after REPL_SEND_LOOP
 * returns.
 * NOTE: This macro, in its current form, cannot be used in all REPL_SEND_LOOP usages due to specific post-error processing
 * done in a few places. There is scope to:
 * (a) Improve the macro to adjust to post-error processing
 * (b) Provide similar macro to be used for REPL_RECV_LOOP usages
 */
#define	CHECK_REPL_SEND_LOOP_ERROR(status, msgstr)		\
{								\
	if (SS_NORMAL != status)				\
	{							\
		gtmrecv_repl_send_loop_error(status, msgstr);	\
		if (repl_connection_reset)			\
			return;					\
	}							\
}

#define GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED					\
{										\
	sgmnt_addrs	*repl_csa;						\
										\
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;		\
	assert(repl_csa->now_crit);						\
	assert(jnlpool.jnlpool_ctl == jnlpool_ctl);				\
	if (repl_csa->onln_rlbk_cycle != jnlpool_ctl->onln_rlbk_cycle)		\
	{									\
		SYNC_ONLN_RLBK_CYCLES;						\
		rel_lock(jnlpool.jnlpool_dummy_reg);				\
		gtmrecv_onln_rlbk_clnup();					\
		if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)	\
			return;							\
	}									\
}

/* For cross-endian conversion to happen on the receiving side, the receiver must understand the layout of the journal
 * records. To keep the endian conversion logic on both primary and secondary simple, the following scheme is used:
 * (a) If primary < secondary, endian conversion will happen on primary.
 * (b) If primary >= secondary, primary will apply internal filters to convert the records to secondary's format. The
 *     secondary on receiving them will do the necessary endian conversion before letting the update process see them.
 *
 * However, the above logic will cause the older versions (< V5.4-002) to NOT replicate to V5.4-002 as the endian-conversion
 * by-primary is introduced only from V5.4-002 and above. Hence, allow secondary to do endian conversion for this special
 * case when the primary is a GT.M version running V5.3-003 (V18_JNL_VER) to V5.4-001 (V20_JNL_VER). The lower limit is
 * chosen to be V5.3-003 since that was the first version where cross-endian conversion was supported.
 *
 * There is one other exception. V5.5 source server (V22_JNL_VER) had a bug wherein a history record is endian-converted
 * when the replication is NOT cross-endian and vice versa. In either case, do an endian conversion of the history record.
 *
 * The below macro takes all the above conditions into consideration to determine if the receiver server needs to do endian
 * converison or not.
 */
#define ENDIAN_CONVERSION_NEEDED(IS_NEW_HISTREC, THIS_JNL_VER, REMOTE_JNL_VER, X_ENDIAN)			\
	((IS_NEW_HISTREC && (V22_JNL_VER == REMOTE_JNL_VER)) 							\
		|| (X_ENDIAN && ((REMOTE_JNL_VER >= THIS_JNL_VER) || (V21_JNL_VER > REMOTE_JNL_VER))))

STATICFNDEF void gtmrecv_repl_send_loop_error(int status, char *msgtypestr)
{
	char		print_msg[1024];

	assert((EREPL_SEND == repl_errno) || (EREPL_SELECT == repl_errno));
	if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
	{
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection got reset while sending %s message. Status = %d ; %s\n",
				msgtypestr, status, STRERROR(status));
		repl_connection_reset = TRUE;
		repl_close(&gtmrecv_sock_fd);
		SNPRINTF(print_msg, SIZEOF(print_msg), "Closing connection on receiver side\n");
		repl_log(gtmrecv_log_fp, TRUE, TRUE, print_msg);
		gtm_event_log(GTM_EVENT_LOG_ARGC, "MUPIP", "ERR_REPLWARN", print_msg);
		return;
	} else if (EREPL_SEND == repl_errno)
	{
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending %s message. Error in send : %s",
				msgtypestr, STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	} else if (EREPL_SELECT == repl_errno)
	{
		SNPRINTF(print_msg, SIZEOF(print_msg), "Error sending %s message. Error in select : %s",
				msgtypestr, STRERROR(status));
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
}

/* convert endianness of transaction */
STATICFNDEF int repl_tr_endian_convert(unsigned char remote_jnl_ver, uchar_ptr_t jnl_buff, uint4 jnl_len)
{
	unsigned char			*jb, *jstart, *ptr;
	enum jnl_record_type		rectype;
	int				status, reclen;
	uint4				jlen;
	jrec_prefix			*prefix;
	jnl_record			*rec;
	jnl_string			*keystr;
	mstr_len_t			*vallen_ptr;
	mstr_len_t			temp_val;
	repl_old_triple_jnl_ptr_t	oldtriple;
	repl_histinfo			*histinfo;
	jnl_str_len_t			nodeflags_keylen;
	uint4				update_num, num_participants_4bytes;
	unsigned short			num_participants_2bytes;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	assert((remote_jnl_ver >= this_side->jnl_ver) || (V21_JNL_VER > remote_jnl_ver) || (V22_JNL_VER == remote_jnl_ver));
	jb = jnl_buff;
	status = SS_NORMAL;
	jlen = jnl_len;
	assert(0 == ((UINTPTR_T)jb % SIZEOF(UINTPTR_T)));
	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % SIZEOF(UINTPTR_T)));
		rec = (jnl_record *) jb;
		rectype = (enum jnl_record_type)rec->prefix.jrec_type;
		reclen = rec->prefix.forwptr = GTM_BYTESWAP_24(rec->prefix.forwptr);
		if (!IS_REPLICATED(rectype) || (0 == reclen) || (reclen > jlen))
		{	/* Bad OR Incomplete record */
			assert(FALSE);
			status = -1;
			break;
		}
		assert(!IS_ZTP(rectype));
		assert((JRT_HISTREC == rectype) || (JRT_TRIPLE == rectype) || IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype)
		       || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
		DEBUG_ONLY(jstart = jb;)
		if (JRT_HISTREC == rectype)
		{
			histinfo = &(((repl_histrec_jnl_ptr_t)rec)->histcontent);
			ENDIAN_CONVERT_REPL_HISTINFO(histinfo);
		} else if (JRT_TRIPLE == rectype)
		{
			oldtriple = (repl_old_triple_jnl_ptr_t)rec;
			oldtriple->cycle = GTM_BYTESWAP_32(oldtriple->cycle);
			oldtriple->start_seqno = GTM_BYTESWAP_64(oldtriple->start_seqno);
		} else
		{	/* pini_addr, time, checksum and tn field of the journal records created by the source server are
			 * irrelevant to the receiver server and hence no point doing the endian conversion for them.
			 */
			((jrec_suffix *)((unsigned char *)rec + reclen - JREC_SUFFIX_SIZE))->backptr = reclen;
			rec->jrec_null.jnl_seqno = GTM_BYTESWAP_64(rec->jrec_null.jnl_seqno);
			/* Starting jnl ver V22, we have a "strm_seqno" field in the journal record so endian convert that */
			if (V22_JNL_VER <= remote_jnl_ver)
			{	/* At this point, we could have a TCOM or NULL or SET/KILL/ZKILL/ZTRIG type of record.
				 * Assert that all of them have "strm_seqno" at the exact same offset so we can avoid
				 * an if/then/else check on the record types in order to endian convert "strm_seqno".
				 */
				assert(&rec->jrec_null.strm_seqno == &rec->jrec_set_kill.strm_seqno);
				assert(&rec->jrec_null.strm_seqno == &rec->jrec_tcom.strm_seqno);
				rec->jrec_null.strm_seqno = GTM_BYTESWAP_64(rec->jrec_null.strm_seqno);
			}
			if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
			{
				/* This code will need changes in case the jnl-ver changes from V23 to V24 so add an assert to
				 * alert to that possibility. Once the code is fixed for the new jnl format, change the assert
				 * to reflect the new latest jnl-ver.
				 */
				assert(JNL_VER_THIS == V23_JNL_VER);
				/* To better understand the logic below (particularly the use of hardcoded offsets), see comment
				 * in repl_filter.c (search for "struct_jrec_upd layout" for the various jnl versions we support).
				 */
				if (V22_JNL_VER <= remote_jnl_ver)
				{	/* byte-swap update_num */
					assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
					rec->jrec_set_kill.update_num = GTM_BYTESWAP_32(rec->jrec_set_kill.update_num);
					/* No need to byte-swap num_participants as it is not used by the update process */
					/* Get pointer to mumps_node */
					keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
					assert(keystr == (jnl_string *)&rec->jrec_ztworm.ztworm_str);
				} else if (V19_JNL_VER <= remote_jnl_ver)
				{	/* byte-swap update_num */
					ptr = (unsigned char *)rec + 32; /* is offset of update_num in V19 struct_jrec_upd */
					update_num = *(uint4 *)ptr;
					*(uint4 *)ptr = GTM_BYTESWAP_32(update_num);
					/* No need to byte-swap num_participants as it is not used by the update process */
					/* Get pointer to mumps_node */
					keystr = (jnl_string *)((unsigned char *)rec + 40); /* is offset of mumps_node */
				} else
				{
					assert(V17_JNL_VER <= remote_jnl_ver);
					/* Note: In V17, there is no update_num or num_participants like V19 so no endian convert */
					/* Get pointer to mumps_node */
					keystr = (jnl_string *)((unsigned char *)rec + 32); /* is offset of mumps_node */
				}
				/* In V18, the jnl_string contained a 32 bit length field followed by mumps_node
				 * In V19, the "length" field is divided into 8 bit "nodeflags" and 24 bit "length" fields.
				 * Byteswap the entire 32 bit value
				 */
				nodeflags_keylen = *(jnl_str_len_t *)keystr;
				*(jnl_str_len_t *)keystr = GTM_BYTESWAP_32(nodeflags_keylen);
				if (IS_SET(rectype))
				{
					assert(!IS_ZTWORM(rectype));
					/* SET records have a 'value' part which needs to be endian converted */
					vallen_ptr = (mstr_len_t *)&keystr->text[keystr->length];
					GET_MSTR_LEN(temp_val, vallen_ptr);
					temp_val = GTM_BYTESWAP_32(temp_val);
					PUT_MSTR_LEN(vallen_ptr, temp_val);
					/* The actual 'value' itself is a character array and hence needs no endian conversion */
				}
			} else if (JRT_TCOM == rectype)
			{	/* byte-swap num_participants as this is relied upon by "repl_sort_tr_buff".
				 * The offset and size of this field are different for older versions. Endian convert accordingly.
 				 *       V22 struct_jrec_tcom layout is as follows.
				 *      	offset = 0042 [0x002a]      size = 0002 [0x0002]    ----> num_participants
 				 *       V19/V20/V21 struct_jrec_tcom layout is as follows.
 				 *      	offset = 0034 [0x0022]      size = 0002 [0x0002]    ----> num_participants
				 *       V17/V18 struct_jrec_tcom layout is as follows.
				 *      	offset = 0040 [0x0028]      size = 0004 [0x0004]    ----> participants
				 */
				if (V22_JNL_VER <= remote_jnl_ver)
				{
					assert(42 == ((INTPTR_T)&rec->jrec_tcom.num_participants - (INTPTR_T)rec));
					assert(SIZEOF(num_participants_2bytes) == SIZEOF(rec->jrec_tcom.num_participants));
					num_participants_2bytes = rec->jrec_tcom.num_participants;
					rec->jrec_tcom.num_participants = GTM_BYTESWAP_16(num_participants_2bytes);
				} else if (V19_JNL_VER <= remote_jnl_ver)
				{
					ptr = (unsigned char *)rec + 34; /* is offset of update_num in V19 struct_jrec_upd */
					assert(SIZEOF(num_participants_2bytes) == SIZEOF(unsigned short));
					num_participants_2bytes = *(unsigned short *)ptr;
					rec->jrec_tcom.num_participants = GTM_BYTESWAP_16(num_participants_2bytes);
				} else
				{
					assert(V17_JNL_VER <= remote_jnl_ver);
					ptr = (unsigned char *)rec + 40; /* is offset of update_num in V19 struct_jrec_upd */
					assert(SIZEOF(num_participants_4bytes) == SIZEOF(uint4));
					num_participants_4bytes = *(uint4 *)ptr;
					rec->jrec_tcom.num_participants = GTM_BYTESWAP_32(num_participants_4bytes);
				}
				assert(rec->jrec_tcom.num_participants);
				/* token_seq.jnl_seqno & strm_seqno have already been endian converted. */
			}
			/* else if (JRT_NULL == rectype)
			 *	token_seq.jnl_seqno & strm_seqno have already been endian converted.
			 */
			assert(reclen == REC_LEN_FROM_SUFFIX(jb, reclen));
		}
		jb = jb + reclen;
		assert(jb == jstart + reclen);
		jlen -= reclen;
	}
	if ((-1 != status) && (0 != jlen))
	{	/* Incomplete record */
		assert(FALSE);
		status = -1;
	}
	return status;
}

STATICFNDEF void do_flow_control(uint4 write_pos)
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
	seq_num			temp_seq_num;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	space_used = 0;
	if (recvpool_ctl->wrapped)
		space_used = write_pos + recvpool_size - (read_pos = upd_proc_local->read);
	if (!recvpool_ctl->wrapped || space_used > recvpool_size)
		space_used = write_pos - (read_pos = upd_proc_local->read);
	assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
	if (space_used >= recvpool_high_watermark && !xoff_sent)
	{	/* Send XOFF message */
		if (!remote_side->cross_endian)
		{
			xoff_msg.type = REPL_XOFF;
			memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, SIZEOF(seq_num));
			xoff_msg.len = MIN_REPL_MSGLEN;
		} else
		{
			xoff_msg.type = GTM_BYTESWAP_32(REPL_XOFF);
			temp_seq_num = GTM_BYTESWAP_64(upd_proc_local->read_jnl_seqno);
			memcpy((uchar_ptr_t)&xoff_msg.msg[0], (uchar_ptr_t)&temp_seq_num, SIZEOF(seq_num));
			xoff_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
		}
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xoff_msg, MIN_REPL_MSGLEN, REPL_POLL_NOWAIT)
		{
			GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
		}
		CHECK_REPL_SEND_LOOP_ERROR(status, "REPL_XOFF");
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
		if (!remote_side->cross_endian)
		{
			xon_msg.type = REPL_XON;
			memcpy((uchar_ptr_t)&xon_msg.msg[0], (uchar_ptr_t)&upd_proc_local->read_jnl_seqno, SIZEOF(seq_num));
			xon_msg.len = MIN_REPL_MSGLEN;
		} else
		{
			xon_msg.type = GTM_BYTESWAP_32(REPL_XON);
			temp_seq_num = GTM_BYTESWAP_64(upd_proc_local->read_jnl_seqno);
			memcpy((uchar_ptr_t)&xon_msg.msg[0], (uchar_ptr_t)&temp_seq_num, SIZEOF(seq_num));
			xon_msg.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
		}
		REPL_SEND_LOOP(gtmrecv_sock_fd, &xon_msg, MIN_REPL_MSGLEN, REPL_POLL_NOWAIT)
		{
			GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
		}
		CHECK_REPL_SEND_LOOP_ERROR(status, "REPL_XON");
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

STATICFNDEF int gtmrecv_est_conn(void)
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	boolean_t     		keepalive;
	GTM_SOCKLEN_TYPE	optlen;
	int			status, keepalive_opt, optval, save_errno;
	int			send_buffsize, recv_buffsize, tcp_r_bufsize;
	struct  linger  	disable_linger = {0, 0};
	char			print_msg[1024];
	struct addrinfo		primary_ai;
	struct sockaddr_storage	primary_sas;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Wait for a connection from a Source Server. The Receiver Server is an iterative server. */
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	/* Create a listen socket */
	gtmrecv_comm_init((in_port_t)gtmrecv_local->listen_port);
	primary_ai.ai_addr = (sockaddr_ptr)&primary_sas;
	primary_ai.ai_addrlen = SIZEOF(primary_sas);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for a connection...\n");
	/* Null initialize fields that need to be initialized only after connecting to the primary.
	 * It is ok not to hold a lock on the journal pool while updating jnlpool_ctl fields since this will be the only
	 * process updating those fields.
	 */
	assert(remote_side == &gtmrecv_local->remote_side);
	remote_side->proto_ver = REPL_PROTO_VER_UNINITIALIZED;
	jnlpool_ctl->primary_instname[0] = '\0';
	while (TRUE)
	{
		while (TRUE)
		{
			if (0 < (status = fd_ioready(gtmrecv_listen_sock_fd, TRUE, REPL_POLL_WAIT)))
				break;
			if (-1 == status)
			{
				save_errno = errno;
				assert((EAGAIN != save_errno) && (EINTR != save_errno));
				ISSUE_REPLCOMM_ERROR("Error in select on listen socket", save_errno);
			} else if (0 == status)	/* timeout */
				gtmrecv_poll_actions(0, 0, NULL);
		}
		ACCEPT_SOCKET(gtmrecv_listen_sock_fd, primary_ai.ai_addr,
					(GTM_SOCKLEN_TYPE *)&primary_ai.ai_addrlen, gtmrecv_sock_fd);
		if (FD_INVALID == gtmrecv_sock_fd)
		{
			save_errno = errno;
#			ifdef __hpux
			if (ENOBUFS == save_errno)
			{
				gtmrecv_poll_actions(0, 0, NULL);
				continue;
			}
#			endif
			ISSUE_REPLCOMM_ERROR("Error accepting connection from Source Server", save_errno);
		}
		break;
	}
	/* Connection established */
	repl_close(&gtmrecv_listen_sock_fd); /* Close the listener socket */
	repl_connection_reset = FALSE;
	if (-1 == setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, SIZEOF(disable_linger)))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket disable linger", errno);
#	ifdef REPL_DISABLE_KEEPALIVE
	keepalive = FALSE;
#	else
	keepalive = TRUE;
#	endif
	if (-1 == setsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&keepalive,
					SIZEOF(keepalive)))
	{
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket enable keepalive", errno);
	}
	/* Set up the keepalive parameters
	 * TCP_KEEPCNT : overrides tcp_keepalive_probes
	 * TCP_KEEPIDLE: overrides tcp_keepalive_time
	 * TCP_KEEPINTVL: overrides tcp_keepalive_intvl
	 */
#	if defined(KEEPALIVE_PROTO_LEVEL)
	keepalive_opt = KEEPALIVE_PROBES;
	if (-1 == setsockopt(gtmrecv_sock_fd, KEEPALIVE_PROTO_LEVEL, TCP_KEEPCNT, (void*)&keepalive_opt, SIZEOF(keepalive_opt)))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket setting tcp_keepalive_probes", errno);
	keepalive_opt = KEEPALIVE_TIME;
	if (-1 == setsockopt(gtmrecv_sock_fd, KEEPALIVE_PROTO_LEVEL, TCP_KEEPIDLE, (void*)&keepalive_opt, SIZEOF(keepalive_opt)))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket setting tcp_keepalive_time", errno);
	keepalive_opt = KEEPALIVE_INTVL;
	if (-1 == setsockopt(gtmrecv_sock_fd, KEEPALIVE_PROTO_LEVEL, TCP_KEEPINTVL, (void*)&keepalive_opt, SIZEOF(keepalive_opt)))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket setting tcp_keepalive_intvl", errno);
#	endif
	optlen = SIZEOF(optval);
   	if ( -1 == getsockopt(gtmrecv_sock_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket checking keepalive enabled or not", errno)
#	if !defined(KEEPALIVE_PROTO_LEVEL)
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "SO_KEEPALIVE is %s\n", (optval ? "ON" : "OFF"));
#	else
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "SO_KEEPALIVE is %s. ", (optval ? "ON" : "OFF"));

	if (-1 == getsockopt(gtmrecv_sock_fd, KEEPALIVE_PROTO_LEVEL, TCP_KEEPCNT, &optval, &optlen))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket getting tcp_keepalive_probes", errno);
	if (optval)
		repl_log(gtmrecv_log_fp, FALSE, TRUE, "TCP_KEEPCNT is %d, ", optval);

	if (-1 == getsockopt(gtmrecv_sock_fd, KEEPALIVE_PROTO_LEVEL, TCP_KEEPIDLE, &optval, &optlen))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket getting tcp_keepalive_time", errno);
	if (optval)
       		repl_log(gtmrecv_log_fp, FALSE, TRUE, "TCP_KEEPIDLE is %d, ", optval);

	if (-1 == getsockopt(gtmrecv_sock_fd, KEEPALIVE_PROTO_LEVEL, TCP_KEEPINTVL, &optval, &optlen))
		ISSUE_REPLCOMM_ERROR("Error with receiver server socket getting tcp_keepalive_intvl", errno);
	if (optval)
		repl_log(gtmrecv_log_fp, FALSE, TRUE, "TCP_KEEPINTVL is %d.\n", optval);
#	endif
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &send_buffsize)))
		ISSUE_REPLCOMM_ERROR("Error getting socket send buffsize", errno);
	if (send_buffsize < GTMRECV_TCP_SEND_BUFSIZE)
	{
		if (0 != (status = set_send_sock_buff_size(gtmrecv_sock_fd, GTMRECV_TCP_SEND_BUFSIZE)))
		{
			if (send_buffsize < GTMRECV_MIN_TCP_SEND_BUFSIZE)
			{
				SNPRINTF(print_msg, SIZEOF(print_msg), "Could not set TCP send buffer size to %d : %s",
						GTMRECV_MIN_TCP_SEND_BUFSIZE, STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0,
						ERR_TEXT, 2, LEN_AND_STR(print_msg));
			}
		}
	}
	if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize))) /* may have changed */
		ISSUE_REPLCOMM_ERROR("Error getting socket send buffsize", errno);
	if (0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &recv_buffsize)))
		ISSUE_REPLCOMM_ERROR("Error getting socket recv buffsize", errno);
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
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_INFO(ERR_REPLCOMM), 0,
					ERR_TEXT, 2, LEN_AND_STR(print_msg));
		}
	}
	if (0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &repl_max_recv_buffsize))) /* may have changed */
		ISSUE_REPLCOMM_ERROR("Error getting socket recv buffsize", errno);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Connection established, using TCP send buffer size %d receive buffer size %d\n",
			repl_max_send_buffsize, repl_max_recv_buffsize);
	repl_log_conn_info(gtmrecv_sock_fd, gtmrecv_log_fp);
	/* re-determine endianness of other side */
	remote_side->endianness_known = FALSE;
	/* re-determine journal format of other side */
	remote_side->jnl_ver = 0;
	/* re-determine compression level on the replication pipe after every connection establishment */
	repl_zlib_cmp_level = ZLIB_CMPLVL_NONE;
	/* Reset prior connection related state variables (see <C9J02_003091_receiver_server_assert_due_to_lingering_XOFF>) */
	xoff_sent = FALSE;
	xoff_msg_log_cnt = 0;
	/* Note that even though we are reopening a fresh connection, we should NOT reset the cached information
	 * last_rcvd_histinfo, last_valid_histinfo etc. in this case as we might just resume processing from where
	 * the previous connection left off in which case all the cached information is still valid. If we dont
	 * resume from where we left off, the receiver will anyways error out asking for a rollback to be done so
	 * the cached information never gets used if it is invalid.
	 */
	return (SS_NORMAL);
}

int gtmrecv_alloc_filter_buff(int bufsiz)
{
	unsigned char	*old_filter_buff, *free_filter_buff;

	bufsiz = ROUND_UP2(bufsiz, OS_PAGE_SIZE);
	if ((NO_FILTER != gtmrecv_filter) && (repl_filter_bufsiz < bufsiz))
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

/* This function can be used to only send fixed-size message types across the replication pipe.
 * This in turn uses REPL_SEND* macros but also does error checks and sets the global variables
 *	"repl_connection_reset" or "gtmrecv_wait_for_jnl_seqno" accordingly.
 * It also does the endian conversion of the 'type' and 'len' fields of the repl_msg_t structure being sent.
 *
 *	msg            = Pointer to the message buffer to send
 *	type	       = One of the various message types listed in repl_msg.h
 *	len	       = Length of the message to be sent
 *	msgtypestr     = Message name as a string to display meaningful error messages
 *	optional_seqno = Optional seqno that needs to be printed along with the message name
 */
void	gtmrecv_repl_send(repl_msg_ptr_t msgp, int4 type, int4 len, char *msgtypestr, seq_num optional_seqno)
{
	unsigned char		*msg_ptr;				/* needed for REPL_SEND_LOOP */
	int			tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int			status;					/* needed for REPL_SEND_LOOP */
	FILE			*log_fp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!mur_options.rollback || (NULL == recvpool.gtmrecv_local));
	assert(mur_options.rollback || (NULL != recvpool.gtmrecv_local));
	assert((REPL_MULTISITE_MSG_START > type) || (REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver));
	log_fp = (NULL == gtmrecv_log_fp) ? stdout : gtmrecv_log_fp;
	if (MAX_SEQNO != optional_seqno)
	{
		repl_log(log_fp, TRUE, TRUE, "Sending %s message with seqno "INT8_FMT" "INT8_FMTX"\n", msgtypestr,
			optional_seqno, optional_seqno);
	} else
		repl_log(log_fp, TRUE, TRUE, "Sending %s message\n", msgtypestr);
	/* Assert that if we dont know the endianness of the remote side, we assume it is the same endianness */
	assert(remote_side->endianness_known || !remote_side->cross_endian);
	if (!remote_side->cross_endian)
	{
		msgp->type = type;
		msgp->len = len;
	} else
	{
		msgp->type = GTM_BYTESWAP_32(type);
		msgp->len = GTM_BYTESWAP_32(len);
	}
	REPL_SEND_LOOP(gtmrecv_sock_fd, msgp, len, REPL_POLL_NOWAIT)
	{
		GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
	}
	CHECK_REPL_SEND_LOOP_ERROR(status, msgtypestr);
	assert(SS_NORMAL == status);
}

/* This function is invoked on receipt of a REPL_NEED_INSTINFO message.
 * After doing some checks, it sends back a REPL_INSTINFO message containing the instance information.
 * If any of the checks fail it issues the appropriate error right away.
 * There can be TWO callers. One is the receiver server and another fetchresync rollback.
 *	is_rcvr_srvr is 1 for the former and 0 for the latter.
 */
void	gtmrecv_check_and_send_instinfo(repl_needinst_msg_ptr_t need_instinfo_msg, boolean_t is_rcvr_srvr)
{
	boolean_t		remote_side_is_supplementary, grab_lock_needed;
	repl_inst_hdr_ptr_t	inst_hdr;
	repl_instinfo_msg_t	instinfo_msg;
	repl_inst_uuid		*strm_start, *strm_top, *strm_info;
	FILE			*log_fp;
	int			reuse_slot, first_usable_slot;
	seq_num			strm_jnl_seqno;
	sgmnt_addrs		*repl_csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	grab_lock_needed = is_rcvr_srvr || ((NULL != jnlpool_ctl) && !repl_csa->hold_onto_crit);
	remote_side_is_supplementary = need_instinfo_msg->is_supplementary;
	remote_side->is_supplementary = remote_side_is_supplementary;
	assert(remote_side->endianness_known); /* ensure remote_side->cross_endian is reliable */
	if (remote_side->cross_endian)
		ENDIAN_CONVERT_REPL_INST_UUID(&need_instinfo_msg->lms_group_info);
	assert(is_rcvr_srvr && (NULL != gtmrecv_log_fp) || !is_rcvr_srvr && (NULL == gtmrecv_log_fp));
	assert(!is_rcvr_srvr || !repl_csa->hold_onto_crit);
	assert(is_rcvr_srvr || !jgbl.onlnrlbk || repl_csa->hold_onto_crit);
	log_fp = !is_rcvr_srvr ? stdout : gtmrecv_log_fp;
	repl_log(log_fp, TRUE, TRUE, "Received REPL_NEED_INSTINFO message from primary instance [%s]\n",
		need_instinfo_msg->instname);
	inst_hdr = jnlpool.repl_inst_filehdr;
	assert(NULL != inst_hdr);
	/* The fact that we came here (REPL_NEED_INSTINFO message) implies the source server understands the
	 * supplementary protocol. In that case, make sure -UPDATERESYNC if specified at receiver server startup
	 * had a value as well. If not, issue error.
	 */
	if (is_rcvr_srvr && recvpool.gtmrecv_local->updateresync && (FD_INVALID == recvpool.gtmrecv_local->updresync_instfile_fd))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
			  LEN_AND_LIT("Source side is >= V5.5-000 implies -UPDATERESYNC needs a value specified"));
		assert(FALSE);	/* we dont expect the rts_error to return control */
	}
	/* We usually expect the LMS group info to be non-NULL on both primary and secondary. An exception is if
	 * both of them are being brought up for the first time using a GT.M version that supports supplementary instances.
	 * In this case, if the primary was brought up as a root primary, then it should still have the group info filled.
	 * Assert that.
	 */
	assert(inst_hdr->lms_group_info.created_time
		|| need_instinfo_msg->lms_group_info.created_time || !need_instinfo_msg->is_rootprimary);
	assert((is_rcvr_srvr && (NULL != jnlpool_ctl)) || (!is_rcvr_srvr && (NULL == jnlpool_ctl)) || jgbl.onlnrlbk
			|| (jgbl.mur_rollback && ANTICIPATORY_FREEZE_AVAILABLE));
	/* If this instance is supplementary and the journal pool exists (to indicate whether updates are enabled or not
	 * which in turn helps us know whether this is an originating instance or not) do some additional checks.
	 */
	if (is_rcvr_srvr && inst_hdr->is_supplementary)
	{
		assert(NULL != jnlpool_ctl);
		if (!jnlpool_ctl->upd_disabled)
		{	/* this supplementary instance was started with -UPDOK. Issue error if source is also supplementary */
			if (need_instinfo_msg->is_supplementary)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_NOSUPPLSUPPL, 4,
						LEN_AND_STR((char *)inst_hdr->inst_info.this_instname),
						LEN_AND_STR((char *)need_instinfo_msg->instname));
				assert(FALSE);	/* we dont expect the rts_error to return control */
			}
		} else
		{	/* this supplementary instance was started with -UPDNOTOK. Issue error if source is not supplementary */
			if (!need_instinfo_msg->is_supplementary)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SUPRCVRNEEDSSUPSRC, 4,
					LEN_AND_STR((char *)inst_hdr->inst_info.this_instname),
					LEN_AND_STR((char *)need_instinfo_msg->instname));
				assert(FALSE);	/* we dont expect the rts_error to return control */
			}
		}
	}
	/* Assert that if the receiver side is a root primary (i.e. has updates enabled), it better be a
	 * supplementary instance and have the LMS group info filled in. Exception is if this a FETCHRESYNC
	 * ROLLBACK run on an instance which was once primary (not necessarily a supplementary one).
	 */
	assert((NULL == jnlpool_ctl) || jnlpool_ctl->upd_disabled
		|| inst_hdr->is_supplementary && IS_REPL_INST_UUID_NON_NULL(inst_hdr->lms_group_info)
		|| jgbl.onlnrlbk || (jgbl.mur_rollback && ANTICIPATORY_FREEZE_AVAILABLE));
	/* Check if primary and secondary are in same LMS group. Otherwise issue error. An exception is if the group info has
	 * not yet been filled in after instance file creation. In that case, copy the info from primary and skip the error check.
	 */
	if (IS_REPL_INST_UUID_NON_NULL(inst_hdr->lms_group_info))
	{	/* LMS Group info has been initialized. Compare with that of the source side */
		if (memcmp(&need_instinfo_msg->lms_group_info, &inst_hdr->lms_group_info, SIZEOF(inst_hdr->lms_group_info)))
		{	/* Source and Receiver are part of DIFFERENT LMS Groups. If this instance is supplementary and remote
			 * side is not supplementary then we expect them to be different. Otherwise issue error.
			 */
			if (!inst_hdr->is_supplementary || remote_side_is_supplementary)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INSNOTJOINED, 4,
						LEN_AND_STR((char *)inst_hdr->inst_info.this_instname),
						LEN_AND_STR((char *)need_instinfo_msg->instname));
				assert(FALSE);	/* we dont expect the rts_error to return control */
			}
		} else
		{	/* Primary and Secondary are part of SAME LMS Group. If this instance is supplementary and remote
			 * side is not supplementary then we expect them to be different. Issue error in that case.
			 */
			if (inst_hdr->is_supplementary && !remote_side_is_supplementary)
			{
				assert(!need_instinfo_msg->is_supplementary); /* else NOSUPPLSUPPL error must have been issued */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INSROLECHANGE, 4,
						LEN_AND_STR((char *)inst_hdr->inst_info.this_instname),
						LEN_AND_STR((char *)need_instinfo_msg->instname));
				assert(FALSE);	/* we dont expect the rts_error to return control */
			}
		}
		if (is_rcvr_srvr && recvpool.gtmrecv_local->updateresync
			&& (FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd))
		{	/* Caller is receiver server and -UPDATERESYNC=<INSTFILENAME> was specified. Check if the lms_group_info
			 * of that file matches that of the source side. If not issue error.
			 */
			if (memcmp(&recvpool.gtmrecv_local->updresync_lms_group,
						&need_instinfo_msg->lms_group_info, SIZEOF(inst_hdr->lms_group_info)))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Specified input instance file does not have same "
						"LMS Group information as source server instance"));
			}
		}
	} else if (IS_REPL_INST_UUID_NON_NULL(need_instinfo_msg->lms_group_info))
	{	/* LMS Group info is NULL in instance file header. Initialize it in journal pool from the value
		 * on the primary side AND flush the changes to disk. Get lock before manipulating it.
		 * If caller is rollback, no other process can be touching the instance file until we are done so
		 * no need of the lock in that case.
		 */
		if (grab_lock_needed)
		{
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
			if (is_rcvr_srvr)
				GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
		}
		inst_hdr->lms_group_info = need_instinfo_msg->lms_group_info;
		repl_inst_flush_filehdr();
		if (grab_lock_needed)
			rel_lock(jnlpool.jnlpool_dummy_reg);
	}
	/* If this instance is supplementary and remote side is not, then find out which stream # the non-supplementary source
	 * corresponds to. Issue error if the source LMS group is unknown in the instance file. If this is non-supplementary,
	 * the stream index is 0.
	 */
	if (inst_hdr->is_supplementary && !remote_side_is_supplementary)
	{
		assert(ARRAYSIZE(inst_hdr->strm_group_info) == (MAX_SUPPL_STRMS - 1));
		assert(SIZEOF(repl_inst_uuid) == SIZEOF(inst_hdr->strm_group_info[0]));
		strm_start = &inst_hdr->strm_group_info[0];
		strm_top = strm_start + ARRAYSIZE(inst_hdr->strm_group_info);
		/* Do this check under a lock as another receiver server started with -UPDATERESYNC=<INSTFILENAME> (not supported
		 * now but will be in the future) could be changing the "strm_group_info" array concurrently.
		 * If a matching stream is found, update gtmrecv_local->strm_index to reflect this while still holding lock.
		 * This field is checked by the -UPDATERESYNC to see if a receiver has a given stream # actively in use.
		 */
		if (grab_lock_needed)
		{
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
			if (is_rcvr_srvr)
				GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
		}
		reuse_slot = 0;
		if (is_rcvr_srvr && recvpool.gtmrecv_local->updateresync && gtmrecv_options.reuse_specified)
		{	/* If -REUSE was specified, check if instance name specified matches any existing slot.
			 * If slot has already been found, dont search any more.
			 */
			for (strm_info = strm_start; strm_info < strm_top; strm_info++)
			{
				if (IS_REPL_INST_UUID_NULL(*strm_info))
					continue;
				if (!STRCMP(gtmrecv_options.reuse_instname, strm_info->this_instname))
				{
					reuse_slot = (strm_info - strm_start) + 1;
					break;
				}
			}
			if (strm_info == strm_top)
			{	/* -REUSE specified an instance name that is not present in any of the 15 strm_group slots */
				rel_lock(jnlpool.jnlpool_dummy_reg);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REUSEINSTNAME, 0, ERR_TEXT, 2,
					  LEN_AND_LIT("Instance name in REUSE does not match any of 15 slots in instance file"));
			}
			assert(reuse_slot);
		}
		first_usable_slot = 0;
		strm_index = 0;
		for (strm_info = strm_start; strm_info < strm_top; strm_info++)
		{
			if (IS_REPL_INST_UUID_NULL(*strm_info))
			{
				if (!first_usable_slot)
					first_usable_slot = (strm_info - strm_start) + 1;
				continue;
			}
			if (!memcmp(&need_instinfo_msg->lms_group_info, strm_info, SIZEOF(repl_inst_uuid)))
			{	/* Found the stream corresponding to the source side */
				strm_index = (strm_info - strm_start) + 1;
				break;
			}
		}
		if (strm_info == strm_top)
		{	/* Non-supplementary source is unknown to this supplementary instance */
			if (reuse_slot)	/* -REUSE specified and did locate a reusable slot. Use it. */
			{
				assert(is_rcvr_srvr);
				strm_index = reuse_slot;
			} else if (gtmrecv_options.resume_specified)
			{
				assert(is_rcvr_srvr);
				strm_index = gtmrecv_options.resume_strm_num;
			} else if (is_rcvr_srvr && recvpool.gtmrecv_local->updateresync)
			{
				if (first_usable_slot)
					strm_index = first_usable_slot;
				else
				{
					if (grab_lock_needed)
						rel_lock(jnlpool.jnlpool_dummy_reg);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0, ERR_TEXT, 2,
						LEN_AND_LIT("No empty slot found. Specify REUSE to choose one for reuse"));
				}
			} else
			{
				if (grab_lock_needed)
					rel_lock(jnlpool.jnlpool_dummy_reg);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INSUNKNOWN, 4,
						LEN_AND_STR((char *)inst_hdr->inst_info.this_instname),
						LEN_AND_STR((char *)need_instinfo_msg->instname));
				assert(FALSE);	/* we dont expect the rts_error to return control */
			}
			/* Since we did not find the stream in the existing instance file but did find a slot, fill that slot
			 * while we have the lock on the instance file. This way another -updateresync startup of a receiver
			 * server (when we have multiple receiver server support) will see this slot taken when it gets the lock.
			 */
			assert(is_rcvr_srvr && recvpool.gtmrecv_local->updateresync
				&& (FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd));
			assert(!memcmp(&recvpool.gtmrecv_local->updresync_lms_group,
						&need_instinfo_msg->lms_group_info, SIZEOF(inst_hdr->lms_group_info)));
			assert(0 < strm_index);
			strm_info = &strm_start[strm_index - 1];
			assert(strm_info < strm_top);
			assert(IS_REPL_INST_UUID_NON_NULL(need_instinfo_msg->lms_group_info));
			*strm_info = need_instinfo_msg->lms_group_info;
			repl_inst_flush_filehdr();
		} else if ((gtmrecv_options.resume_specified) && (gtmrecv_options.resume_strm_num != strm_index))
		{	/* If -RESUME was specified, then the slot it matched must be same as slot found without its use */
			assert(is_rcvr_srvr);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RESUMESTRMNUM, 0, ERR_TEXT, 2, LEN_AND_LIT("Source side LMS "
				"group is found in instance file but RESUME specifies different stream number"));
		} else if (reuse_slot && (reuse_slot != strm_index))
		{	/* If -REUSE was specified, then the slot it matched must be same as slot found without its use */
			assert(is_rcvr_srvr);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REUSEINSTNAME, 0, ERR_TEXT, 2, LEN_AND_LIT("Source side LMS "
				"group is found in instance file but REUSE specifies different instance name"));
		}
		assert(INVALID_SUPPL_STRM != strm_index);
		assert(0 < strm_index);
		assert(MAX_SUPPL_STRMS > strm_index);
		/* Maintain stream slot # in shared memory as well so another -UPDATERESYNC=<INSTFILENAME>
		 * can see which stream# is actively in use by this receiver server. */
		if (is_rcvr_srvr)
		{
			if (recvpool.gtmrecv_local->strm_index && (strm_index != recvpool.gtmrecv_local->strm_index))
			{	/* This receiver server has already connected to a source server with a different stream #.
				 * Since mixing of multiple stream journal records in the same receive pool confuses the
				 * update process, issue error. Note: This limitation "might" be removed in the future.
				 */
				rel_lock(jnlpool.jnlpool_dummy_reg);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RCVRMANYSTRMS, 2,
						strm_index, recvpool.gtmrecv_local->strm_index);
			}
			recvpool.gtmrecv_local->strm_index = strm_index;
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
					"Determined non-supplementary source Stream # = %d\n", strm_index);
			assert(IS_REPL_INST_UUID_NON_NULL(need_instinfo_msg->lms_group_info));
			recvpool.gtmrecv_local->remote_lms_group = need_instinfo_msg->lms_group_info;
			rel_lock(jnlpool.jnlpool_dummy_reg);
		} else
			repl_log(stdout, TRUE, TRUE, "Determined non-supplementary source Stream # = %d\n", strm_index);
		/* Compute non-zero strm_jnl_seqno to send across in the REPL_INSTINFO message.
		 * For receiver server
		 *	If updateresync startup and this is the first connection to a source server
		 *		If -RESUME is not specified :
		 *			--> gtmrecv_local->updresync_jnl_seqno
		 *		If -RESUME is specified :
		 *			--> jnlpool_ctl->strm_seqno[gtmrecv_options.resume_strm_num]
		 *	If updateresync startup and this is not the first connection to a source server
		 *		--> recvpool_ctl->jnl_seqno
		 *	If normal startup and this is the first connection to a source server
		 *		--> inst_hdr->strm_seqno[strm_index] where strm_index > 0
		 *	If this is not the first connection to a source server
		 *		--> recvpool_ctl->jnl_seqno
		 *	In case of instance crash
		 *		--> No need to worry about this case as we would have otherwise issued a
		 *		--> REPLREQROLLBACK error when the source server for this instance started up after the crash.
		 * For rollback
		 *	If normal startup after a clean shutdown of the instance file
		 *		--> inst_hdr->strm_seqno[strm_index] where strm_index > 0
		 *	In case of crash shutdown of instance file
		 *		--> Take max of cs_data->strm_seqno[strm_index] across all databases and use this (strm_index > 0)
		 */
		if (is_rcvr_srvr)
		{
			if (recvpool.upd_proc_local->read_jnl_seqno)
				strm_jnl_seqno = recvpool.recvpool_ctl->jnl_seqno;
			else
			{
				if (recvpool.gtmrecv_local->updateresync)
				{
					assert(FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd);
					if (!gtmrecv_options.resume_specified)
						strm_jnl_seqno = recvpool.gtmrecv_local->updresync_jnl_seqno;
					else
					{
						strm_jnl_seqno = jnlpool_ctl->strm_seqno[gtmrecv_options.resume_strm_num];
						/* It is possible for the strm_seqno to be 0. This implies the stream has
						 * had no updates. In that case, ideally this value should have been 1.
						 * But because we want to differentiate a stream that has had updates from
						 * stream numbers where there is no interest, we follow this convention.
						 * Therefore, in this case, reset the 0 back to 1 so we never send a zero
						 * seqno to the other side.
						 */
						if (!strm_jnl_seqno)
							strm_jnl_seqno = 1;
					}
				} else
					strm_jnl_seqno = jnlpool_ctl->strm_seqno[strm_index];
				assert(0 == GET_STRM_INDEX(strm_jnl_seqno));
			}
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Sending Stream Seqno = "INT8_FMT" "INT8_FMTX"\n",
										strm_jnl_seqno, strm_jnl_seqno);
		} else
		{
			if (jnlpool.repl_inst_filehdr->crash)
				strm_jnl_seqno = mur_get_max_strm_reg_seqno(strm_index);
			else
			{
				assert((NULL == jnlpool_ctl) || jgbl.onlnrlbk);
				assert((NULL == jnlpool_ctl)
					|| (jnlpool_ctl->strm_seqno[strm_index] == inst_hdr->strm_seqno[strm_index]));
				strm_jnl_seqno = jnlpool.repl_inst_filehdr->strm_seqno[strm_index];
			}
			repl_log(stdout, TRUE, TRUE, "Sending Stream Seqno = "INT8_FMT" "INT8_FMTX"\n",
										strm_jnl_seqno, strm_jnl_seqno);
		}
	} else
	{
		strm_jnl_seqno = 0;	/* actually no need to initialize this since source server will not look at this
					 * field in this case but still be safe */
		if (inst_hdr->is_supplementary)
		{	/* In case caller is receiver server, "strm_index" would have been already set to 0 in jnlpool_init.c.
			 * But in case caller is rollback, "strm_index" would still be set to -1. In this case, set it to 0.
			 */
			assert(!is_rcvr_srvr || (0 == strm_index));
			strm_index = 0;
		}
		DEBUG_ONLY(
			if (is_rcvr_srvr)
				NULL_INITIALIZE_REPL_INST_UUID(recvpool.gtmrecv_local->remote_lms_group);
		)
	}
	assert(!is_rcvr_srvr || (INVALID_SUPPL_STRM == recvpool.gtmrecv_local->strm_index)
		|| ((0 <= recvpool.gtmrecv_local->strm_index) && (MAX_SUPPL_STRMS > recvpool.gtmrecv_local->strm_index)));
	/* Initialize the remote side protocol version from "proto_ver" field of this msg */
	assert(REPL_PROTO_VER_SUPPLEMENTARY <= need_instinfo_msg->proto_ver);
	remote_side->proto_ver = need_instinfo_msg->proto_ver;
	/*************** Send REPL_INSTINFO message ***************/
	memset(&instinfo_msg, 0, SIZEOF(instinfo_msg));
	memcpy(instinfo_msg.instname, inst_hdr->inst_info.this_instname, MAX_INSTNAME_LEN - 1);
	if (grab_lock_needed)
	{
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
		if (is_rcvr_srvr)
			GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
	}
	instinfo_msg.was_rootprimary = (unsigned char)repl_inst_was_rootprimary();
	if (grab_lock_needed)
		rel_lock(jnlpool.jnlpool_dummy_reg);
	if (!is_rcvr_srvr)
		murgbl.was_rootprimary = instinfo_msg.was_rootprimary;
	instinfo_msg.strm_jnl_seqno = strm_jnl_seqno;
	/* strm_jnl_seqno is not expected to be zero unless this is a non-supplementary instance (like A->B) in which case
	 * strm_seqno is not maintained OR the remote side is supplementary (like P->Q) in which case the two instances do
	 * not communicate in-terms of strm_seqno (once the handshake is established)
	 */
	assert(strm_jnl_seqno || !inst_hdr->is_supplementary || remote_side_is_supplementary);
	instinfo_msg.lms_group_info = inst_hdr->lms_group_info;
	assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
	if (remote_side->cross_endian)
	{
		ENDIAN_CONVERT_REPL_INST_UUID(&instinfo_msg.lms_group_info);
		instinfo_msg.strm_jnl_seqno = GTM_BYTESWAP_64(instinfo_msg.strm_jnl_seqno);
	}
	gtmrecv_repl_send((repl_msg_ptr_t)&instinfo_msg, REPL_INSTINFO, SIZEOF(repl_instinfo_msg_t), "REPL_INSTINFO", MAX_SEQNO);
	if (repl_connection_reset || is_rcvr_srvr && gtmrecv_wait_for_jnl_seqno)
		return;
	/* Do not allow an instance which was formerly a root primary or which still
	 * has a non-zero value of "zqgblmod_seqno" to start up as a tertiary. The only exception is
	 * if this is P (supplementary instance) receiving from a non-supplementary instance.
	 * In that case, P can never be a root primary of the non-supplementary group and therefore
	 * cannot be affected by lost transactions being applied from the non-supplementary group.
	 */
	if ((!inst_hdr->is_supplementary || remote_side_is_supplementary)
		&& !need_instinfo_msg->is_rootprimary
		&& (instinfo_msg.was_rootprimary
			|| (is_rcvr_srvr && jnlpool_ctl->max_zqgblmod_seqno)))
	{
		if (is_rcvr_srvr)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PRIMARYNOTROOT, 2,
					LEN_AND_STR((char *) need_instinfo_msg->instname));
			gtmrecv_autoshutdown();	/* should not return */
		} else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PRIMARYNOTROOT, 2,
					LEN_AND_STR((char *) need_instinfo_msg->instname));
		assert(FALSE);
	}
	if (is_rcvr_srvr)
		memcpy(jnlpool_ctl->primary_instname, need_instinfo_msg->instname, MAX_INSTNAME_LEN - 1);
}

STATICFNDEF int	gtmrecv_start_onln_rlbk(void)
{
	char			command[ONLN_RLBK_CMD_MAXLEN], *errptr;
	int			status, save_errno, cmdlen;
	gtmrecv_local_ptr_t	gtmrecv_local;

	assert(!have_crit(CRIT_HAVE_ANY_REG));
	gtmrecv_local = recvpool.gtmrecv_local;
	MEMCPY_LIT(command, MUPIP_DIST_STR);
	cmdlen = STR_LIT_LEN(MUPIP_DIST_STR);
	MEMCPY_LIT(&command[cmdlen], ONLN_RLBK_CMD);
	cmdlen += STR_LIT_LEN(ONLN_RLBK_CMD);
	if (gtmrecv_options.autorollback_verbose)
	{
		MEMCPY_LIT(&command[cmdlen], ONLN_RLBK_VERBOSE);
		cmdlen += STR_LIT_LEN(ONLN_RLBK_VERBOSE);
	}
	MEMCPY_LIT(&command[cmdlen], ONLN_RLBK_QUALIFIERS);
	cmdlen += STR_LIT_LEN(ONLN_RLBK_QUALIFIERS);
	assert(0 < gtmrecv_local->listen_port);
	SNPRINTF(&command[cmdlen], ONLN_RLBK_CMD_MAXLEN, "%d", gtmrecv_local->listen_port); /* will add '\0' at the end */
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Executing %s\n", command);
	status = SYSTEM(((char *)command));
	if (0 != status)
	{
		if (-1 == status)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "SYSTEM command failed: %s\n", errptr);
		} else
		{	/* ONLINE ROLLBACK returned a non-zero status */
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "ONLINE FETCHRESYNC ROLLBACK exited with code - %d\n", status);
		}
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Could not complete ONLINE FETCHRESYNC ROLLBACK due to the above errors\n");
	} else
	{
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "ONLINE FETCHRESYNC ROLLBACK completed successfully\n");
	}
	return status;
}

/* We are here because a grab_lock or grab_crit saw a concurrent online rollback and we need to break the connection and
 * re-establish with the new sequence number (the rolled back one). Wait for the update process to let us know the new sequence
 * number from where we should start requesting transactions from.
 * Ideally this function should have been a part of onln_rlbk.c. However, the function calls into gtmrecv_poll_actions which
 * is strictly in the libmupip archive. Moving the function into onln_rlbk.c means it will try to pull in gtmrecv_poll_actions
 * and all the modules that it calls which is something that we don't desire. So, keep this function isolated from onln_rlbk.c
 */
void	gtmrecv_onln_rlbk_clnup(void)
{
	boolean_t		connection_already_reset;

	assert(NULL != gtmrecv_log_fp);
	assert(jnlpool.jnlpool_ctl == jnlpool_ctl);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "---> ONLINE ROLLBACK. Current Jnlpool Seqno : "INT8_FMT"\n", jnlpool_ctl->jnl_seqno);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for update process to set recvpool_ctl->onln_rlbk_flag\n");
	connection_already_reset = repl_connection_reset;
	assert(!gtmrecv_wait_for_jnl_seqno);
	while (TRUE)
	{
		SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
		gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
		/* If connection was already closed before we came here (possible if receiver server closed the connection in
		 * response to a REPL_ROLLBACK_FIRST message) after spawning off an online rollback, then we should NOT check for
		 * repl_connection_reset. This way, we avoid breaking prematurely from this loop without waiting for the update
		 * process to acknowledge the online rollback. In this case, wait for gtmrecv_wait_for_jnl_seqno to be set to TRUE
		 * by gtmrecv_poll_actions.
		 */
		if ((!connection_already_reset && repl_connection_reset) || gtmrecv_wait_for_jnl_seqno)
			break;
	}
	return;
}

/* This function is invoked on receipt of a REPL_NEED_HISTINFO message.
 * This in turn sends a REPL_HISTINFO message containing the history information.
 */
void gtmrecv_send_histinfo(repl_histinfo *cur_histinfo)
{
	repl_histinfo1_msg_t	histinfo1_msg;
	repl_histinfo2_msg_t	histinfo2_msg;
	repl_histinfo_msg_t	histinfo_msg;
	FILE			*log_fp;
	char			remote_proto_ver;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If sending history from a supplementary to a non-supplementary version, assert that the history record
	 * particularly the "strm_seqno" is 0 as a non-zero value is not understood by a non-supplementary instance.
	 */
	assert((NULL == this_side) || (this_side->is_supplementary == jnlpool.repl_inst_filehdr->is_supplementary));
	assert(!jnlpool.repl_inst_filehdr->is_supplementary || remote_side->is_supplementary || !cur_histinfo->strm_seqno);
	remote_proto_ver = remote_side->proto_ver;
	assert(REPL_PROTO_VER_MULTISITE <= remote_proto_ver);
	assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
	if (REPL_PROTO_VER_SUPPLEMENTARY > remote_proto_ver)
	{	/* Remote side does not support supplementary protocol. Send OLDER histinfo messages */
		/*************** Send REPL_OLD_TRIPLEINFO1 message ***************/
		histinfo1_msg.start_seqno = !remote_side->cross_endian ?
			cur_histinfo->start_seqno : GTM_BYTESWAP_64(cur_histinfo->start_seqno);
		memcpy(histinfo1_msg.instname, cur_histinfo->root_primary_instname, MAX_INSTNAME_LEN - 1);
		histinfo1_msg.instname[MAX_INSTNAME_LEN - 1] = '\0';
		gtmrecv_repl_send((repl_msg_ptr_t)&histinfo1_msg, REPL_OLD_TRIPLEINFO1, MIN_REPL_MSGLEN,
					"REPL_OLD_TRIPLEINFO1", cur_histinfo->start_seqno);
		if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
			return;
		/*************** Send REPL_OLD_TRIPLEINFO2 message ***************/
		if (!remote_side->cross_endian)
		{
			histinfo2_msg.start_seqno = cur_histinfo->start_seqno;
			histinfo2_msg.cycle = cur_histinfo->root_primary_cycle;
			histinfo2_msg.histinfo_num = cur_histinfo->histinfo_num;
		} else
		{
			histinfo2_msg.start_seqno = GTM_BYTESWAP_64(cur_histinfo->start_seqno);
			histinfo2_msg.cycle = GTM_BYTESWAP_32(cur_histinfo->root_primary_cycle);
			histinfo2_msg.histinfo_num = GTM_BYTESWAP_32(cur_histinfo->histinfo_num);
		}
		gtmrecv_repl_send((repl_msg_ptr_t)&histinfo2_msg, REPL_OLD_TRIPLEINFO2, MIN_REPL_MSGLEN,
					"REPL_OLD_TRIPLEINFO2", cur_histinfo->start_seqno);
		if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
			return;
	} else
	{	/* Remote side does support supplementary protocol. Send NEWER histinfo message. */
		histinfo_msg.history = *cur_histinfo;
		histinfo_msg.history.root_primary_instname[MAX_INSTNAME_LEN - 1] = '\0'; /* just in case */
		if (remote_side->cross_endian)
			ENDIAN_CONVERT_REPL_HISTINFO(&histinfo_msg.history);
		gtmrecv_repl_send((repl_msg_ptr_t)&histinfo_msg, REPL_HISTINFO, SIZEOF(repl_histinfo_msg_t),
					"REPL_HISTINFO", cur_histinfo->start_seqno);
	}
	log_fp = (NULL == gtmrecv_log_fp) ? stdout : gtmrecv_log_fp;
	repl_dump_histinfo(log_fp, TRUE, FALSE, "History sent", cur_histinfo);
}

STATICFNDEF void prepare_recvpool_for_write(int datalen, int pre_filter_write_len)
{
	recvpool_ctl_ptr_t	recvpool_ctl;

	recvpool_ctl = recvpool.recvpool_ctl;
	if (datalen > recvpool_size)
	{	/* Too large a transaction to be accommodated in the Receive Pool */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLTRANS2BIG, 5, &recvpool_ctl->jnl_seqno,
				datalen, pre_filter_write_len, LEN_AND_LIT("Receive"));
	}
	if ((write_loc + datalen) > recvpool_size)
	{
		REPL_DEBUG_ONLY(
			if (recvpool_ctl->wrapped)
				REPL_DPRINT1("Update Process too slow. Waiting for it to free up space and wrap\n");
		)
		while (recvpool_ctl->wrapped)
		{	/* Wait till the updproc wraps */
			SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
			GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
		}
		assert(recvpool_ctl->wrapped == FALSE);
		recvpool_ctl->write_wrap = write_wrap = write_loc;
		/* The update process reads (a) "recvpool_ctl->write" first. If "write" is not equal to
		 * "upd_proc_local->read", it then reads (b) "recvpool_ctl->write_wrap" and assumes that
		 * "write_wrap" holds a non-stale value. This is in turn used to compare "temp_read" and
		 * "write_wrap" to determine how much of unprocessed data there is in the receive pool. If
		 * it so happens that the receiver server sets "write_wrap" in the above line to a value
		 * that is lesser than its previous value (possible if in the previous wrap of the pool,
		 * transactions used more portions of the pool than in the current wrap), it is important
		 * that the update process sees the updated value of "write_wrap" as long as it sees the
		 * corresponding update to "write". This is because it will otherwise end up processing
		 * the tail section of the receive pool (starting from the uptodate value of "write" to the
		 * stale value of "write_wrap") that does not contain valid journal data. For this read order
		 * dependency to hold good, the receiver server needs to do a write memory barrier
		 * after updating "write_wrap" but before updating "write".  The update process
		 * will do a read memory barrier after reading "wrapped" but before reading "write".
		 */
		SHM_WRITE_MEMORY_BARRIER;
		/* The update process reads (a) "recvpool_ctl->wrapped" first and then reads (b)
		 * "recvpool_ctl->write". If "wrapped" is TRUE, it assumes that "write" will never hold a stale
		 * value that reflects a corresponding previous state of "wrapped" (i.e. "write" will point to
		 * the beginning of the receive pool, either 0 or a small non-zero value instead of pointing
		 * to the end of the receive pool). For this to hold good, the receiver server needs to do
		 * a write memory barrier after updating "write" but before updating "wrapped".  The update
		 * process will do a read memory barrier after reading "wrapped" but before reading "write".
		 */
		recvpool_ctl->write = write_loc = 0;
		SHM_WRITE_MEMORY_BARRIER;
		recvpool_ctl->wrapped = TRUE;
	}
	assert(buffered_data_len <= recvpool_size);
	DO_FLOW_CONTROL(write_loc);
}

STATICFNDEF void copy_to_recvpool(uchar_ptr_t databuff, int datalen)
{
	uint4			upd_read;
	uint4			future_write;
	upd_proc_local_ptr_t	upd_proc_local;
	recvpool_ctl_ptr_t	recvpool_ctl;

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	future_write = write_loc + datalen;
	upd_read = upd_proc_local->read;
	REPL_DEBUG_ONLY(
		if (recvpool_ctl->wrapped && (upd_read <= future_write))
		{
			REPL_DPRINT1("Update Process too slow. Waiting for it to free up space\n");
		}
	)
	while (recvpool_ctl->wrapped && (upd_read <= future_write))
	{	/* Write will cause overflow. Wait till there is more space available */
		SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
		GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
		upd_read = upd_proc_local->read;
	}
	memcpy(recvpool.recvdata_base + write_loc, databuff, datalen);
	write_loc = future_write;
	if (write_loc > write_wrap)
		write_wrap = write_loc;
}

STATICFNDEF void wait_for_updproc_to_clear_backlog(void)
{
	upd_proc_local_ptr_t	upd_proc_local;
	recvpool_ctl_ptr_t	recvpool_ctl;

	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	while (upd_proc_local->read != recvpool_ctl->write)
	{
		SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_PROGRESS);
		GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
	}
}

STATICFNDEF void process_tr_buff(int msg_type)
{
	recvpool_ctl_ptr_t		recvpool_ctl;
	seq_num				log_seqno, recv_jnl_seqno, upd_seqno, diff_seqno;
	uint4				in_size, out_size, out_bufsiz, tot_out_size, upd_read, max_strm_histinfo;
	boolean_t			filter_pass = FALSE, is_new_histrec, is_repl_cmpc;
	uchar_ptr_t			save_buffp, save_filter_buff, in_buff, out_buff;
	int				idx, status, num_strm_histinfo;
	qw_num				msg_total;
	repl_old_triple_jnl_t		old_triple_content;
	uLongf				destlen;
	int				cmpret, cur_data_len, rc;
	repl_msg_ptr_t			msgp, msgp_top;
	int4				histinfo_strm_num;
	uint4				write_len, pre_filter_write_len, pre_filter_write;
	boolean_t			uncmpfail;
	repl_histinfo			*cur_histinfo, *pool_histinfo, tmp_histinfo;
	repl_histrec_jnl_t		*pool_histrec, tmp_histjrec, rcvd_strm_histjrec[MAX_SUPPL_STRMS];
	static boolean_t		first_histrec = TRUE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	recvpool_ctl = recvpool.recvpool_ctl;
	is_repl_cmpc  = ((REPL_TR_CMP_JNL_RECS == msg_type) || (REPL_TR_CMP_JNL_RECS2 == msg_type));
	is_new_histrec = ((REPL_OLD_TRIPLE == msg_type) || (REPL_HISTREC == msg_type));
	assert(!is_repl_cmpc || !is_new_histrec);	/* HISTINFO records should not be compressed in the pipe */
	if (is_repl_cmpc)
	{
		assert(gtmrecv_max_repl_uncmpmsglen);
		destlen = gtmrecv_max_repl_uncmpmsglen;
		if (ZLIB_CMPLVL_NONE == gtm_zlib_cmp_level)
		{	/* Receiver does not have compression enabled in the first place but yet source server has sent
			 * compressed records. Stop source server from sending compressed records.
			 */
			uncmpfail = TRUE;
		} else
		{
			ZLIB_UNCOMPRESS(gtmrecv_uncmpmsgp, destlen, gtmrecv_cmpmsgp, gtmrecv_repl_cmpmsglen, cmpret);
			GTM_WHITE_BOX_TEST(WBTEST_REPL_TR_UNCMP_ERROR, cmpret, Z_DATA_ERROR);
			recv_jnl_seqno = recvpool_ctl->jnl_seqno;
			switch(cmpret)
			{
				case Z_MEM_ERROR:
					assert(FALSE);
					repl_log(gtmrecv_log_fp, TRUE, TRUE, GTM_ZLIB_Z_MEM_ERROR_STR
						GTM_ZLIB_UNCMP_ERR_SEQNO_STR, recv_jnl_seqno, recv_jnl_seqno);
					break;
				case Z_BUF_ERROR:
					assert(FALSE);
					repl_log(gtmrecv_log_fp, TRUE, TRUE, GTM_ZLIB_Z_BUF_ERROR_STR
						GTM_ZLIB_UNCMP_ERR_SEQNO_STR, recv_jnl_seqno, recv_jnl_seqno);
					break;
				case Z_DATA_ERROR:
					assert(gtm_white_box_test_case_enabled
						&& (WBTEST_REPL_TR_UNCMP_ERROR == gtm_white_box_test_case_number));
					repl_log(gtmrecv_log_fp, TRUE, TRUE, GTM_ZLIB_Z_DATA_ERROR_STR
						GTM_ZLIB_UNCMP_ERR_SEQNO_STR, recv_jnl_seqno, recv_jnl_seqno);
					break;
			}
			uncmpfail = (Z_OK != cmpret);
			if (!uncmpfail)
			{
				GTM_WHITE_BOX_TEST(WBTEST_REPL_TR_UNCMP_ERROR, destlen, gtmrecv_repl_uncmpmsglen - 1);
				if (destlen != gtmrecv_repl_uncmpmsglen)
				{	/* decompression did not yield precompressed data length */
					assert(gtm_white_box_test_case_enabled
						&& (WBTEST_REPL_TR_UNCMP_ERROR == gtm_white_box_test_case_number));
					repl_log(gtmrecv_log_fp, TRUE, TRUE, GTM_ZLIB_UNCMPLEN_ERROR_STR
						GTM_ZLIB_UNCMP_ERR_SEQNO_STR, destlen, gtmrecv_repl_uncmpmsglen,
						recv_jnl_seqno, recv_jnl_seqno);
					uncmpfail = TRUE;
				}
			}
		}
		if (uncmpfail)
		{	/* Since uncompression failed, default to NO compression. Send a REPL_CMP2UNCMP message accordingly */
			repl_log(gtmrecv_log_fp, TRUE, TRUE, GTM_ZLIB_UNCMPTRANSITION_STR);
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for update process to clear the backlog first\n");
			wait_for_updproc_to_clear_backlog();
			if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
				return;
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Update process has successfully cleared the backlog\n");
			gtmrecv_send_cmp2uncmp = TRUE;	/* trigger REPL_CMP2UNCMP message processing */
			gtmrecv_poll_actions(data_len, buff_unprocessed, buffp);
			assert(!gtmrecv_send_cmp2uncmp);
			assert(gtmrecv_wait_for_jnl_seqno);
			return;
		}
		assert(0 == destlen % REPL_MSG_ALIGN);
		msgp = (repl_msg_ptr_t)gtmrecv_uncmpmsgp;
		msgp_top = (repl_msg_ptr_t)(gtmrecv_uncmpmsgp + destlen);
	}
	do
	{
		assert(remote_side->endianness_known);	/* only then is remote_side->cross_endian reliable */
		if (is_repl_cmpc)
		{
			if (msgp >= msgp_top)
			{
				assert(msgp == msgp_top);
				break;
			}
			/* If primary is of different endianness, endian convert the UNCOMPRESSED message header
			 * before using the type and len fields (the compressed message header was already endian
			 * converted as part of receiving the message in do_main_loop())
			 */
			if (remote_side->cross_endian)
			{
				msgp->type = GTM_BYTESWAP_32(msgp->type);
				msgp->len = GTM_BYTESWAP_32(msgp->len);
			}
			assert(REPL_TR_JNL_RECS == msgp->type);
			cur_data_len = msgp->len - REPL_MSG_HDRLEN;
			assert(0 < cur_data_len);
			assert(0 == (cur_data_len % REPL_MSG_ALIGN));
			PREPARE_RECVPOOL_FOR_WRITE(cur_data_len, 0);	/* could update "recvpool_ctl->write" and "write_loc" */
			COPY_TO_RECVPOOL((uchar_ptr_t)msgp + REPL_MSG_HDRLEN, cur_data_len);/* uses and updates "write_loc" */
			msgp = (repl_msg_ptr_t)((uchar_ptr_t)msgp + cur_data_len + REPL_MSG_HDRLEN);
		}
		write_off = recvpool_ctl->write;
		write_len = (write_loc - write_off);
		assert((write_off != write_wrap) || (0 == write_off));
		assert(remote_side->jnl_ver);
		assert(!remote_side->cross_endian || (V18_JNL_VER <= remote_side->jnl_ver));
		if (ENDIAN_CONVERSION_NEEDED(is_new_histrec, this_side->jnl_ver, remote_side->jnl_ver, remote_side->cross_endian))
		{
			if (SS_NORMAL != (status = repl_tr_endian_convert(remote_side->jnl_ver,
							recvpool.recvdata_base + write_off, write_len)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLXENDIANFAIL, 3, LEN_AND_LIT("Replicating"),
						&recvpool.upd_proc_local->read_jnl_seqno);
		}
		if (!is_new_histrec)
		{
			if (NO_FILTER != gtmrecv_filter)
			{	/* Need to pass through filter */
				pre_filter_write = write_off;
				pre_filter_write_len = write_len;
				if (gtmrecv_filter & INTERNAL_FILTER)
				{
					in_buff = recvpool.recvdata_base + write_off;
					in_size = write_len;
					out_buff = repl_filter_buff;
					out_bufsiz = repl_filter_bufsiz;
					tot_out_size = 0;
					while (SS_NORMAL != (status =
						repl_filter_old2cur[remote_side->jnl_ver - JNL_VER_EARLIEST_REPL](
								in_buff, &in_size, out_buff, &out_size, out_bufsiz))
						&& (EREPL_INTLFILTER_NOSPC == repl_errno))
					{
						save_filter_buff = repl_filter_buff;
						gtmrecv_alloc_filter_buff(repl_filter_bufsiz + (repl_filter_bufsiz >> 1));
						in_buff += in_size;
						in_size = (uint4)(pre_filter_write_len -
								(in_buff - recvpool.recvdata_base - write_off));
						out_bufsiz = (uint4)(repl_filter_bufsiz - (out_buff - save_filter_buff) - out_size);
						out_buff = repl_filter_buff + (out_buff - save_filter_buff) + out_size;
						tot_out_size += out_size;
					}
					if (SS_NORMAL == status)
						write_len = tot_out_size + out_size;
					else
					{
						assert(EREPL_INTLFILTER_SECNODZTRIGINTP == repl_errno);
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
				GTMTRIG_ONLY(
					if ((unsigned char)V19_JNL_VER <= remote_side->jnl_ver)
					{
						repl_sort_tr_buff(repl_filter_buff, write_len);
						DBG_VERIFY_TR_BUFF_SORTED(repl_filter_buff, write_len);
					}
				)
				if ((gtmrecv_filter & EXTERNAL_FILTER) &&
				    (SS_NORMAL != (status = repl_filter(recvpool_ctl->jnl_seqno, repl_filter_buff, (int*)&write_len,
									repl_filter_bufsiz))))
					repl_filter_error(recvpool_ctl->jnl_seqno, status);
				GTMTRIG_ONLY(
					/* Ensure that the external filter has not disturbed the sorted sequence of the
					 * update_num
					 */
					DEBUG_ONLY(
						if ((unsigned char)V19_JNL_VER <= remote_side->jnl_ver)
							DBG_VERIFY_TR_BUFF_SORTED(repl_filter_buff, write_len);
					)
				)
				assert(write_len <= repl_filter_bufsiz);
				write_loc = write_off;		/* reset "write_loc" */
				PREPARE_RECVPOOL_FOR_WRITE(write_len, pre_filter_write_len);	/* could update "->write"
												 * and "write_loc" */
				COPY_TO_RECVPOOL((uchar_ptr_t)repl_filter_buff, write_len);/* uses and updates "write_loc" */
				write_off = recvpool_ctl->write;
				repl_recv_postfltr_data_procd += (qw_num)write_len;
				filter_pass = TRUE;
			} else
			{
				GTMTRIG_ONLY(
					if ((unsigned char)V19_JNL_VER <= remote_side->jnl_ver)
					{
						repl_sort_tr_buff((uchar_ptr_t)(recvpool.recvdata_base + write_off), write_len);
						DBG_VERIFY_TR_BUFF_SORTED((recvpool.recvdata_base + write_off), write_len);
					}
				)
			}
		}
		if (recvpool_ctl->jnl_seqno - lastlog_seqno >= log_interval)
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
					log_seqno, log_seqno,
					repl_recv_data_processed, repl_recv_data_processed,
					repl_recv_postfltr_data_procd, repl_recv_postfltr_data_procd,
					msg_total, msg_total, diff_seqno, diff_seqno);
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
					"%00.f  Tr recvd : "INT8_FMT"  Tr bytes : "INT8_FMT"  Msg bytes : "INT8_FMT"\n",
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
				repl_log(gtmrecv_log_fp, FALSE, FALSE, "Tr : "INT8_FMT"  Size : %d  Write : %d  "
					 "Total : "INT8_FMT"\n", recvpool_ctl->jnl_seqno, write_len,
					 write_off, repl_recv_data_processed);
			} else
			{
				assert(!is_new_histrec);
				repl_log(gtmrecv_log_fp, FALSE, FALSE, "Tr : "INT8_FMT"  Pre filter Size : %d  "
					"Post filter Size  : %d  Pre filter Write : %d  Post filter Write : %d  "
					"Pre filter Total : "INT8_FMT"  Post filter Total : "INT8_FMT"\n",
					recvpool_ctl->jnl_seqno, pre_filter_write_len, write_len,
					pre_filter_write, write_off, repl_recv_data_processed,
					repl_recv_postfltr_data_procd);
			}
		}
		recvpool_ctl->write_wrap = write_wrap;
		if (recvpool.gtmrecv_local->noresync)
		{	/* With -NORESYNC, we could take the strm_seqno further back with multiple connects of the same receiver
			 * server with different source servers at different seqnos. So avoid any confusion with asserts
			 * that check for increasing seqnos using last_valid_histinfo and last_rcvd_histinfo.
			 */
			memset(&recvpool.recvpool_ctl->last_valid_histinfo, 0, SIZEOF(recvpool.recvpool_ctl->last_valid_histinfo));
			memset(&recvpool.recvpool_ctl->last_rcvd_histinfo, 0, SIZEOF(recvpool.recvpool_ctl->last_rcvd_histinfo));
			/* In the case of -NORESYNC, we are a root primary supplementary instance (not a propagating primary)
			 * and therefore recvpool.recvpool_ctl->last_valid_strm_histinfo[] and
			 * recvpool.recvpool_ctl->last_rcvd_strm_histinfo[] are unused and hence no need to reset them.
			 */
			assert(!remote_side->is_supplementary);	/* assert that we are not a propagating primary supplementary */
			assert(jnlpool.repl_inst_filehdr->is_supplementary && !jnlpool_ctl->upd_disabled);
		}
		if (is_new_histrec)
		{
			/* Note: The REPL_OLD_TRIPLE or REPL_HISTREC messages are endian converted by the source server
			 * in case the receiver is running with a this_side->jnl_ver higher than the source. If not, the call to
			 * function "repl_tr_endian_convert" (done already in the current function) will take care of
			 * the endian conversion. So no more endian conversion needed at this point.
			 */
			if (REPL_OLD_TRIPLE == msg_type)
			{
				assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
				assert(REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver);
				assert(SIZEOF(old_triple_content) == write_len);
				memcpy((sm_uc_ptr_t)&old_triple_content, (recvpool.recvdata_base + write_off),
					SIZEOF(old_triple_content));
				assert(JRT_TRIPLE == old_triple_content.jrec_type);
				assert(old_triple_content.forwptr == SIZEOF(old_triple_content));
				assert(old_triple_content.start_seqno == recvpool_ctl->jnl_seqno);
				assert(old_triple_content.start_seqno >= recvpool.upd_proc_local->read_jnl_seqno);
				assert((old_triple_content.start_seqno > recvpool_ctl->last_valid_histinfo.start_seqno)
					 || ((old_triple_content.start_seqno == recvpool_ctl->last_valid_histinfo.start_seqno)
						&& gtm_white_box_test_case_enabled
						&& (WBTEST_UPD_PROCESS_ERROR == gtm_white_box_test_case_number)));
				cur_histinfo = &tmp_histjrec.histcontent;
				memcpy(cur_histinfo->root_primary_instname, old_triple_content.instname, MAX_INSTNAME_LEN - 1);
				cur_histinfo->root_primary_instname[MAX_INSTNAME_LEN - 1] = '\0';
				cur_histinfo->start_seqno = old_triple_content.start_seqno;
				cur_histinfo->strm_seqno = 0;
				cur_histinfo->root_primary_cycle = old_triple_content.cycle;
				cur_histinfo->creator_pid = 0;
				cur_histinfo->created_time = 0;
				/* No need to initialize the following fields as they are reinitialized later
				 * when the history record gets added to the instance file (in "repl_inst_histinfo_add").
				 *	cur_histinfo->histinfo_num = INVALID_HISTINFO_NUM;
				 *	cur_histinfo->prev_histinfo_num = INVALID_HISTINFO_NUM;
				 *	cur_histinfo->last_histinfo_num[] = INVALID_HISTINFO_NUM;
				 */
				cur_histinfo->strm_index = 0;
				cur_histinfo->history_type = HISTINFO_TYPE_NORMAL;
				NULL_INITIALIZE_REPL_INST_UUID(cur_histinfo->lms_group);
				tmp_histjrec.jrec_type = JRT_HISTREC;
				tmp_histjrec.forwptr = SIZEOF(repl_histrec_jnl_t);
				/* We now have to upgrade a REPL_OLD_TRIPLE format history record to a REPL_HISTREC record.
				 * The latter is a much bigger record so make room for this in the receive pool.
				 */
				write_loc = write_off;		/* reset "write_loc" */
				write_len = tmp_histjrec.forwptr;
				PREPARE_RECVPOOL_FOR_WRITE(write_len, SIZEOF(old_triple_content));
					/* above macro could update "recvpool_ctl->write" and "write_loc" */
				COPY_TO_RECVPOOL((uchar_ptr_t)&tmp_histjrec, write_len);/* uses and updates "write_loc" */
				write_off = recvpool_ctl->write;
				/* Copy relevant fields from received histinfo message to "last_rcvd_histinfo" if applicable */
				cur_histinfo = &recvpool_ctl->last_rcvd_histinfo;
				assert(old_triple_content.start_seqno >= cur_histinfo->start_seqno);
				*cur_histinfo = tmp_histjrec.histcontent;
				assert(!remote_side->is_supplementary); /* so no need to maintain last_rcvd_strm_histinfo[] */
			} else
			{
				assert(REPL_HISTREC == msg_type);
				assert(REPL_PROTO_VER_SUPPLEMENTARY <= remote_side->proto_ver);
				assert(SIZEOF(repl_histrec_jnl_t) == write_len);
				pool_histrec = (repl_histrec_jnl_t *)((sm_uc_ptr_t)recvpool.recvdata_base + write_off);
				assert(JRT_HISTREC == pool_histrec->jrec_type);
				assert(pool_histrec->forwptr == SIZEOF(repl_histrec_jnl_t));
				pool_histinfo = &pool_histrec->histcontent;
				assert(pool_histinfo->start_seqno == recvpool_ctl->jnl_seqno);
				assert(pool_histinfo->start_seqno >= recvpool.upd_proc_local->read_jnl_seqno);
				assert(jnlpool.jnlpool_ctl == jnlpool_ctl);
				if (jnlpool.repl_inst_filehdr->is_supplementary && !jnlpool_ctl->upd_disabled)
				{	/* Modify the history record to reflect the stream # */
					assert((0 <= recvpool.gtmrecv_local->strm_index)
						&& (MAX_SUPPL_STRMS > recvpool.gtmrecv_local->strm_index));
					assert(strm_index == recvpool.gtmrecv_local->strm_index);
					pool_histinfo->strm_index = recvpool.gtmrecv_local->strm_index;
					assert(0 == pool_histinfo->strm_seqno);
					assert(IS_REPL_INST_UUID_NON_NULL(recvpool.gtmrecv_local->remote_lms_group));
					pool_histinfo->lms_group = recvpool.gtmrecv_local->remote_lms_group;
					if (recvpool.gtmrecv_local->updateresync)
					{
						assert(FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd);
						/* Make it known that this is an updateresync type history record */
						pool_histinfo->history_type = HISTINFO_TYPE_UPDRESYNC;
					}
					/* We want to do a similar check for -noresync but we cannot use gtmrecv_local->noresync
					 * since that is reset the moment a REPL_WILL_RESTART_WITH_INFO message is seen (much
					 * before coming here). So use a static variable to determine if this is the first
					 * history record that is being received by this receiver server and if -noresync was
					 * specified in the command line (indicated by gtmrecv_options.noresync), set the
					 * history_type of the incoming history record to reflect the noresync type.
					 */
					assert(!recvpool.gtmrecv_local->noresync);
					if (first_histrec)
					{
						if (gtmrecv_options.noresync)
							pool_histinfo->history_type = HISTINFO_TYPE_NORESYNC;
						first_histrec = FALSE;
					}
				}
				assert((INVALID_SUPPL_STRM != strm_index) || (0 == pool_histinfo->strm_index));
				if ((INVALID_SUPPL_STRM == strm_index) || (strm_index == pool_histinfo->strm_index))
				{
					assert((pool_histinfo->start_seqno > recvpool_ctl->last_valid_histinfo.start_seqno)
						 || ((pool_histinfo->start_seqno == recvpool_ctl->last_valid_histinfo.start_seqno)
							&& gtm_white_box_test_case_enabled
							&& (WBTEST_UPD_PROCESS_ERROR == gtm_white_box_test_case_number)));
					cur_histinfo = &recvpool_ctl->last_rcvd_histinfo;
					assert(pool_histinfo->start_seqno >= cur_histinfo->start_seqno);
					*cur_histinfo = *pool_histinfo;
				} else
				{
					assert(!recvpool_ctl->insert_strm_histinfo);
					cur_histinfo = pool_histinfo;
				}
			}
			/* If supplementary instance with updates disabled, initialize last_rcvd_strm_histinfo as well */
			if (remote_side->is_supplementary)
			{	/* Check if "insert_strm_histinfo" is set. If so and if REPL_STRMINFO messages were
				 * exchanged between the source and receiver, we need to potentially insert history
				 * records for valid stream #s > 0. This is to ensure that even in case of a
				 * -updateresync startup, we record knowledge of all streams that exist on the source side
				 * (which we previously received as part of the same connection) on the receiver side as well.
				 * Examples of when REPL_STRMINFO messages are not exchanged are if receiver side is at
				 * jnl_seqno = 1, or if the receiver side is at the exact same jnl_seqno as the source side
				 * and no prior history is available.
				 */
				if (recvpool_ctl->insert_strm_histinfo
					&& (max_strm_histinfo = recvpool_ctl->max_strm_histinfo)) /* caution: assignment */
				{	/* Make room for the new records in the receive pool first.
					 * At this point "cur_histinfo" holds the first received history record.
					 * Add the other stream history records onto it.
					 */
					assert(0 == cur_histinfo->strm_index);
					/* Determine how many history records need to be added */
					num_strm_histinfo = 0;
					/* Copy 0th stream history record into receive pool first */
					rcvd_strm_histjrec[num_strm_histinfo].jrec_type = JRT_HISTREC;
					rcvd_strm_histjrec[num_strm_histinfo].forwptr = SIZEOF(repl_histrec_jnl_t);
					cur_histinfo->strm_seqno = jnlpool_ctl->strm_seqno[0];
					rcvd_strm_histjrec[num_strm_histinfo++].histcontent = *cur_histinfo;
					/* Copy non-zero stream history records if they exist */
					for (idx = 1; idx < max_strm_histinfo; idx++)
					{
						if (recvpool_ctl->is_valid_strm_histinfo[idx])
						{
							DEBUG_ONLY(tmp_histinfo = recvpool_ctl->last_rcvd_strm_histinfo[idx]);
							assert(IS_REPL_INST_UUID_NON_NULL(tmp_histinfo.lms_group));
							rcvd_strm_histjrec[num_strm_histinfo].jrec_type = JRT_HISTREC;
							rcvd_strm_histjrec[num_strm_histinfo].forwptr = SIZEOF(repl_histrec_jnl_t);
							rcvd_strm_histjrec[num_strm_histinfo].histcontent
								= recvpool_ctl->last_rcvd_strm_histinfo[idx];
							/* Fix the "start_seqno" & "strm_seqno" fields of the history record */
							rcvd_strm_histjrec[num_strm_histinfo].histcontent.start_seqno
								= cur_histinfo->start_seqno;
							/* Note that jnlpool_ctl->strm_seqno[idx] could be 0 if we have not yet
							 * seen any updates in this stream. But since we do have a history record
							 * for this stream and its strm_seqno cannot be 0 (has to be non-zero),
							 * set it to 1 in that case.
							 */
							rcvd_strm_histjrec[num_strm_histinfo].histcontent.strm_seqno
								= jnlpool_ctl->strm_seqno[idx] ? jnlpool_ctl->strm_seqno[idx] : 1;
							num_strm_histinfo++;
							/* Do not reset recvpool_ctl->is_valid_strm_histinfo[idx] to FALSE
							 * as this reflects reality and staying this way helps later
							 * communication with the source in cases of reconnects/pipe-drains.
							 */
						}
					}
					write_loc = write_off;		/* reset "write_loc" */
					write_len = num_strm_histinfo * SIZEOF(repl_histrec_jnl_t);
					PREPARE_RECVPOOL_FOR_WRITE(write_len, 0);
						/* could update "recvpool_ctl->write" * and "write_loc" */
					COPY_TO_RECVPOOL((uchar_ptr_t)rcvd_strm_histjrec, write_len);
						/* uses and updates "write_loc" */
					write_off = recvpool_ctl->write;
					/* Note: "last_rcvd_strm_histinfo" is automatically maintained in this case */
					/* Now that we have inserted the stream specific history records, reset flag */
					recvpool_ctl->insert_strm_histinfo = FALSE;
				} else
				{	/* Maintain last_rcvd_strm_histinfo[] as well.
					 * In addition, if insert_strm_histinfo is TRUE, it means no REPL_STRMINFO messages
					 * were exchanged at startup. In that case, no need of inserting stream specific
					 * history information in the receive pool so reset variable to FALSE.
					 */
					if (recvpool_ctl->insert_strm_histinfo)
						recvpool_ctl->insert_strm_histinfo = FALSE;
					histinfo_strm_num = cur_histinfo->strm_index;
					assert((0 <= histinfo_strm_num) && (MAX_SUPPL_STRMS > histinfo_strm_num));
					memcpy(&recvpool_ctl->last_rcvd_strm_histinfo[histinfo_strm_num],
							cur_histinfo, SIZEOF(repl_histinfo));
					recvpool_ctl->is_valid_strm_histinfo[histinfo_strm_num] = TRUE;
					if (recvpool_ctl->max_strm_histinfo <= histinfo_strm_num)
						recvpool_ctl->max_strm_histinfo = histinfo_strm_num + 1;
				}
			}
			repl_dump_histinfo(gtmrecv_log_fp, TRUE, TRUE, "New History Content", cur_histinfo);
		} else
		{	/* Note: In case of a propagating primary supplementary instance, the below if implies that we will
			 * change last_rcvd_histinfo to last_valid_histinfo even if the logical update corresponded to a
			 * different stream than 0 (the stream corresponding to the history of interest on this receiver).
			 * But there should not be any issues due to this as it is an update nevertheless and is going to
			 * bump up the recvpool_ctl->jnl_seqno to one more than last_rcvd_histinfo.start_seqno.
			 */
			if (recvpool_ctl->jnl_seqno == recvpool_ctl->last_rcvd_histinfo.start_seqno)
			{	/* Move over stuff from "last_rcvd_histinfo" to "last_valid_histinfo" */
				memcpy(&recvpool_ctl->last_valid_histinfo,
					&recvpool_ctl->last_rcvd_histinfo, SIZEOF(repl_histinfo));
				if (remote_side->is_supplementary)
				{	/* Propagating primary supplementary instance. Maintain last_valid_strm_histinfo too. */
					max_strm_histinfo = recvpool_ctl->max_strm_histinfo;
					assert(max_strm_histinfo);
					for (idx = 0; idx < max_strm_histinfo; idx++)
					{
						if (recvpool_ctl->is_valid_strm_histinfo[idx])
						{
							DEBUG_ONLY(tmp_histinfo = recvpool_ctl->last_rcvd_strm_histinfo[idx]);
							assert((0 == idx) || IS_REPL_INST_UUID_NON_NULL(tmp_histinfo.lms_group));
							assert((0 != idx) || IS_REPL_INST_UUID_NULL(tmp_histinfo.lms_group));
							memcpy(&recvpool_ctl->last_valid_strm_histinfo[idx],
								&recvpool_ctl->last_rcvd_strm_histinfo[idx], SIZEOF(repl_histinfo));
							recvpool_ctl->is_valid_strm_histinfo[idx] = FALSE;
						}
					}
					recvpool_ctl->max_strm_histinfo = 0;
					/* Now that last_valid_strm_histinfo[0] is initialized, it is safe to reset the below */
					if (recvpool_ctl->insert_strm_histinfo)
						recvpool_ctl->insert_strm_histinfo = FALSE;
				}
				/* Now that at least one history record has been written into the receive pool and is guaranteed
				 * to be written to the instance file (when this gets processed by the update process), dont use
				 * -updateresync or -noresync for future handshakes in case the current connection gets reset.
				 */
				if (recvpool.gtmrecv_local->updateresync)
				{
					recvpool.gtmrecv_local->updateresync = FALSE;
					/* Close fd of the input instance file name used for the -updateresync */
					if (FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd)
						CLOSEFILE_RESET(recvpool.gtmrecv_local->updresync_instfile_fd, rc);
				}
				assert(!recvpool.gtmrecv_local->noresync);	/* should have been reset already */
			}
			QWINCRBYDW(recvpool_ctl->jnl_seqno, 1);
			assert(recvpool_ctl->last_valid_histinfo.start_seqno < recvpool_ctl->jnl_seqno);
		}
		/* The update process looks at "recvpool_ctl->write" first and then reads (a) "recvpool_ctl->write_wrap"
		 * AND (b) all journal data in the receive pool upto this offset. It assumes that (a) and (b) will never
		 * hold stale values corresponding to a previous state of "recvpool_ctl->write". In order for this
		 * assumption to hold good, the receiver server needs to do a write memory barrier after updating the
		 * receive pool data and "write_wrap" but before updating "write". The update process will do a read
		 * memory barrier after reading "write" but before reading "write_wrap" or the receive pool data. Not
		 * enforcing the read order will result in the update process attempting to read/process invalid data
		 * from the receive pool (which could end up in db out of sync situation between primary and secondary).
		 */
		SHM_WRITE_MEMORY_BARRIER;
		recvpool_ctl->write = write_loc;
	} while (is_repl_cmpc);
	return;
}

/* Retrieve the history record corresponding to "input_seqno" in the instance file used with the -UPDATERESYNC qualifier.
 * Do special processing in case of P->Q type of connection (where primary and secondary are both supplementary instances)
 * which is to search only in the 0th stream.
 *
 * Note: This function is similar to "repl_inst_histinfo_find_seqno" except that this operates on the input instance file
 * provided with the -updateresync qualifier. Two reasons why we need this code duplication is
 *	a) The "repl_inst_histinfo_find_seqno" function currently is coded to use only the replication instance file.
 *	b) The input instance file for -updateresync could be cross endian.
 * If "repl_inst_histinfo_find_seqno" is enhanced to fix these limitations, then we can avoid this code duplication.
 */
STATICFNDEF void	gtmrecv_updresync_histinfo_find_seqno(seq_num input_seqno, int4 strm_num, repl_histinfo *histinfo)
{
	char		print_msg[1024];
	int		fd, status;
	int4		histinfo_num;
	off_t		offset;

	fd = recvpool.gtmrecv_local->updresync_instfile_fd;
	assert(FD_INVALID != fd);
	/* If remote side is non-supplementary then its instance file (which is given as input to the -updateresync
	 * command) knows only strem 0, so reset strm_num to 0 unconditionally.
	 */
	if (!remote_side->is_supplementary)
		strm_num = 0;
	if (INVALID_SUPPL_STRM == strm_num)
		histinfo_num = recvpool.gtmrecv_local->updresync_num_histinfo;
	else
		histinfo_num = recvpool.gtmrecv_local->updresync_num_histinfo_strm[strm_num];
	if (INVALID_HISTINFO_NUM == histinfo_num)
	{	/* The instance file cannot be used for updateresync if it has NO history records. */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_UPDSYNCINSTFILE, 0, ERR_STRMNUMIS, 1, strm_num, ERR_TEXT, 2,
				LEN_AND_LIT("Input instance file has NO history records"));
	}
	assert(0 <= histinfo_num);
	if (!recvpool.gtmrecv_local->updresync_jnl_seqno)
	{	/* The instance file cannot be used for updateresync if it has a ZERO seqno */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_UPDSYNCINSTFILE, 0, ERR_STRMNUMIS, 1, strm_num,
			ERR_TEXT, 2, LEN_AND_LIT("Input instance file has jnl_seqno of 0"));
	}
	if (input_seqno > recvpool.gtmrecv_local->updresync_jnl_seqno)
	{	/* Input seqno is greater than the max seqno in the updateresync input instance file. So can never be found. */
		SNPRINTF(print_msg, SIZEOF(print_msg), "Seqno "INT8_FMT" "INT8_FMTX" cannot be found in input instance file "
			" which has a max seqno of "INT8_FMT" "INT8_FMTX"\n", input_seqno, input_seqno,
			recvpool.gtmrecv_local->updresync_jnl_seqno, recvpool.gtmrecv_local->updresync_jnl_seqno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_UPDSYNCINSTFILE, 0,
				ERR_STRMNUMIS, 1, strm_num, ERR_TEXT, 2, LEN_AND_STR(print_msg));
	}
	histinfo->start_seqno = 0;
	do
	{
		offset = REPL_INST_HISTINFO_START + ((histinfo_num) * SIZEOF(repl_histinfo));
		LSEEKREAD(fd, offset, histinfo, SIZEOF(repl_histinfo), status);
		if (0 != status)
		{	/* At this point, we dont have the name of the input instance file used in the -updateresync qualifier.
			 * So we use a value of "" instead. The fact that the REPLINSTREAD message is preceded by a UPDSYNCINSTFILE
			 * error indicates to the user it is the -updateresync qualifier where the issue is so it is not a big loss.
			 */
			if (-1 == status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(15) ERR_UPDSYNCINSTFILE, 0,
					ERR_STRMNUMIS, 1, strm_num,
					ERR_TEXT, 2, LEN_AND_LIT("Error reading history record"),
					ERR_REPLINSTREAD, 4, SIZEOF(repl_histinfo), (qw_off_t *)&offset, LEN_AND_LIT(""));
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(16) ERR_UPDSYNCINSTFILE, 0,
					ERR_STRMNUMIS, 1, strm_num,
					ERR_TEXT, 2, LEN_AND_LIT("Error reading history record"),
					ERR_REPLINSTREAD, 4, SIZEOF(repl_histinfo), (qw_off_t *)&offset, LEN_AND_LIT(""), status);
		}
		if (recvpool.gtmrecv_local->updresync_cross_endian)
			ENDIAN_CONVERT_REPL_HISTINFO(histinfo);
		if (input_seqno > histinfo->start_seqno)
			return;		/* found history record corresponding to input_seqno. return right away */
		histinfo_num = (INVALID_SUPPL_STRM == strm_num) ? (histinfo_num - 1) : histinfo->prev_histinfo_num;
	} while (INVALID_HISTINFO_NUM != histinfo_num);
	/* Could not find history record in -updateresync= input instance file */
	SNPRINTF(print_msg, SIZEOF(print_msg),
			"Receiver side instance seqno "INT8_FMT" "INT8_FMTX" is less than"
			" any history record found in instance file", input_seqno, input_seqno);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_UPDSYNCINSTFILE, 0,
			ERR_STRMNUMIS, 1, strm_num, ERR_TEXT, 2, LEN_AND_STR(print_msg));
}

/* Retrieve the "index"'th history record in the instance file specified using the -UPDATERESYNC qualifier.
 *
 * This function is similar to "repl_inst_histinfo_get" except that this operates on the input instance file provided
 * with the -updateresync qualifier. Two reasons why we need this code duplication is
 *	a) The "repl_inst_histinfo_get" function currently is coded to use only the replication instance file.
 *	b) The input instance file for -updateresync could be cross endian.
 * If "repl_inst_histinfo_get" is enhanced to fix these limitations, then we can avoid this code duplication.
 */
STATICFNDEF void	gtmrecv_updresync_histinfo_get(int4 index, repl_histinfo *histinfo)
{
	int		fd, status;
	off_t		offset;

	fd = recvpool.gtmrecv_local->updresync_instfile_fd;
	assert(FD_INVALID != fd);
	assert(0 <= index);
	offset = REPL_INST_HISTINFO_START + ((index) * SIZEOF(repl_histinfo));
	LSEEKREAD(fd, offset, histinfo, SIZEOF(repl_histinfo), status);
	if (0 != status)
	{	/* At this point, we dont have the name of the input instance file used in the -updateresync qualifier.
		 * So we use a value of "" instead. The fact that the REPLINSTREAD message is preceded by a UPDSYNCINSTFILE
		 * error indicates to the user it is the -updateresync qualifier where the issue is so it is not a big loss.
		 */
		if (-1 == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_UPDSYNCINSTFILE, 0,
				ERR_TEXT, 2, LEN_AND_LIT("Error reading history record"),
				ERR_REPLINSTREAD, 4, SIZEOF(repl_histinfo), (qw_off_t *)&offset, LEN_AND_LIT(""));
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_UPDSYNCINSTFILE, 0,
				ERR_TEXT, 2, LEN_AND_LIT("Error reading history record"),
				ERR_REPLINSTREAD, 4, SIZEOF(repl_histinfo), (qw_off_t *)&offset, LEN_AND_LIT(""), status);
	}
	if (recvpool.gtmrecv_local->updresync_cross_endian)
		ENDIAN_CONVERT_REPL_HISTINFO(histinfo);
}

/* This function is invoked on receipt of a REPL_NEED_STRMINFO message. In case of an error, it returns if either
 * repl_connection_reset OR gtmrecv_wait_for_jnl_seqno are set so the caller should check for this and return as well.
 */
STATICFNDEF void	gtmrecv_process_need_strminfo_msg(repl_needstrminfo_msg_ptr_t need_strminfo_msg)
{
	int4			status;
	int			idx;
	repl_histinfo		histinfo, *previously_rcvd_histinfo;
	repl_strminfo_msg_t	strminfo_msg;
	seq_num			need_strminfo_seqno, last_valid_histinfo_seqno;

	assert(remote_side->is_supplementary);	/* STRMINFO messages are sent only in case source and receiver are supplementary */
	assert(0 == strm_index);
	assert(remote_side->endianness_known); /* ensure remote_side->cross_endian is reliable */
	if (!remote_side->cross_endian)
		need_strminfo_seqno = need_strminfo_msg->seqno;
	else
		need_strminfo_seqno = GTM_BYTESWAP_64(need_strminfo_msg->seqno);
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_NEED_STRMINFO message for seqno "INT8_FMT" "INT8_FMTX"\n",
			need_strminfo_seqno, need_strminfo_seqno);
	if (recvpool.gtmrecv_local->updateresync && (FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd))
	{	/* The stream information that is being requested needs to be found in the -updateresync input instance file
		 * (not the receiver side instance file).
		 */
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Searching for desired stream info in -updateresync input instance file\n");
		gtmrecv_updresync_histinfo_find_seqno(need_strminfo_seqno, INVALID_SUPPL_STRM, &histinfo);
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			strminfo_msg.last_histinfo_num[idx] = histinfo.last_histinfo_num[idx];
		assert(recvpool.recvpool_ctl->insert_strm_histinfo);
	} else
	{	/* The history record needs to be found in the receiver side instance file or in the receive pool.
		 * If last_valid_strm_histinfo[0] has non-default content, then because this is a supplementary instance
		 * and a propagating primary, we can rest assured that last_valid_strm_histinfo[1] thru [15] reflect
		 * the latest history records for each stream if the stream exists on this instance. And so no need
		 * to go to the instance file at all. If [0] does not have any content, then it means the cached history
		 * is empty for not just the 0th stream but for every other stream as well i.e. the receive pool is empty
		 * and the receiver is connecting with a source for the first time. So go to the instance file in that case.
		 */
		last_valid_histinfo_seqno = recvpool.recvpool_ctl->last_valid_strm_histinfo[0].start_seqno;
		if (last_valid_histinfo_seqno)
		{
			assert(need_strminfo_seqno > last_valid_histinfo_seqno);
			assert(!recvpool.gtmrecv_local->updateresync);
			assert(!recvpool.gtmrecv_local->noresync);
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Searching for the desired history in the receive pool\n");
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			{
				previously_rcvd_histinfo = &recvpool.recvpool_ctl->last_valid_strm_histinfo[idx];
				assert((0 != idx) || IS_REPL_INST_UUID_NULL(previously_rcvd_histinfo->lms_group));
				if ((0 == idx) || IS_REPL_INST_UUID_NON_NULL(previously_rcvd_histinfo->lms_group))
					strminfo_msg.last_histinfo_num[idx] = UNKNOWN_HISTINFO_NUM;
				else
				{
					assert(0 != idx);
					strminfo_msg.last_histinfo_num[idx] = INVALID_HISTINFO_NUM;
				}
			}
			assert(!recvpool.recvpool_ctl->insert_strm_histinfo);
			histinfo.strm_index = 0;	/* so "0 < histinfo.strm_index" if block is skipped below for this case */
		} else
		{
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
				"Searching for the desired history in the replication instance file\n");
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
			GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
				/* above macro will "return" if repl_connection_reset OR gtmrecv_wait_for_jnl_seqno is set */
			status = repl_inst_wrapper_histinfo_find_seqno(need_strminfo_seqno, INVALID_SUPPL_STRM, &histinfo);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			if (0 != status)
			{	/* Close the connection */
				assert(ERR_REPLINSTNOHIST == status);
				assert(FALSE);
				gtmrecv_autoshutdown();	/* should not return */
			}
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				strminfo_msg.last_histinfo_num[idx] = histinfo.last_histinfo_num[idx];
		}
	}
	if (0 < histinfo.strm_index)
	{
		assert(histinfo.last_histinfo_num[histinfo.strm_index] < histinfo.histinfo_num);
		strminfo_msg.last_histinfo_num[histinfo.strm_index] = histinfo.histinfo_num;
	}
	if (remote_side->cross_endian)
	{
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		{
			assert(4 == SIZEOF(strminfo_msg.last_histinfo_num[idx]));	/* so GTM_BYTESWAP_32 can be used below */
			strminfo_msg.last_histinfo_num[idx] = GTM_BYTESWAP_32(strminfo_msg.last_histinfo_num[idx]);
		}
	}
	gtmrecv_repl_send((repl_msg_ptr_t)&strminfo_msg, REPL_STRMINFO, SIZEOF(repl_strminfo_msg_t), "REPL_STRMINFO", MAX_SEQNO);
	return;
}

/* This function is invoked on receipt of a REPL_NEED_HISTINFO message. In case of an error, it returns if either
 * repl_connection_reset OR gtmrecv_wait_for_jnl_seqno are set so the caller should check for this and return as well.
 */
STATICFNDEF void	gtmrecv_process_need_histinfo_msg(repl_needhistinfo_msg_ptr_t need_histinfo_msg, repl_histinfo *histinfo)
{
	boolean_t		maintain_rcvd_strm_histinfo, suppl_propagate_primary;
	int4			need_histinfo_num, need_histinfo_strm_num, status;
	recvpool_ctl_ptr_t	recvpool_ctl;
	seq_num			first_unprocessed_seqno, last_unprocessed_histinfo_seqno;
	seq_num			need_histinfo_seqno, last_valid_histinfo_seqno;

	assert(remote_side->endianness_known); /* ensure remote_side->cross_endian is reliable */
	if (!remote_side->cross_endian)
	{
		need_histinfo_seqno = need_histinfo_msg->seqno;
		need_histinfo_num = need_histinfo_msg->histinfo_num;
		need_histinfo_strm_num = need_histinfo_msg->strm_num;
	} else
	{
		need_histinfo_seqno = GTM_BYTESWAP_64(need_histinfo_msg->seqno);
		need_histinfo_num = GTM_BYTESWAP_32(need_histinfo_msg->histinfo_num);
		need_histinfo_strm_num = GTM_BYTESWAP_32(need_histinfo_msg->strm_num);
	}
	assert((INVALID_SUPPL_STRM == strm_index) || ((0 <= strm_index) && (MAX_SUPPL_STRMS > strm_index)));
	if (INVALID_SUPPL_STRM == strm_index)
	{	/* Both receiver and source sides are non-supplementary instances */
		if (REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver)
		{	/* needhistinfo_msg.strm_num & histinfo_num are uninitialized in this case. Fix it */
			need_histinfo_strm_num = INVALID_SUPPL_STRM;
			need_histinfo_num = INVALID_HISTINFO_NUM;
		} else
		{
			assert(INVALID_SUPPL_STRM == need_histinfo_strm_num);
			assert(INVALID_HISTINFO_NUM == need_histinfo_num);
		}
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_NEED_HISTINFO message for seqno "INT8_FMT" "INT8_FMTX"\n",
			need_histinfo_seqno, need_histinfo_seqno);
	} else if (0 < strm_index)
	{	/* Receiver side is supplementary but Source side is a non-supplementary instance */
		assert(INVALID_SUPPL_STRM == need_histinfo_strm_num);
		assert(INVALID_HISTINFO_NUM == need_histinfo_num);
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_NEED_HISTINFO message for Stream %d : Seqno "
			INT8_FMT" "INT8_FMTX"\n", strm_index, need_histinfo_seqno, need_histinfo_seqno);
		need_histinfo_strm_num = strm_index;
	} else
	{	/* Both receiver and source sides are supplementary instances */
		/* strm_index is 0 at this point (already asserted above) */
		assert(INVALID_SUPPL_STRM != need_histinfo_strm_num);
		if (INVALID_HISTINFO_NUM == need_histinfo_num)
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_NEED_HISTINFO message for Stream %d : "
				"Seqno "INT8_FMT" "INT8_FMTX"\n", need_histinfo_strm_num, need_histinfo_seqno, need_histinfo_seqno);
		else
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_NEED_HISTINFO message for History Number %d\n",
				need_histinfo_num);
	}
	/* The only two histinfo_num values that have special meaning are negative. So we can check for a valid value
	 * by checking for positive. Assert that below before doing the positive check.
	 */
	assert((0 > INVALID_HISTINFO_NUM) && (0 > UNKNOWN_HISTINFO_NUM));
	recvpool_ctl = recvpool.recvpool_ctl;
	if (0 <= need_histinfo_num)
	{	/* Handle simplest case first. Get the "need_histinfo_num"'th history record directly from the instance file */
		if (recvpool.gtmrecv_local->updateresync && (FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd))
		{	/* The history record that is being requested needs to be found in the -updateresync input instance file
			 * (not the receiver side instance file).
			 */
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
				"Searching for desired history in the -updateresync input instance file\n");
			gtmrecv_updresync_histinfo_get(need_histinfo_num, histinfo);
		} else
		{	/* The history record needs to be found in the receiver side instance file */
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
				"Searching for the desired history in the replication instance file\n");
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
			GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
				/* above macro will "return" if repl_connection_reset OR gtmrecv_wait_for_jnl_seqno is set */
			status = repl_inst_histinfo_get(need_histinfo_num, histinfo);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			if (0 != status)
			{	/* Close the connection */
				assert(ERR_REPLINSTNOHIST == status);
				assert(FALSE);
				gtmrecv_autoshutdown();	/* should not return */
			}
		}
		maintain_rcvd_strm_histinfo = TRUE;
	} else if (UNKNOWN_HISTINFO_NUM == need_histinfo_num)
	{	/* This value was sent in a previous REPL_STRMINFO message for a particular non-zero stream # because
		 * we had not yet played this history record in the instance file. Return history record directly from
		 * where the receiver server saved a copy of the last unprocessed history record for this stream number.
		 */
		assert(need_histinfo_strm_num);
		assert(remote_side->is_supplementary);
		assert((0 < need_histinfo_strm_num) && (MAX_SUPPL_STRMS > need_histinfo_strm_num));
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Searching for desired history in the receive pool\n");
		*histinfo = recvpool_ctl->last_valid_strm_histinfo[need_histinfo_strm_num];
		maintain_rcvd_strm_histinfo = FALSE;
	} else
	{
		first_unprocessed_seqno = recvpool.upd_proc_local->read_jnl_seqno;
		repl_log(gtmrecv_log_fp, TRUE, FALSE, "Update process has processed upto seqno "INT8_FMT" "INT8_FMTX"\n",
			first_unprocessed_seqno, first_unprocessed_seqno);
		suppl_propagate_primary = remote_side->is_supplementary;
		if (!suppl_propagate_primary)
			last_valid_histinfo_seqno = recvpool_ctl->last_valid_histinfo.start_seqno;
		else
			last_valid_histinfo_seqno = recvpool_ctl->last_valid_strm_histinfo[0].start_seqno;
		repl_log(gtmrecv_log_fp, TRUE, TRUE,
			"Starting seqno of the last valid history in the receive pool is "INT8_FMT" "INT8_FMTX"\n",
			last_valid_histinfo_seqno, last_valid_histinfo_seqno);
		if (last_valid_histinfo_seqno >= first_unprocessed_seqno)
			last_unprocessed_histinfo_seqno = last_valid_histinfo_seqno;
		else
			last_unprocessed_histinfo_seqno = MAX_SEQNO;
		if (last_unprocessed_histinfo_seqno && (need_histinfo_seqno > last_unprocessed_histinfo_seqno))
		{
			/* NOTE0: The source server is requesting histinfo information for a seqno whose corresponding
			 *   histinfo has also not yet been processed by the update process (and hence not present in the
			 *   instance file). Find latest histinfo information that is stored in receive pool.
			 * NOTE1: Even though there could be more than one unprocessed history record in the receive pool,
			 *   the source should request only the last one for comparison. If the last history record on the
			 *   receiver matches on the source side too, replication can resume from there.  If not, the receiver
			 *   side should do a rollback (REPL_ROLLBACK_FIRST message). That is why it is enough to maintain
			 *   recvpool_ctl->last_valid_histinfo and not pointers to all the unprocessed histories.
			 * NOTE2: There is one exception to this and that is if the receiver server had already connected
			 *   to a source server and placed records in the receiver pool and then lost the connection and
			 *   reestablished connection (with the same or a different source server) AND had been started with
			 *   the -noresync option. In this case, it will not see a REPL_ROLLBACK_FIRST message but instead
			 *   will go back in history to find the first matching history. But in this case, the -noresync is
			 *   valid only for the FIRST connection and is cleared for future connections. So reconnections would
			 *   assume as if -noresync was not specified and hence will fall into the same REPL_ROLLBACK_FIRST
			 *   category as described in NOTE0. Assert it below.
			 * NOTE3: In the case of a propagating primary supplementary instance, we need to not just store the
			 *   last received history record but also one for each stream possible.
			 */
			assert(!recvpool.gtmrecv_local->updateresync);
			assert(!recvpool.gtmrecv_local->noresync);
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Searching for the desired history in the receive pool\n");
			if (!suppl_propagate_primary)
				memcpy(histinfo, &recvpool_ctl->last_valid_histinfo, SIZEOF(repl_histinfo));
			else
			{
				assert(!need_histinfo_strm_num);
				memcpy(histinfo, &recvpool_ctl->last_valid_strm_histinfo[0], SIZEOF(repl_histinfo));
			}
			assert(!recvpool_ctl->insert_strm_histinfo);
		} else if (recvpool.gtmrecv_local->updateresync && (FD_INVALID != recvpool.gtmrecv_local->updresync_instfile_fd))
		{	/* The receiver was started with -UPDATERESYNC=<INSTFILENAME>. The history record that is being requested
			 * needs to be found in the input instance file (not the receiver side instance file).
			 * Read the history record corresponding to need_histinfo_seqno.
			 */
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
				"Searching for desired history in the -updateresync input instance file\n");
			gtmrecv_updresync_histinfo_find_seqno(need_histinfo_seqno, need_histinfo_strm_num, histinfo);
			assert(histinfo->start_seqno);
		} else
		{	/* The seqno has been processed by the update process. Hence the histinfo
			 * for this will be found in the instance file. Search there.
			 */
			assert(NULL != jnlpool.jnlpool_dummy_reg);
			repl_log(gtmrecv_log_fp, TRUE, TRUE,
				"Searching for the desired history in the replication instance file\n");
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
			GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
			status = repl_inst_wrapper_histinfo_find_seqno(need_histinfo_seqno, need_histinfo_strm_num, histinfo);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			if (0 != status)
			{	/* Close the connection */
				assert(ERR_REPLINSTNOHIST == status);
				gtmrecv_autoshutdown();	/* should not return */
			}
			assert((histinfo->histinfo_num != (jnlpool.repl_inst_filehdr->num_histinfo - 1))
				|| (histinfo->start_seqno == jnlpool_ctl->last_histinfo_seqno));
			if (0 < need_histinfo_strm_num)
			{	/* About to send to a non-supplementary instance. It does not understand strm_seqnos.
				 * So convert it back to a format it understands.
				 */
				CONVERT_SUPPL2NONSUPPL_HISTINFO(*histinfo);
			}
			assert(histinfo->start_seqno);
			assert(histinfo->start_seqno < need_histinfo_seqno);
		}
		maintain_rcvd_strm_histinfo = (0 == histinfo->strm_index);

	}
	if (maintain_rcvd_strm_histinfo && recvpool_ctl->insert_strm_histinfo)
	{
		assert(remote_side->is_supplementary);
		need_histinfo_strm_num = histinfo->strm_index;
		assert((0 <= need_histinfo_strm_num) && (MAX_SUPPL_STRMS > need_histinfo_strm_num));
		assert(IS_REPL_INST_UUID_NULL(recvpool_ctl->last_rcvd_strm_histinfo[need_histinfo_strm_num].lms_group));
		assert((FALSE == recvpool_ctl->is_valid_strm_histinfo[need_histinfo_strm_num])
			|| !memcmp(&recvpool_ctl->last_rcvd_strm_histinfo[need_histinfo_strm_num],
									histinfo, SIZEOF(repl_histinfo)));
		memcpy(&recvpool_ctl->last_rcvd_strm_histinfo[need_histinfo_strm_num], histinfo, SIZEOF(repl_histinfo));
		recvpool_ctl->is_valid_strm_histinfo[need_histinfo_strm_num] = TRUE;
		if (recvpool_ctl->max_strm_histinfo <= need_histinfo_strm_num)
			recvpool_ctl->max_strm_histinfo = need_histinfo_strm_num + 1;
	}
	return;
}

STATICFNDEF void do_main_loop(boolean_t crash_restart)
{
	/* The work-horse of the Receiver Server */
	boolean_t			dont_reply_to_heartbeat = FALSE, is_repl_cmpc;
	boolean_t			uncmpfail, send_cross_endian, preserve_buffp, recvpool_prepared;
	gtmrecv_local_ptr_t		gtmrecv_local;
	gtm_time4_t			ack_time;
	int4				msghdrlen, strm_num;
	int4				need_histinfo_num;
	int				cmpret;
	int				msg_type, msg_len;
	int				status;					/* needed for REPL_{SEND,RECV}_LOOP */
	int				torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int				tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	recvpool_ctl_ptr_t		recvpool_ctl;
	repl_cmpinfo_msg_ptr_t		cmptest_msg;
	repl_cmpinfo_msg_t		cmpsolve_msg;
	repl_cmpmsg_ptr_t		cmpmsgp;
	repl_heartbeat_msg_t		heartbeat;
	repl_histinfo			histinfo;
	repl_needhistinfo_msg_ptr_t	need_histinfo_msg;
	repl_needinst_msg_ptr_t		need_instinfo_msg;
	repl_needstrminfo_msg_ptr_t	need_strminfo_msg;
	repl_old_instinfo_msg_t		old_instinfo_msg;
	repl_old_needinst_msg_ptr_t	old_need_instinfo_msg;
	repl_start_msg_ptr_t		msgp;
	repl_start_reply_msg_t		*start_msg;
	seq_num				ack_seqno, temp_ack_seqno;
	seq_num				request_from, recvd_jnl_seqno;
	sgmnt_addrs			*repl_csa;
	uchar_ptr_t			old_buffp;
	uint4				recvd_start_flags, len;
	uLong				cmplen;
	uLongf				destlen;
	unsigned char			*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	unsigned char			remote_jnl_ver;
	upd_proc_local_ptr_t		upd_proc_local;
	repl_logfile_info_msg_t		*logfile_msgp, logfile_msg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;
	gtmrecv_wait_for_jnl_seqno = FALSE;

	assert((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open);
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	DEBUG_ONLY(
		assert(!repl_csa->hold_onto_crit);
		ASSERT_VALID_JNLPOOL(repl_csa);
	)
	/* If BAD_TRANS was written by the update process, it would have updated recvpool_ctl->jnl_seqno accordingly.
	 * Only otherwise, do we need to wait for it to write "recvpool_ctl->jnl_seqno".
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
		/* Since remote primary is multisite capable (otherwise we would have issued an error), we need to send the
		 * journal seqno of this instance for comparison. If the receiver has received more seqnos than have been
		 * processed by the update process, we should be sending the last received seqno across to avoid receiving
		 * duplicate and out-of-order seqnos. This is maintained in "recvpool_ctl->jnl_seqno" and is guaranteed to
		 * be greater than or equal to the journal seqno of this instance.
		 */
		request_from = recvpool_ctl->jnl_seqno;
		/* If this is the first time the update process initialized "recvpool_ctl->jnl_seqno", it should be
		 * equal to "jnlpool_ctl->jnl_seqno". But if the receiver had already connected and received a bunch
		 * of seqnos and if the update process did not process all of them and if the receiver disconnects
		 * and re-establishes the connection, the value of "recvpool_ctl->jnl_seqno" could be greater than
		 * "jnlpool_ctl->jnl_seqno" if there is non-zero backlog on the secondary. Assert accordingly.
		 * There is one exception to this and that is if this is a root primary supplementary instance.
		 * In that case, the receive pool talks about the non-supplementary instance stream jnl_seqnos whereas
		 * the jnlpool talks about the merged stream of jnl_seqnos (including any local updates). Therefore we
		 * cannot compare the two at all.
		 */
		assert((!jnlpool_ctl->upd_disabled && jnlpool.repl_inst_filehdr->is_supplementary)
			|| (recvpool_ctl->jnl_seqno >= jnlpool_ctl->jnl_seqno));
		assert(request_from);
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Requesting transactions from JNL_SEQNO "INT8_FMT" "INT8_FMTX"\n",
			request_from, request_from);
		/* Send (re)start JNL_SEQNO to Source Server.
		 * Note that even though we might know the endianness of the source side at this time, we still send this
		 * message in the native endian format. This keeps the logic on the source server side simple. This is the only
		 * exception to the general rule that the receiver server does all endian conversion for messages except the
		 * first one in the connection handshake. The source side knows to endian convert this (instead of expecting it
		 * to be in the source endian format) if this EPL_START_JNL_SEQNO message is not the first one in the connection.
		 * The only exception is if the source side is pre-V55000 AND we have already determined the endianness of the
		 * source side (i.e. the REPL_START_JNL_SEQNO message about to be sent is not the first one for this connection)
		 * AND the source is cross-endian. In that case, the pre-V55000 source does not know to handle a
		 * receiver-side-native-endian format message. So endian convert the message only in this case.
		 */
		repl_log(gtmrecv_log_fp, TRUE, TRUE, "Sending REPL_START_JNL_SEQNO message with seqno "INT8_FMT" "INT8_FMTX"\n",
			request_from, request_from);
		send_cross_endian = (remote_side->endianness_known && remote_side->cross_endian
						&& (REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver));
		msgp = (repl_start_msg_ptr_t)gtmrecv_msgp;
		memset(msgp, 0, SIZEOF(*msgp));
		/* Since REPL_START_JNL_SEQNO is 0, there is no endian conversion necessary but for completeness we do it */
		msgp->type = send_cross_endian ? GTM_BYTESWAP_32(REPL_START_JNL_SEQNO) : REPL_START_JNL_SEQNO;
		if (send_cross_endian)
			*((seq_num *)msgp->start_seqno) = GTM_BYTESWAP_64(request_from);
		else
			*((seq_num *)msgp->start_seqno) = request_from;
		msgp->start_flags = START_FLAG_NONE;
		msgp->start_flags |= (gtmrecv_options.stopsourcefilter ? START_FLAG_STOPSRCFILTER : 0);
		/* If -UPDATERESYNC is specified then let pre-V55000 source server know so it does not ask for history record
		 * exchange. In case of post-V55000 source server, we expect a value to the -updateresync qualifier (the instance
		 * file name) and that is used for the history record exchange. In that case the source server ignores the
		 * START_FLAG_UPDATERESYNC bit and requests a history exchange anyways.
		 */
		msgp->start_flags |= (gtmrecv_local->updateresync ? START_FLAG_UPDATERESYNC : 0);
		/* Let source server know if -NORESYNC was specified in receiver server startup */
		msgp->start_flags |= (gtmrecv_local->noresync ? START_FLAG_NORESYNC : 0);
		msgp->start_flags |= START_FLAG_HASINFO;
		if (this_side->is_std_null_coll)
			msgp->start_flags |= START_FLAG_COLL_M;
		msgp->start_flags |= START_FLAG_VERSION_INFO;
		GTMTRIG_ONLY(msgp->start_flags |= START_FLAG_TRIGGER_SUPPORT;)
		if (send_cross_endian)
			msgp->start_flags = GTM_BYTESWAP_32(msgp->start_flags);
		msgp->jnl_ver = this_side->jnl_ver;
		msgp->proto_ver = REPL_PROTO_VER_THIS;
		msgp->node_endianness = NODE_ENDIANNESS;
		msgp->is_supplementary = jnlpool.repl_inst_filehdr->is_supplementary;
		msgp->len = send_cross_endian ? GTM_BYTESWAP_32(MIN_REPL_MSGLEN) : MIN_REPL_MSGLEN;
		msg_len = MIN_REPL_MSGLEN;
		REPL_SEND_LOOP(gtmrecv_sock_fd, msgp, msg_len, REPL_POLL_NOWAIT)
		{
			GTMRECV_POLL_ACTIONS(0, 0, NULL);
		}
		CHECK_REPL_SEND_LOOP_ERROR(status, "REPL_START_JNL_SEQNO");
	}
	gtmrecv_bad_trans_sent = FALSE;
	request_from = recvpool_ctl->jnl_seqno;
	assert(request_from >= seq_num_one);

	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Waiting for REPL_WILL_RESTART_WITH_INFO or REPL_ROLLBACK_FIRST message\n");
	/* Receive journal data and place it in the Receive Pool */
	buff_start = (unsigned char *)gtmrecv_msgp;
	buffp = buff_start;
	buff_unprocessed = 0;
	data_len = 0;
	write_loc = recvpool_ctl->write;
	write_wrap = recvpool_ctl->write_wrap;

	repl_recv_data_recvd = 0;
	repl_recv_data_processed = 0;
	repl_recv_postfltr_data_procd = 0;
	repl_recv_lastlog_data_recvd = 0;
	repl_recv_lastlog_data_procd = 0;
	msghdrlen = REPL_MSG_HDRLEN;
	while (TRUE)
	{
		recvd_len = gtmrecv_max_repl_msglen - buff_unprocessed;
		while ((SS_NORMAL == (status = repl_recv(gtmrecv_sock_fd,
							(buffp + buff_unprocessed), &recvd_len, REPL_POLL_WAIT)))
			       && (0 == recvd_len))
		{
			recvd_len = gtmrecv_max_repl_msglen - buff_unprocessed;
			if (xoff_sent)
			{
				DO_FLOW_CONTROL(write_loc);
			}
			if (xoff_sent && GTMRECV_XOFF_LOG_CNT <= xoff_msg_log_cnt)
			{	/* update process is still running slow, Force wait before logging any message. */
				SHORT_SLEEP(REPL_POLL_WAIT >> 10); /* approximate in ms */
				REPL_DPRINT1("Waiting for Update Process to clear recvpool space\n");
				xoff_msg_log_cnt = 0;
			} else if (xoff_sent)
				xoff_msg_log_cnt++;
			GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
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
					ISSUE_REPLCOMM_ERROR("Error in receiving from source. Error in recv", status);
				}
			} else if (EREPL_SELECT == repl_errno)
			{
					ISSUE_REPLCOMM_ERROR("Error in receiving from source. Error in select", status);
			}
		}
		if (repl_connection_reset)
			return;
#		ifdef REPL_CMP_SOLVE_TESTING
		/* Received communication from the source server, so we can cancel the timer */
		if (TREF(gtm_environment_init) && repl_cmp_solve_timer_set)
		{
			cancel_timer((TID)repl_cmp_solve_rcv_timeout);
			repl_cmp_solve_timer_set = FALSE;
		}
#		endif
		/* Something on the replication pipe - read it */
		REPL_DPRINT3("Pending data len : %d  Prev buff unprocessed : %d\n", data_len, buff_unprocessed);
		buff_unprocessed += recvd_len;
		repl_recv_data_recvd += (qw_num)recvd_len;
		if (gtmrecv_logstats)
			repl_log(gtmrecv_log_fp, FALSE, FALSE, "Recvd : %d  Total : %d\n", recvd_len, repl_recv_data_recvd);
		while (msghdrlen <= buff_unprocessed)
		{
			if (0 == data_len)
			{
				assert(0 == ((unsigned long)buffp % REPL_MSG_ALIGN));
				DEBUG_ONLY(recvpool_prepared = FALSE);
				if (!remote_side->endianness_known)
				{
					remote_side->endianness_known = TRUE;
			                msg_type = ((repl_msg_ptr_t)buffp)->type;
					assert((REPL_MSGTYPE_LAST > msg_type) || (REPL_MSGTYPE_LAST > GTM_BYTESWAP_32(msg_type)));
					if ((REPL_MSGTYPE_LAST < msg_type) && (REPL_MSGTYPE_LAST > GTM_BYTESWAP_32(msg_type)))
					{
						remote_side->cross_endian = TRUE;
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Source and Receiver sides have opposite "
							"endianness\n");
					} else
					{
						remote_side->cross_endian = FALSE;
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Source and Receiver sides have same "
							"endianness\n");
					}
				}
				if (remote_side->cross_endian)
				{
					((repl_msg_ptr_t)buffp)->type = GTM_BYTESWAP_32(((repl_msg_ptr_t)buffp)->type);
					((repl_msg_ptr_t)buffp)->len = GTM_BYTESWAP_32(((repl_msg_ptr_t)buffp)->len);
				}
				msg_type = (((repl_msg_ptr_t)buffp)->type & REPL_TR_CMP_MSG_TYPE_MASK);
				if (REPL_TR_CMP_JNL_RECS == msg_type)
				{
					msg_len = ((repl_msg_ptr_t)buffp)->len - REPL_MSG_HDRLEN;
					gtmrecv_repl_cmpmsglen = msg_len;
					gtmrecv_repl_uncmpmsglen = (((repl_msg_ptr_t)buffp)->type >> REPL_TR_CMP_MSG_TYPE_BITS);
					assert(0 < gtmrecv_repl_uncmpmsglen);
					assert(REPL_TR_CMP_THRESHOLD > gtmrecv_repl_uncmpmsglen);
					/* Since msg_len is compressed length, it need not be 8-byte aligned. Make it so
					 * since 8-byte aligned length would have been sent by the source server anyways.
					 */
					msg_len = ROUND_UP(msg_len, REPL_MSG_ALIGN);
					buffp += REPL_MSG_HDRLEN;
					exp_data_len = gtmrecv_repl_uncmpmsglen;
					buff_unprocessed -= REPL_MSG_HDRLEN;
					GTMRECV_SET_BUFF_TARGET_CMPBUFF(gtmrecv_repl_cmpmsglen, gtmrecv_repl_uncmpmsglen,
						gtmrecv_cur_cmpmsglen);
				} else if (REPL_TR_CMP_JNL_RECS2 == msg_type)
				{	/* A REPL_TR_CMP_JNL_RECS2 message is special in that it has a bigger message header.
					 * So check if unprocessed length is greater than the header. If not need to read more.
					 */
					msghdrlen = REPL_MSG_HDRLEN2;
					if (msghdrlen > buff_unprocessed)	/* Did not receive the full-header.            */
						break;				/* Break out of here and read more data first. */
					msghdrlen = REPL_MSG_HDRLEN; /* reset to regular msg hdr length for future messages */
					cmpmsgp = (repl_cmpmsg_ptr_t)buffp;
					if (remote_side->cross_endian)
					{
						cmpmsgp->cmplen = GTM_BYTESWAP_32(cmpmsgp->cmplen);
						cmpmsgp->uncmplen = GTM_BYTESWAP_32(cmpmsgp->uncmplen);
					}
					gtmrecv_repl_cmpmsglen = cmpmsgp->cmplen;
					gtmrecv_repl_uncmpmsglen = cmpmsgp->uncmplen;
					assert(0 < gtmrecv_repl_uncmpmsglen);
					assert(REPL_TR_CMP_THRESHOLD <= gtmrecv_repl_uncmpmsglen);
					msg_len = ((repl_msg_ptr_t)buffp)->len - REPL_MSG_HDRLEN2;
					/* Unlike REPL_TR_CMP_JNL_RECS message, msg_len is guaranteed to be 8-byte aligned here */
					buffp += REPL_MSG_HDRLEN2;
					exp_data_len = gtmrecv_repl_uncmpmsglen;
					buff_unprocessed -= REPL_MSG_HDRLEN2;
					GTMRECV_SET_BUFF_TARGET_CMPBUFF(gtmrecv_repl_cmpmsglen, gtmrecv_repl_uncmpmsglen,
						gtmrecv_cur_cmpmsglen);
				} else
				{
					msg_len = ((repl_msg_ptr_t)buffp)->len - REPL_MSG_HDRLEN;
					exp_data_len = msg_len;
					if (REPL_TR_JNL_RECS == msg_type || REPL_OLD_TRIPLE == msg_type || REPL_HISTREC == msg_type)
					{
						/* Target buffer is the receive pool. Prepare the receive pool for write (also
						 * checks if the transaction will fit in).
						 * Note: this is a special case where PREPARE_RECVPOOL_FOR_WRITE is not invoked
						 * right before COPY_TO_RECVPOOL because we want to prepare the receive pool just
						 * once even though the actual data (coming from the other end) might come in
						 * different pieces.
						 */
						PREPARE_RECVPOOL_FOR_WRITE(exp_data_len, 0);
						DEBUG_ONLY(recvpool_prepared = TRUE);
					} /* for REPL_TR_CMP_JNL_RECS{2}, receive pool is prepared after uncompression */
					buffp += REPL_MSG_HDRLEN;
					buff_unprocessed -= REPL_MSG_HDRLEN;
				}
				assert(0 <= buff_unprocessed);
				assert(0 == (msg_len % REPL_MSG_ALIGN));
				data_len = msg_len;
				assert(0 == (exp_data_len % REPL_MSG_ALIGN));
			}
			assert(0 == (data_len % REPL_MSG_ALIGN));
			buffered_data_len = ((data_len <= buff_unprocessed) ? data_len : buff_unprocessed);
			buffered_data_len = ROUND_DOWN2(buffered_data_len, REPL_MSG_ALIGN);
			old_buffp = buffp;
			buffp += buffered_data_len;
			buff_unprocessed -= buffered_data_len;
			data_len -= buffered_data_len;
			preserve_buffp = (0 != data_len);
			switch(msg_type)
			{
				case REPL_TR_JNL_RECS:
				case REPL_TR_CMP_JNL_RECS:
				case REPL_TR_CMP_JNL_RECS2:
				case REPL_OLD_TRIPLE:
				case REPL_HISTREC:
					is_repl_cmpc = ((REPL_TR_CMP_JNL_RECS == msg_type) || (REPL_TR_CMP_JNL_RECS2 == msg_type));
					if (!is_repl_cmpc)
					{
						assert(recvpool_prepared);
						COPY_TO_RECVPOOL(old_buffp, buffered_data_len);	/* uses and updates "write_loc" */
					} else
					{
						memcpy(gtmrecv_cmpmsgp + gtmrecv_cur_cmpmsglen, old_buffp, buffered_data_len);
						gtmrecv_cur_cmpmsglen += buffered_data_len;
						assert(gtmrecv_cur_cmpmsglen <= gtmrecv_max_repl_cmpmsglen);
					}
					repl_recv_data_processed += (qw_num)buffered_data_len;
					if (0 == data_len)
					{
						process_tr_buff(msg_type);
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
					}
					preserve_buffp = FALSE;
					break;

				case REPL_LOSTTNCOMPLETE:
					if (0 == data_len)
					{
						assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_LOSTTNCOMPLETE message\n");
						status = repl_inst_reset_zqgblmod_seqno_and_tn();
						assert(-1 == status || EXIT_ERR == status || SS_NORMAL == status);
						if (-1 == status)
						{	/* only reason we know currently for the above function to return -1 is due
							 * to a concurrent online rollback. In this case, we cannot continue
							 * and need to start afresh.
							 */
							gtmrecv_onln_rlbk_clnup();
							if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
								return;
						}
					}
					break;

				case REPL_HEARTBEAT:
					if (0 == data_len)
					{	/* Heartbeat msg contents start from buffp - msg_len */
						GTM_WHITE_BOX_TEST(WBTEST_REPL_HEARTBEAT_NO_ACK, dont_reply_to_heartbeat, TRUE);
						if (dont_reply_to_heartbeat)
						{
							dont_reply_to_heartbeat = FALSE;
							break;
						}
						memcpy(heartbeat.ack_seqno, buffp - msg_len, msg_len);
						assert(remote_side->endianness_known); /* only then is remote_side->cross_endian
											* reliable */
						if (!remote_side->cross_endian)
						{
							 ack_time = *(gtm_time4_t *)&heartbeat.ack_time[0];
							 memcpy((uchar_ptr_t)&ack_seqno,
								(uchar_ptr_t)&heartbeat.ack_seqno[0], SIZEOF(seq_num));
						} else
						{
							 ack_time = GTM_BYTESWAP_32(*(gtm_time4_t *)&heartbeat.ack_time[0]);
							 memcpy((uchar_ptr_t)&temp_ack_seqno,
								(uchar_ptr_t)&heartbeat.ack_seqno[0], SIZEOF(seq_num));
							 ack_seqno = GTM_BYTESWAP_64(temp_ack_seqno);
						}
						REPL_DPRINT4("HEARTBEAT received with time %ld SEQNO "INT8_FMT" at %ld\n",
							     ack_time, ack_seqno, time(NULL));
						ack_seqno = upd_proc_local->read_jnl_seqno;
						if (!remote_side->cross_endian)
						{
							heartbeat.type = REPL_HEARTBEAT;
							heartbeat.len = MIN_REPL_MSGLEN;
							memcpy((uchar_ptr_t)&heartbeat.ack_seqno[0],
								(uchar_ptr_t)&ack_seqno, SIZEOF(seq_num));

						} else
						{
							heartbeat.type = GTM_BYTESWAP_32(REPL_HEARTBEAT);
							heartbeat.len = GTM_BYTESWAP_32(MIN_REPL_MSGLEN);
							temp_ack_seqno = GTM_BYTESWAP_64(ack_seqno);
							memcpy((uchar_ptr_t)&heartbeat.ack_seqno[0],
								(uchar_ptr_t)&temp_ack_seqno, SIZEOF(seq_num));
						}
						REPL_SEND_LOOP(gtmrecv_sock_fd, &heartbeat, MIN_REPL_MSGLEN, REPL_POLL_NOWAIT)
						{
							GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
						}
						CHECK_REPL_SEND_LOOP_ERROR(status, "REPL_HEARTBEAT");
						REPL_DPRINT4("HEARTBEAT sent with time %ld SEQNO "INT8_FMT" at %ld\n",
							     ack_time, ack_seqno, time(NULL));
					}
					break;

				case REPL_OLD_NEED_INSTANCE_INFO:
					if (0 == data_len)
					{
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						old_need_instinfo_msg = (repl_old_needinst_msg_ptr_t)(buffp -
												msg_len - REPL_MSG_HDRLEN);
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_OLD_NEED_INSTANCE_INFO message"
							" from primary instance [%s]\n", old_need_instinfo_msg->instname);
						if (jnlpool.repl_inst_filehdr->is_supplementary)
						{	/* Issue REPL2OLD error because this is a supplementary instance and remote
							 * side runs a GT.M version that does not know the supplementary protocol */
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPL2OLD, 4,
								LEN_AND_STR(old_need_instinfo_msg->instname),
								LEN_AND_STR(jnlpool.repl_inst_filehdr->inst_info.this_instname));
						}
						/* The source server does not understand the supplementary protocol.
						 * So make sure -UPDATERESYNC if specified at receiver server startup
						 * had no value. If not, issue error.
						 */
						if (gtmrecv_local->updateresync
							&& (FD_INVALID != gtmrecv_local->updresync_instfile_fd))
						{
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UPDSYNCINSTFILE, 0,
								ERR_TEXT, 2,
								LEN_AND_LIT("Source side is non-supplementary implies "
								  	"-UPDATERESYNC needs no value specified"));
						}
						/* Initialize the remote side protocol version from "proto_ver"
						 * field of this msg
						 */
						remote_side->proto_ver = old_need_instinfo_msg->proto_ver;
						assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
						assert(REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver);
						/*************** Send REPL_OLD_INSTANCE_INFO message ***************/
						memset(&old_instinfo_msg, 0, SIZEOF(old_instinfo_msg));
						memcpy(old_instinfo_msg.instname,
							jnlpool.repl_inst_filehdr->inst_info.this_instname, MAX_INSTNAME_LEN - 1);
						grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
						GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
						old_instinfo_msg.was_rootprimary = (unsigned char)repl_inst_was_rootprimary();
						rel_lock(jnlpool.jnlpool_dummy_reg);
						gtmrecv_repl_send((repl_msg_ptr_t)&old_instinfo_msg, REPL_OLD_INSTANCE_INFO,
								MIN_REPL_MSGLEN, "REPL_OLD_INSTANCE_INFO", MAX_SEQNO);
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
						/* Do not allow an instance which was formerly a root primary or which still
						 * has a non-zero value of "zqgblmod_seqno" to start up as a tertiary.
						 */
						assert(jnlpool.jnlpool_ctl == jnlpool_ctl);
						if ((old_instinfo_msg.was_rootprimary || jnlpool_ctl->max_zqgblmod_seqno)
								&& !old_need_instinfo_msg->is_rootprimary)
						{
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PRIMARYNOTROOT, 2,
								LEN_AND_STR((char *) old_need_instinfo_msg->instname));
							gtmrecv_autoshutdown();	/* should not return */
							assert(FALSE);
						}
						memcpy(jnlpool_ctl->primary_instname, old_need_instinfo_msg->instname,
							MAX_INSTNAME_LEN - 1);
					}
					break;

				case REPL_NEED_INSTINFO:
					if (0 == data_len)
					{
						assert(msg_len == SIZEOF(repl_needinst_msg_t) - REPL_MSG_HDRLEN);
						need_instinfo_msg = (repl_needinst_msg_ptr_t)(buffp - msg_len - REPL_MSG_HDRLEN);
						gtmrecv_check_and_send_instinfo(need_instinfo_msg, IS_RCVR_SRVR_TRUE);
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
					}
					break;

				case REPL_CMP_TEST:
					if (0 == data_len)
					{
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_CMP_TEST message\n");
						uncmpfail = FALSE;
						if (ZLIB_CMPLVL_NONE == gtm_zlib_cmp_level)
						{	/* Receiver does not have compression enabled in the first place.
							 * Send dummy REPL_CMP_SOLVE response message.
							 */
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Environment variable "
								"gtm_zlib_cmp_level specifies NO decompression (set to %d)\n",
								gtm_zlib_cmp_level);
							uncmpfail = TRUE;
						}
						assert(remote_side->endianness_known); /* ensure remote_side->cross_endian
											* is reliable */
						if (!uncmpfail)
						{
							assert(msg_len == REPL_MSG_CMPINFOLEN - REPL_MSG_HDRLEN);
							cmptest_msg = (repl_cmpinfo_msg_ptr_t)(buffp - msg_len - REPL_MSG_HDRLEN);
							if (!remote_side->cross_endian)
								cmplen = cmptest_msg->datalen;
							else
								cmplen = GTM_BYTESWAP_32(cmptest_msg->datalen);
							if (REPL_MSG_CMPEXPDATALEN < cmplen)
							{
								assert(FALSE);	/* since src srvr should not have sent such a msg */
								repl_log(gtmrecv_log_fp, TRUE, TRUE, "Compression test message "
									"has compressed data length (%d) greater than receiver "
									"allocated length (%d)\n", (int)cmplen,
									REPL_MSG_CMPEXPDATALEN);
								uncmpfail = TRUE;
							}
						}
						if (!uncmpfail)
						{
							destlen = REPL_MSG_CMPEXPDATALEN; /* initialize available
												     * decompressed buffer space */
							ZLIB_UNCOMPRESS(&cmpsolve_msg.data[0], destlen, &cmptest_msg->data[0],
										cmplen, cmpret);
							GTM_WHITE_BOX_TEST(WBTEST_REPL_TEST_UNCMP_ERROR, cmpret, Z_DATA_ERROR);
							switch(cmpret)
							{
								case Z_MEM_ERROR:
									assert(FALSE);
									repl_log(gtmrecv_log_fp, TRUE, TRUE,
										GTM_ZLIB_Z_MEM_ERROR_STR
										GTM_ZLIB_UNCMP_ERR_SOLVE_STR);
									break;
								case Z_BUF_ERROR:
									assert(FALSE);
									repl_log(gtmrecv_log_fp, TRUE, TRUE,
										GTM_ZLIB_Z_BUF_ERROR_STR
										GTM_ZLIB_UNCMP_ERR_SOLVE_STR);
									break;
								case Z_DATA_ERROR:
									assert(gtm_white_box_test_case_enabled
										&& (WBTEST_REPL_TEST_UNCMP_ERROR
											== gtm_white_box_test_case_number));
									repl_log(gtmrecv_log_fp, TRUE, TRUE,
										GTM_ZLIB_Z_DATA_ERROR_STR
										GTM_ZLIB_UNCMP_ERR_SOLVE_STR);
									break;
							}
							if (Z_OK != cmpret)
								uncmpfail = TRUE;
						}
						if (!uncmpfail)
						{
							cmpsolve_msg.datalen = (int4)destlen;
							GTM_WHITE_BOX_TEST(WBTEST_REPL_TEST_UNCMP_ERROR, cmpsolve_msg.datalen,
								REPL_MSG_CMPDATALEN - 1);
							if (REPL_MSG_CMPDATALEN != cmpsolve_msg.datalen)
							{	/* decompression did not yield precompressed data length */
								assert(gtm_white_box_test_case_enabled
									&& (WBTEST_REPL_TEST_UNCMP_ERROR
										== gtm_white_box_test_case_number));
								repl_log(gtmrecv_log_fp, TRUE, TRUE, GTM_ZLIB_UNCMPLEN_ERROR_STR
									"\n", cmpsolve_msg.datalen, REPL_MSG_CMPDATALEN);
								uncmpfail = TRUE;
							}
						}
						if (uncmpfail)
						{
							cmpsolve_msg.datalen = REPL_RCVR_CMP_TEST_FAIL;
							repl_log(gtmrecv_log_fp, TRUE, TRUE, GTM_ZLIB_UNCMPTRANSITION_STR);
						}
						if (remote_side->cross_endian)
							cmpsolve_msg.datalen = GTM_BYTESWAP_32(cmpsolve_msg.datalen);
						cmpsolve_msg.proto_ver = REPL_PROTO_VER_THIS;
#						ifdef REPL_CMP_SOLVE_TESTING
						if (TREF(gtm_environment_init))
						{
							start_timer((TID)repl_cmp_solve_rcv_timeout, 15 * 60 * 1000,
									repl_cmp_solve_rcv_timeout, 0, NULL);
							repl_cmp_solve_timer_set = TRUE;
						}
#						endif
						gtmrecv_repl_send((repl_msg_ptr_t)&cmpsolve_msg, REPL_CMP_SOLVE,
									REPL_MSG_CMPINFOLEN, "REPL_CMP_SOLVE", MAX_SEQNO);
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
						if (!uncmpfail)
							repl_zlib_cmp_level = gtm_zlib_cmp_level;
					}
					break;

				case REPL_NEED_STRMINFO:
					if (0 == data_len)
					{
						assert(REPL_PROTO_VER_SUPPLEMENTARY <= remote_side->proto_ver);
						assert(remote_side->is_supplementary);
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						need_strminfo_msg = (repl_needstrminfo_msg_ptr_t)(buffp - msg_len
													- REPL_MSG_HDRLEN);
						gtmrecv_process_need_strminfo_msg(need_strminfo_msg);
						/* Check for error return from above function call */
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
					}
					break;

				case REPL_NEED_HISTINFO:
				/* case REPL_NEED_TRIPLE_INFO: too but that message has been renamed to REPL_NEED_HISTINFO */
					if (0 == data_len)
					{
						assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						need_histinfo_msg = (repl_needhistinfo_msg_ptr_t)(buffp - msg_len
													- REPL_MSG_HDRLEN);
						gtmrecv_process_need_histinfo_msg(need_histinfo_msg, &histinfo);
						/* Check for error return from above function call */
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
						/* Send the histinfo */
						gtmrecv_send_histinfo(&histinfo);
						/* Check for error return from above function call as well */
						if (repl_connection_reset || gtmrecv_wait_for_jnl_seqno)
							return;
					}
					break;

				case REPL_WILL_RESTART_WITH_INFO:
				case REPL_ROLLBACK_FIRST:
					if (0 != data_len)
						break;
					/* Have received a REPL_WILL_RESTART_WITH_INFO or REPL_ROLLBACK_FIRST message. If
					 * have not yet received a REPL_OLD_NEED_INSTANCE_INFO or REPL_NEED_INSTINFO message
					 * (which would have initialized "remote_side->proto_ver"), it
					 * means the remote side does not understand multi-site replication communication
					 * protocol. Note that down.
					 */
					if (REPL_PROTO_VER_UNINITIALIZED == remote_side->proto_ver)
					{	/*  Issue REPL2OLD error because primary is dual-site */
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPL2OLD, 4,
								LEN_AND_STR(UNKNOWN_INSTNAME),
								LEN_AND_STR(jnlpool.repl_inst_filehdr->inst_info.this_instname));
					}
					/* Assert that endianness_known and cross_endian have already been initialized.
					 * This ensures that remote_side->cross_endian is reliable */
					assert(remote_side->endianness_known);
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
						grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
						GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
						jnlpool.repl_inst_filehdr->was_rootprimary = FALSE;
						repl_inst_flush_filehdr();
						rel_lock(jnlpool.jnlpool_dummy_reg);
					}
					assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
					assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
					start_msg = (repl_start_reply_msg_ptr_t)(buffp - msg_len - REPL_MSG_HDRLEN);
					assert((unsigned long)start_msg % SIZEOF(seq_num) == 0); /* alignment check */
					memcpy((uchar_ptr_t)&recvd_jnl_seqno,
						(uchar_ptr_t)start_msg->start_seqno, SIZEOF(seq_num));
					/* Assert that "node_endianness" field reflects our cross-endian understanding */
					BIGENDIAN_ONLY(assert((REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver)
						|| (remote_side->cross_endian
							&& (LITTLE_ENDIAN_MARKER == start_msg->node_endianness))
						|| (!remote_side->cross_endian
							&& (BIG_ENDIAN_MARKER == start_msg->node_endianness))));
					LITTLEENDIAN_ONLY(assert((REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver)
						|| (remote_side->cross_endian
							&& (BIG_ENDIAN_MARKER == start_msg->node_endianness))
						|| (!remote_side->cross_endian
							&& (LITTLE_ENDIAN_MARKER == start_msg->node_endianness))));
					if (remote_side->cross_endian)
						recvd_jnl_seqno = GTM_BYTESWAP_64(recvd_jnl_seqno);
					/* Handle REPL_ROLLBACK_FIRST case (easy one) first */
					if (REPL_ROLLBACK_FIRST == msg_type)
					{
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_ROLLBACK_FIRST message. "
							"Secondary is out of sync with the primary. "
							"Secondary at "INT8_FMT" "INT8_FMTX", Primary at "INT8_FMT" "INT8_FMTX". "
							"Do ROLLBACK FIRST\n",
							request_from, request_from, recvd_jnl_seqno, recvd_jnl_seqno);
						if (gtmrecv_options.autorollback)
						{
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Receiver was started with "
								"-AUTOROLLBACK. Initiating automatic ONLINE FETCHRESYNC "
								"ROLLBACK\n");
							repl_connection_reset = TRUE;
							repl_close(&gtmrecv_sock_fd);
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Closing connection before starting "
								"ROLLBACK\n");
							if (SS_NORMAL != gtmrecv_start_onln_rlbk())
							{	/* gtmrecv_start_onln_rlbk() would have issued the appropriate
								 * error message.
								 */
								assert(FALSE);
								gtmrecv_autoshutdown(); /* should not return */
								assert(FALSE);
							}
							grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
							GTMRECV_ONLN_RLBK_CLNUP_IF_NEEDED;
							/* The ONLINE ROLLBACK did not change the physical or the logical state as
							 * otherwise the above macro would have returned to the caller. But, since
							 * we have already disconnected the connection by now, we cannot resume the
							 * flow from this point on. So, go ahead and release the lock and shutdown
							 * the Receiver Server.
							 */
							rel_lock(jnlpool.jnlpool_dummy_reg);
						} else
						{
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Receiver was not started with "
									"-AUTOROLLBACK. Manual ROLLBACK required. Shutting down\n");
						}
						gtmrecv_autoshutdown();	/* should not return */
						assert(FALSE);
						break;
					}
					/* Handle REPL_WILL_RESTART_WITH_INFO case now */
					assert(REPL_WILL_RESTART_WITH_INFO == msg_type);
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received REPL_WILL_RESTART_WITH_INFO message"
							" with seqno "INT8_FMT" "INT8_FMTX"\n", recvd_jnl_seqno, recvd_jnl_seqno);
					remote_side->jnl_ver = start_msg->jnl_ver;
					remote_jnl_ver = remote_side->jnl_ver;
					REPL_DPRINT3("Local jnl ver is octal %o, remote jnl ver is octal %o\n",
						     this_side->jnl_ver, remote_jnl_ver);
					repl_check_jnlver_compat(!remote_side->cross_endian);
					/* older versions zero filler that was in place of start_msg->start_flags,
					 * so we are okay fetching start_msg->start_flags unconditionally.
					 */
					GET_ULONG(recvd_start_flags, start_msg->start_flags);
					if (remote_side->cross_endian)
						recvd_start_flags = GTM_BYTESWAP_32(recvd_start_flags);
					assert(remote_jnl_ver > V15_JNL_VER || 0 == recvd_start_flags);
					if (remote_jnl_ver <= V15_JNL_VER) /* safety in pro */
						recvd_start_flags = 0;
					remote_side->is_std_null_coll = (recvd_start_flags & START_FLAG_COLL_M) ?  TRUE : FALSE;
					if (remote_side->is_std_null_coll != this_side->is_std_null_coll)
						remote_side->null_subs_xform = (remote_side->is_std_null_coll
							?  STDNULL_TO_GTMNULL_COLL : GTMNULL_TO_STDNULL_COLL);
					else
						remote_side->null_subs_xform = FALSE;
						/* this sets null_subs_xform regardless of remote_jnl_ver */
					remote_side->is_supplementary = start_msg->is_supplementary;
					remote_side->trigger_supported = (recvd_start_flags & START_FLAG_TRIGGER_SUPPORT)
														? TRUE : FALSE;
					if (this_side->jnl_ver > remote_jnl_ver)
					{
						assert(JNL_VER_EARLIEST_REPL <= remote_jnl_ver);
						assert((remote_jnl_ver - JNL_VER_EARLIEST_REPL) < ARRAYSIZE(repl_filter_old2cur));
						assert((remote_jnl_ver - JNL_VER_EARLIEST_REPL) < ARRAYSIZE(repl_filter_cur2old));
						assert(IF_NONE != repl_filter_old2cur[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
						assert(IF_INVALID != repl_filter_old2cur[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
						/* reverse transformation should exist */
						assert(IF_INVALID != repl_filter_cur2old[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
						assert(IF_NONE != repl_filter_cur2old[remote_jnl_ver - JNL_VER_EARLIEST_REPL]);
						gtmrecv_filter |= INTERNAL_FILTER;
						/* Any time the ^#t global format version is bumped, the below
						 * assert will trip. This way, anyone who bumps the trigger label
						 * ensures that the internal filter routines in repl_filter.c are
						 * accordingly changed to upgrade triggers before applying them
						 * on the current database which supports the latest ^#t format.
						 */
						assert(0 == MEMCMP_LIT(HASHT_GBL_CURLABEL, "2"));
						gtmrecv_alloc_filter_buff(gtmrecv_max_repl_msglen);
					} else
					{
						gtmrecv_filter &= ~INTERNAL_FILTER;
						if (NO_FILTER == gtmrecv_filter)
							gtmrecv_free_filter_buff();
					}
					/* Don't send any more stopsourcefilter message */
					gtmrecv_options.stopsourcefilter = FALSE;
					assert(jnlpool.jnlpool_ctl == jnlpool_ctl);
					assert(QWEQ(recvd_jnl_seqno, request_from)
							|| (jnlpool.repl_inst_filehdr->is_supplementary
								&& !jnlpool_ctl->upd_disabled && strm_index));
					assert(this_side->is_supplementary == jnlpool.repl_inst_filehdr->is_supplementary);
					if (this_side->is_supplementary && !remote_side->is_supplementary)
					{
						/* For the non-supplementary -> supplementary replication connection that happens
						 * for the very first time during the lifetime of this receiver server,
						 * (i.e. when upd_proc_local->read_jnl_seqno is 0), until this point in time,
						 * we cannot be sure what the starting point of the transmission is (partly due
						 * to -UPDATERESYNC but primarily due to -NORESYNC). Update process will be
						 * waiting for receiver to set read_jnl_seqno to a non-zero value. Finish it now.
						 * In case this is a reconnect, recvpool.upd_proc_local->read_jnl_seqno already
						 * reflects how much the update process has processed so that should be untouched
						 * by the receiver server as it will otherwise confuse the concurrently running
						 * update process (see <C9J02_003091_updproc_assert_fail_due_to_seqno_gap>).
						 * In addition do not touch recvpool_ctl->jnl_seqno in this case as we need to
						 * resume filling in the receive pool from where the previous connection stopped.
						 */
						assert(0 == GET_STRM_INDEX(recvd_jnl_seqno));
						if (0 == recvpool.upd_proc_local->read_jnl_seqno)
						{	/* Set read_jnl_seqno after jnl_seqno. Update process reads it in opposite
							 * order. Have memory barriers in between to ensure no out-of-order reads.
							 */
							recvpool.recvpool_ctl->jnl_seqno = recvd_jnl_seqno;
							SHM_WRITE_MEMORY_BARRIER;
							recvpool.upd_proc_local->read_jnl_seqno = recvd_jnl_seqno;
							repl_log(gtmrecv_log_fp, TRUE, TRUE, "Wrote upd_proc_local->read_jnl_seqno"
								" : "INT8_FMT" "INT8_FMTX"\n", recvd_jnl_seqno, recvd_jnl_seqno);
							/* If -NORESYNC was used in receiver startup, we dont want to use it
							 * anymore as future such connections could cause recvpool_ctl->jnl_seqno
							 * to be reset further backwards and will confuse the update process.
							 */
							if (recvpool.gtmrecv_local->noresync)
								recvpool.gtmrecv_local->noresync = FALSE;
						} else
						{
							assert(recvpool.recvpool_ctl->jnl_seqno
								>= recvpool.upd_proc_local->read_jnl_seqno);
							assert(!recvpool.gtmrecv_local->noresync);
						}
					}
					/* Now that recvpool.recvpool_ctl->jnl_seqno has been determined for sure (for
					 * non-supplementary or non-root-primary instances it is determined even before but
					 * otherwise it takes until now). Force a log on the first recv. This function uses
					 * recvpool.recvpool_ctl->jnl_seqno hence the placement here.
					 */
					gtmrecv_reinit_logseqno();
					break;

				case REPL_INST_NOHIST:
					if (0 == data_len)
					{
						assert(msg_len == MIN_REPL_MSGLEN - REPL_MSG_HDRLEN);
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Originating instance encountered a "
							"REPLINSTNOHIST error. JNL_SEQNO of this replicating instance precedes"
							" the current history in the originating instance file. "
							"Receiver server exiting.\n");
						gtmrecv_autoshutdown();	/* should not return */
						assert(FALSE);
					}
					break;

				case REPL_LOGFILE_INFO:
					if (0 == data_len)
					{
						logfile_msgp = (repl_logfile_info_msg_t *)(buffp - msg_len - REPL_MSG_HDRLEN);
						assert(REPL_PROTO_VER_REMOTE_LOGPATH <= logfile_msgp->proto_ver);
						if (remote_side->cross_endian)
						{
							logfile_msgp->fullpath_len = GTM_BYTESWAP_32(logfile_msgp->fullpath_len);
							logfile_msgp->pid = GTM_BYTESWAP_32(logfile_msgp->pid);
						}
						assert('\0' == logfile_msgp->fullpath[logfile_msgp->fullpath_len - 1]);
						repl_log(gtmrecv_log_fp, TRUE, TRUE, "Remote side source log file path is %s; "
								"Source Server PID = %d\n",
								logfile_msgp->fullpath, logfile_msgp->pid);
						/* Now, send our logfile path to the source side */
						assert(remote_side->endianness_known);
						len = repl_logfileinfo_get(recvpool.gtmrecv_local->log_file,
										&logfile_msg,
										remote_side->cross_endian,
										gtmrecv_log_fp);
						REPL_SEND_LOOP(gtmrecv_sock_fd, &logfile_msg, len, REPL_POLL_NOWAIT)
						{
							GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
						}
						CHECK_REPL_SEND_LOOP_ERROR(status, "REPL_LOGFILE_INFO");
					}
					break;
				default:
					/* Discard the message */
					repl_log(gtmrecv_log_fp, TRUE, TRUE, "Received UNKNOWN message (type = %d). "
						"Discarding it.\n", msg_type);
					assert(FALSE);
					buffp += buffered_data_len;
					buff_unprocessed -= buffered_data_len;
					data_len -= buffered_data_len;
					break;
			}
			if (repl_connection_reset)
				return;
		}
		assert(0 == ((unsigned long)(buffp) % REPL_MSG_ALIGN));
		if (!preserve_buffp)
		{
			if ((0 != buff_unprocessed) && (buff_start != buffp))
			{
				REPL_DPRINT4("Incmpl msg hdr, moving %d bytes from %lx to %lx\n", buff_unprocessed, (caddr_t)buffp,
					     (caddr_t)buff_start);
				memmove(buff_start, buffp, buff_unprocessed);
			}
			buffp = buff_start;
		}
		GTMRECV_POLL_ACTIONS(data_len, buff_unprocessed, buffp);
	}
}

void repl_cmp_solve_rcv_timeout(void)
{
	GTMASSERT;
}

STATICFNDEF void gtmrecv_heartbeat_timer(TID tid, int4 interval_len, int *interval_ptr)
{
	assert(0 != gtmrecv_now);
	UNIX_ONLY(assert(*interval_ptr == heartbeat_period);)	/* interval_len and interval_ptr are dummies on VMS */
	gtmrecv_now += heartbeat_period;
	REPL_DPRINT2("Starting heartbeat timer with %d s\n", heartbeat_period);
	start_timer((TID)gtmrecv_heartbeat_timer, heartbeat_period * 1000, gtmrecv_heartbeat_timer, SIZEOF(heartbeat_period),
			&heartbeat_period); /* start_timer expects time interval in milli seconds, heartbeat_period is in seconds */
}

STATICFNDEF void gtmrecv_main_loop(boolean_t crash_restart)
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

	if (ZLIB_CMPLVL_NONE != gtm_zlib_cmp_level)
		gtm_zlib_init();	/* Open zlib shared library for compression/decompression */
	recvpool_ctl = recvpool.recvpool_ctl;
	upd_proc_local = recvpool.upd_proc_local;
	gtmrecv_local = recvpool.gtmrecv_local;

	/* Check all message sizes are the same size (32 bytes = MIN_REPL_MSGLEN) except for the REPL_OLD_TRIPLE message
	 * (repl_histrec_msg_t structure) which is 8 bytes more. Pre-supplementary, the receiver server knew to handle
	 * different sized messages only for a few messages types REPL_TR_JNL_RECS, REPL_OLD_TRIPLE and REPL_CMP_SOLVE.
	 * But post-supplementary it knows to handle different sized messages for various additional message types
	 * (including REPL_NEED_INSTINFO, REPL_INSTINFO, REPL_HISTREC).
	 */
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_start_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_start_reply_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_resync_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_old_needinst_msg_t));
	assert(MIN_REPL_MSGLEN <  SIZEOF(repl_needinst_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_needhistinfo_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_old_instinfo_msg_t));
	assert(MIN_REPL_MSGLEN <  SIZEOF(repl_instinfo_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_histinfo1_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_histinfo2_msg_t));
	assert(MIN_REPL_MSGLEN < SIZEOF(repl_histinfo_msg_t));
	assert(MIN_REPL_MSGLEN < SIZEOF(repl_old_triple_msg_t));
	assert(MIN_REPL_MSGLEN < SIZEOF(repl_histrec_msg_t));
	assert(MIN_REPL_MSGLEN == SIZEOF(repl_heartbeat_msg_t));
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
