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

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_time.h"
#include "gtm_stat.h"
#include <sys/time.h>

#include <errno.h>
#include <signal.h>
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
#include "hashtab_mname.h"    /* needed for muprec.h */
#include "hashtab_int4.h"     /* needed for muprec.h */
#include "hashtab_int8.h"     /* needed for muprec.h */
#include "buddy_list.h"
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "gt_timer.h"
#include "gtmsource_heartbeat.h"
#include "repl_filter.h"
#include "repl_log.h"
#include "min_max.h"
#include "rel_quant.h"
#include "copy.h"
#include "ftok_sems.h"
#include "repl_instance.h"
#include "gtmmsg.h"
#include "repl_sem.h"
#include "have_crit.h"			/* needed for ZLIB_COMPRESS */
#include "deferred_signal_handler.h"	/* needed for ZLIB_COMPRESS */
#include "gtm_zlib.h"
#include "repl_sort_tr_buff.h"
#include "replgbl.h"
#include "gtmsource_srv_latch.h"
#include "gv_trigger_common.h"

#define MAX_HEXDUMP_CHARS_PER_LINE	26		/* 2 characters per byte + space, 80 column assumed */

#define BREAK_IF_CMP_ERROR(CMPRET, SEND_TR_LEN)											\
{																\
	switch(CMPRET)														\
	{															\
		case Z_MEM_ERROR:												\
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Out-of-memory error from compress function "			\
					"while compressing %d bytes\n", SEND_TR_LEN);						\
			assert(FALSE);												\
			break;													\
		case Z_BUF_ERROR:												\
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Insufficient output buffer error from compress function "	\
					"while compressing %d bytes\n", SEND_TR_LEN);						\
			assert(FALSE);												\
			break;													\
		case Z_STREAM_ERROR:												\
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Compression level %d invalid error from compress function "	\
					"while compressing %d bytes\n", repl_zlib_cmp_level, SEND_TR_LEN);			\
			assert(FALSE);												\
			break;													\
	}															\
}

#define SET_8BYTE_CMP_MSGHDR(SEND_MSGP, SEND_TR_LEN, CMPBUFLEN, MSGHDRLEN)							\
{																\
	SEND_MSGP->type = (SEND_TR_LEN << REPL_TR_CMP_MSG_TYPE_BITS) | REPL_TR_CMP_JNL_RECS;					\
	SEND_MSGP->len = (int4)cmpbuflen + msghdrlen;										\
	/* Note that a compressed message need not be 8-byte aligned even though the input message was. So round it up to	\
	 * the nearest align boundary. The actual message will contain the unaligned length which is what the receiver will	\
	 * receive. But the # of bytes transmitted across will be the aligned length.						\
	 */															\
	SEND_TR_LEN = ROUND_UP(SEND_MSGP->len, REPL_MSG_ALIGN);									\
}

#define SET_16BYTE_CMP_MSGHDR(SEND_MSGP, SEND_TR_LEN, CMPBUFLEN, MSGHDRLEN)							\
{																\
	repl_cmpmsg_ptr_t		send_cmpmsgp;										\
																\
	send_cmpmsgp = (repl_cmpmsg_ptr_t)SEND_MSGP;										\
	assert(&send_cmpmsgp->type == &SEND_MSGP->type);									\
	assert(&send_cmpmsgp->len == &SEND_MSGP->len);										\
	send_cmpmsgp->type = REPL_TR_CMP_JNL_RECS2;										\
	/* Note that a compressed message need not be 8-byte aligned even though the input message was. So round it up to	\
	 * the nearest align boundary. The actual message will contain the unaligned length which is what the receiver will	\
	 * receive. But the # of bytes transmitted across will be the aligned length.						\
	 */															\
	send_cmpmsgp->len = (int4)(ROUND_UP(CMPBUFLEN + MSGHDRLEN, REPL_MSG_ALIGN));						\
	send_cmpmsgp->uncmplen = SEND_TR_LEN;											\
	send_cmpmsgp->cmplen = (int4)CMPBUFLEN;											\
	SEND_TR_LEN = SEND_MSGP->len;												\
}

#ifdef GTM_TRIGGER
#define ISSUE_TRIG2NOTRIG_IF_NEEDED												\
{																\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	if (!(TREF(replgbl)).trig_replic_warning_issued && (TREF(replgbl)).trig_replic_suspect_seqno				\
		&& !remote_side->trigger_supported)										\
	{ 	/* Note: The below repl_log text is copied from TRIG2NOTRIG error message content from merrors.msg. Change 	\
		 * to one should be reflected in another									\
		 */														\
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Warning: Sending transaction sequence number %d which used "		\
			"triggers to a replicator that does not support triggers\n", (TREF(replgbl)).trig_replic_suspect_seqno);\
		(TREF(replgbl)).trig_replic_warning_issued = TRUE; /* No more warnings till restart */				\
		(TREF(replgbl)).trig_replic_suspect_seqno = seq_num_zero;							\
	}															\
}
#endif


GBLDEF	repl_msg_ptr_t		gtmsource_msgp = NULL;
GBLDEF	int			gtmsource_msgbufsiz = 0;
GBLDEF	repl_msg_ptr_t		gtmsource_cmpmsgp = NULL;
GBLDEF	int			gtmsource_cmpmsgbufsiz = 0;
GBLDEF	boolean_t		gtmsource_received_cmp2uncmp_msg;
GBLDEF	qw_num			repl_source_data_sent = 0;
GBLDEF	qw_num			repl_source_msg_sent = 0;
GBLDEF	qw_num			repl_source_cmp_sent = 0;
GBLDEF	qw_num			repl_source_lastlog_data_sent = 0;
GBLDEF	qw_num			repl_source_lastlog_msg_sent = 0;
GBLDEF	time_t			repl_source_prev_log_time;
GBLDEF  time_t			repl_source_this_log_time;
GBLDEF	time_t			gtmsource_last_flush_time;

GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	uchar_ptr_t		repl_filter_buff;
GBLREF	int			repl_filter_bufsiz;
GBLREF	volatile time_t		gtmsource_now;
GBLREF	int			gtmsource_sock_fd;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gd_addr			*gd_header;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	repl_ctl_element	*repl_ctl_list;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	int			gtmsource_log_fd;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_filter;
GBLREF	gd_addr			*gd_header;
GBLREF	seq_num			seq_num_zero, seq_num_minus_one, seq_num_one;
GBLREF	unsigned int		jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF	unsigned char		jnl_source_rectype, jnl_dest_maxrectype;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	seq_num			lastlog_seqno;
GBLREF	uint4			log_interval;
GBLREF	qw_num			trans_sent_cnt, last_log_tr_sent_cnt;
GBLREF	repl_conn_info_t	*this_side, *remote_side;
GBLREF	int4			strm_index;
GBLREF	uint4			process_id;
GBLREF	seq_num			gtmsource_save_read_jnl_seqno;

error_def(ERR_JNLNEWREC);
error_def(ERR_JNLSETDATA2LONG);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLFTOKSEM);
error_def(ERR_REPLGBL2LONG);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_REPLNOMULTILINETRG);
error_def(ERR_REPLRECFMT);
error_def(ERR_REPLXENDIANFAIL);
error_def(ERR_SECNODZTRIGINTP);
error_def(ERR_TRIG2NOTRIG);
error_def(ERR_TEXT);

/* Endian converts the given set of journal records (possibly multiple sequence numbers) so that the secondary can consume them
 * as-is. This is done only in the case when the primary is running on a GT.M version less than the GT.M version on secondary
 * side. Otherwise, the secondary takes the responsibility of doing the endian conversion. Note that the endian conversion happens
 * in-place. The below function is based on gtmrecv_process.c/repl_tr_endian_convert()
 */
static void repl_tr_endian_convert(repl_msg_ptr_t send_msgp, int send_tr_len, seq_num pre_read_seqno)
{
	uchar_ptr_t		buffp, jb;
	DEBUG_ONLY(uchar_ptr_t	jstart;)
	int			buflen, remaining_len, jlen, reclen, status, nodeflags_keylen, temp_val, keylen;
	jnl_record		*rec;
	enum jnl_record_type	rectype;
	jrec_suffix		*suffixp;
	jnl_string		*keystr;
	mstr_len_t		*vallen_ptr;
	/* seq_num		good_seqno; */

	buffp = send_msgp->msg;
	buflen = send_msgp->len - REPL_MSG_HDRLEN;
	remaining_len = send_tr_len;
	/* QWASSIGN(good_seqno, seq_num_zero); */
	while (0 < remaining_len)
	{
		jlen = buflen;
		jb = buffp;
		while (JREC_PREFIX_SIZE <= jlen)
		{
			DEBUG_ONLY(jstart = jb);
			rec = (jnl_record *)(jb);
			/* endian convert the prefix fields. Not all of the prefix fields are used by the secondary. Only rectype
			 * and forwptr are needed.
			 */
			rectype = (enum jnl_record_type)rec->prefix.jrec_type;
			reclen = rec->prefix.forwptr;
			rec->prefix.forwptr = GTM_BYTESWAP_24(reclen);
			if (!IS_REPLICATED(rectype) || (0 == reclen) || (reclen > jlen) || (reclen > MAX_LOGI_JNL_REC_SIZE))
			{
				assert(FALSE);
				status = -1;
				break;
			}
			assert(!IS_ZTP(rectype));
			assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype) || (JRT_TCOM == rectype) || (JRT_NULL == rectype));
			/* endian convert the suffix fields. Only backptr needs endian conversion as the other field - suffix_code
			 * is 8 bit.
			 */
			suffixp = ((jrec_suffix *)((unsigned char *)rec + reclen - JREC_SUFFIX_SIZE));
			suffixp->backptr = GTM_BYTESWAP_24(suffixp->backptr);
			/* QWASSIGN(good_seqno, rec->jrec_null.jnl_seqno); */ /* update good_seqno */
			rec->jrec_null.jnl_seqno = GTM_BYTESWAP_64(rec->jrec_null.jnl_seqno);
			/* At this point, we could have a TCOM or NULL or SET/KILL/ZKILL/ZTRIG type of record.
			 * Assert that all of them have "strm_seqno" at the exact same offset so we can avoid
			 * an if/then/else check on the record types in order to endian convert "strm_seqno".
			 */
			assert(&rec->jrec_null.strm_seqno == &rec->jrec_set_kill.strm_seqno);
			assert(&rec->jrec_null.strm_seqno == &rec->jrec_tcom.strm_seqno);
			rec->jrec_null.strm_seqno = GTM_BYTESWAP_64(rec->jrec_null.strm_seqno);
			if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
			{
				keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
				assert(keystr == (jnl_string *)&rec->jrec_ztworm.ztworm_str);
				assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
				rec->jrec_set_kill.update_num = GTM_BYTESWAP_32(rec->jrec_set_kill.update_num);
				/* From V19 onwards, the 'length' field is divided into 8 bit 'nodeflags' and 24 bit 'length'
				 * fields.
				 */
				keylen = keystr->length;
				nodeflags_keylen = *(jnl_str_len_t *)keystr;
				*(jnl_str_len_t *)keystr = GTM_BYTESWAP_32(nodeflags_keylen);
				if (IS_SET(rectype) || IS_ZTWORM(rectype))
				{ 	/* SET and ZTWORM records have a 'value' part which needs to be endian converted */
					vallen_ptr = (mstr_len_t *)&keystr->text[keylen];
					GET_MSTR_LEN(temp_val, vallen_ptr);
					temp_val = GTM_BYTESWAP_32(temp_val);
					PUT_MSTR_LEN(vallen_ptr, temp_val);
					/* The actual 'value' itself is a character array and hence needs no endian conversion */
				}
			} else if (JRT_TCOM == rectype)
			{
				assert((unsigned char *)&rec->jrec_tcom.token_seq
					+ SIZEOF(token_seq_t) == (unsigned char *)&rec->jrec_tcom.filler_short);
				/* endian convert num_participants */
				rec->jrec_tcom.num_participants = GTM_BYTESWAP_16(rec->jrec_tcom.num_participants);
			}
			/* else records can only be JRT_NULL. The only relevant field in JRT_NULL is the sequence number which is
			 * already endian converted.
			 */
			assert(jstart == jb); /* endian conversion should always happen in-place. */
			jlen -= reclen;
			jb += reclen;
		}
		if ((-1 == status) || (0 != jlen))
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLXENDIANFAIL, 3,
					LEN_AND_LIT("Originating"), &pre_read_seqno);
		}
		/* move on to the next transaction */
		remaining_len -= (buflen + REPL_MSG_HDRLEN);
		buffp += buflen;
		assert((REPL_TR_JNL_RECS == ((repl_msg_ptr_t)(buffp))->type) || (0 == remaining_len));
		buflen = ((repl_msg_ptr_t)(buffp))->len - REPL_MSG_HDRLEN;
		buffp += REPL_MSG_HDRLEN;
	}
	if (0 != remaining_len)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLXENDIANFAIL, 3, LEN_AND_LIT("Originating"), &pre_read_seqno);
		assert(FALSE);
	}
}

/* The work-horse of the Source Server */
int gtmsource_process(void)
{
	gtmsource_local_ptr_t		gtmsource_local;
	jnlpool_ctl_ptr_t		jctl;
	seq_num				recvd_seqno, sav_read_jnl_seqno;
	seq_num				recvd_jnl_seqno, tmp_read_jnl_seqno;
	int				data_len, srch_status;
	unsigned char			*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int				tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int				torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int				status;					/* needed for REPL_{SEND,RECV}_LOOP */
	int				tot_tr_len, send_tr_len, remaining_len, pre_cmpmsglen;
	int				recvd_msg_type, recvd_start_flags;
	uchar_ptr_t			in_buff, out_buff, out_buffmsg;
	uint4				in_buflen, out_buflen, out_bufsiz;
	seq_num				log_seqno, diff_seqno, pre_read_seqno, post_read_seqno, jnl_seqno;
	char				err_string[1024];
	boolean_t			xon_wait_logged, already_communicated;
	double				time_elapsed;
	seq_num				resync_seqno, zqgblmod_seqno, filter_seqno;
	gd_region			*reg, *region_top;
	sgmnt_addrs			*csa, *repl_csa;
	qw_num				delta_sent_cnt, delta_data_sent, delta_msg_sent;
	time_t				prev_now;
	int				index, poll_time;
	uint4				temp_ulong;
	unix_db_info			*udi;
	repl_histinfo			remote_histinfo, local_histinfo;
	int4				num_histinfo, max_epoch_interval;
	seq_num				local_jnl_seqno, tmp_seqno;
	repl_msg_t			xoff_ack, instnohist_msg, losttncomplete_msg;
	repl_msg_ptr_t			send_msgp;
	repl_cmpmsg_ptr_t		send_cmpmsgp;
	repl_start_reply_msg_ptr_t	reply_msgp;
	boolean_t			rollback_first, secondary_ahead, secondary_was_rootprimary;
	boolean_t			intfilter_error, skip_last_histinfo_check, msg_is_cross_endian, retval;
	int				semval, cmpret;
	uLongf				cmpbuflen;
	int4				msghdrlen;
	Bytef				*cmpbufptr;
	char				histdetail[256];
	gtm_time4_t			tmp_time4;
	repl_heartbeat_msg_ptr_t	heartbeat_msg;
	sm_global_latch_ptr_t		gtmsource_srv_latch;
	DEBUG_ONLY(uchar_ptr_t		save_inbuff;)
	DEBUG_ONLY(uchar_ptr_t		save_outbuff;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open);
	DEBUG_ONLY(
		repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		assert(!repl_csa->hold_onto_crit); /* so we can do unconditional grab_lock/rel_lock */
		ASSERT_VALID_JNLPOOL(repl_csa);
	)
	assert(REPL_MSG_HDRLEN == SIZEOF(jnldata_hdr_struct)); /* necessary for reading multiple transactions from jnlpool in
								* a single attempt */
	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	gtmsource_msgp = NULL;
	gtmsource_msgbufsiz = MAX_REPL_MSGLEN;
	if (ZLIB_CMPLVL_NONE != gtm_zlib_cmp_level)
		gtmsource_cmpmsgp = NULL;

	assert(REPL_POLL_WAIT < MILLISECS_IN_SEC);
	assert(GTMSOURCE_IDLE_POLL_WAIT < REPL_POLL_WAIT);

	gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
	gtmsource_srv_latch = &gtmsource_local->gtmsource_srv_latch;
	/* Below is a simplistic representation of the state diagram of a source server.
	 *
	 *      ------------------------------
	 *            GTMSOURCE_START
	 *      ------------------------------
	 *                    |
	 *                    | (startup state)
	 *                    v
	 *      ------------------------------
	 *     GTMSOURCE_WAITING_FOR_CONNECTION
	 *      ------------------------------
	 *                    |
	 *                    | (gtmsource_est_conn)
	 *                    v
	 *      ------------------------------
	 *       GTMSOURCE_WAITING_FOR_RESTART
	 *      ------------------------------
	 *                    |
	 *                    | (gtmsource_recv_restart)
	 *                    v
	 *      ------------------------------
	 *     GTMSOURCE_SEARCHING_FOR_RESTART
	 *      ------------------------------
	 *                    |
	 *                    | (gtmsource_srch_restart)
	 *                    v
	 *      ------------------------------
	 *        GTMSOURCE_SENDING_JNLRECS         <---------\
	 *      ------------------------------                |
	 *                    |                               |
	 *                    | (receive REPL_XOFF)           |
	 *                    | (receive REPL_XOFF_ACK_ME)    |
	 *                    v                               |
	 *            ------------------------------          ^
	 *              GTMSOURCE_WAITING_FOR_XON             |
	 *            ------------------------------          |
	 *                    |                               |
	 *                    v (receive REPL_XON)            |
	 *                    |                               |
	 *                    \--------------------->---------/
	 */
	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	/* Before entering the loop find the max EPOCH interval (for use in lock waits) */
	max_epoch_interval = 0;
	for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
	{
		csa = &FILE_INFO(reg)->s_addrs;
		if (max_epoch_interval < csa->hdr->epoch_interval)
			max_epoch_interval = csa->hdr->epoch_interval;
	}
	while (TRUE)
	{
		assert(!udi->grabbed_ftok_sem);
		gtmsource_stop_heartbeat();
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
		{
			/* Ensure we don't hold any locks at this moment */
			assert(process_id != gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid);
			assert(!have_crit(CRIT_HAVE_ANY_REG)); /* checks both journal pool lock and database crit lock */
			assert(FD_INVALID != gtmsource_sock_fd);
			if (FD_INVALID != gtmsource_sock_fd)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Closing connection due to ONLINE ROLLBACK\n");
				repl_close(&gtmsource_sock_fd);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
			}
			jnl_seqno = jnlpool.jnlpool_ctl->jnl_seqno;
			repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO - Current Jnlpool Seqno : "INT8_FMT"\n",
					jnl_seqno);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO - Last Seqno sent : "INT8_FMT"\n",
					gtmsource_local->read_jnl_seqno - 1);
			/* gtmsource_save_read_jnl_seqno is kept uptodate with gtmsource_local->read_addr and gtmsource_local->read
			 * fields in gtmsource_onln_rlbk_clnup. But, gtmsource_local->read_jnl_seqno is still pointing to the last
			 * sequence number that we sent to the receiver (which could have been rolled back now). We don't want to
			 * continue with a stale value of read_jnl_seqno. So, set it to gtmsource_save_read_jnl_seqno which itself
			 * is taken from the jnlpool_ctl->jnl_seqno right when we detected the online rollback. We could have set
			 * this right when we set gtmsource_save_read_jnl_seqno but we don't do that because we want to print the
			 * old value in the log file but we can't use repl_log/gtmsource_log_fp in gtmsource_onln_rlbk_clnup() as
			 * it is bundled up as part of libgtmshr.so whereas repl_log is bundled in libmupip.a.
			 */
			gtmsource_local->read_jnl_seqno = gtmsource_save_read_jnl_seqno;
			repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO - Source Server Read Seqno is now set to : "INT8_FMT"\n",
					gtmsource_local->read_jnl_seqno);
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
			assert(READ_FILE == gtmsource_local->read_state);
			gtmsource_ctl_close(); /* can't rely on the journal files anymore since rollback could have touched them */
		}
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
		{
			gtmsource_start_jnl_release_timer();
			gtmsource_est_conn();
			gtmsource_stop_jnl_release_timer();
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			repl_source_data_sent = repl_source_msg_sent = repl_source_cmp_sent = 0;
			repl_source_lastlog_data_sent = 0;
			repl_source_lastlog_msg_sent = 0;

			gtmsource_alloc_msgbuff(MAX_REPL_MSGLEN, TRUE);
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_RESTART;
			recvd_start_flags = START_FLAG_NONE;
			repl_source_prev_log_time = time(NULL);
		}
		if (GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state &&
		    SS_NORMAL != (status = gtmsource_recv_restart(&recvd_seqno, &recvd_msg_type, &recvd_start_flags)))
		{
			if (EREPL_RECV == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{	/* Connection reset */
					repl_log(gtmsource_log_fp, TRUE, TRUE,
							"Connection reset while receiving restart SEQNO. Status = %d ; %s\n",
							status, STRERROR(status));
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				} else
				{
					SNPRINTF(err_string, SIZEOF(err_string),
							"Error receiving RESTART SEQNO. Error in recv : %s", STRERROR(status));
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
							 LEN_AND_STR(err_string));
				}
			} else if (EREPL_SEND == repl_errno)
			{
				if (REPL_CONN_RESET(status))
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE,
					       "Connection reset while sending XOFF_ACK due to possible update process shutdown. "
					       "Status = %d ; %s\n", status, STRERROR(status));
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				}
				SNPRINTF(err_string, SIZEOF(err_string), "Error sending XOFF_ACK_ME message. Error in send : %s",
						STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						LEN_AND_STR(err_string));
			} else if (EREPL_SELECT == repl_errno)
			{
				SNPRINTF(err_string, SIZEOF(err_string), "Error receiving RESTART SEQNO/sending XOFF_ACK_ME.  "
						"Error in select : %s", STRERROR(status));
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						 LEN_AND_STR(err_string));
			}
		}
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			return (SS_NORMAL);
		/* Connection might have been closed if "gtmsource_recv_restart" got an unexpected message. In that case
		 * re-establish the same by continuing to the beginning of this loop. */
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
			continue;
		assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
		assert((GTMSOURCE_SEARCHING_FOR_RESTART == gtmsource_state) || (GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state));
		/* Receiver runs on a version of GT.M that supports multi-site capability */
		/* If gtmsource_state == GTMSOURCE_SEARCHING_FOR_RESTART, we have already communicated with the
		 * receiver and hence checked the instance info so no need to do it again.
		 */
		if (GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state)
		{	/* Get replication instance info */
			DEBUG_ONLY(secondary_was_rootprimary = -1;)
			/* Note: As part of the REPL_INSTINFO message, the receiver could be sending a non-zero "strm_jnl_seqno"
			 * in some cases. If so, it will override "recvd_seqno" we saw before in the REPL_START_JNL_SEQNO message.
			 */
			if (!gtmsource_get_instance_info(&secondary_was_rootprimary, &recvd_seqno))
			{
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
				else if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
					continue;
				else
				{	/* Got a REPL_XOFF_ACK_ME from the receiver. Restart the initial handshake */
					assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state);
					continue;
				}
			}
			assert((FALSE == secondary_was_rootprimary) || (TRUE == secondary_was_rootprimary));
		}
		rollback_first = FALSE;
		secondary_ahead = FALSE;
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
			continue;
		local_jnl_seqno = jctl->jnl_seqno;
		rel_lock(jnlpool.jnlpool_dummy_reg);
		/* Take care to set the flush parameter in repl_log calls below to FALSE until at least the first message
		 * gets sent back. This is so the fetchresync rollback on the other side does not timeout before receiving
		 * a response. */
		assert(0 == GET_STRM_INDEX(recvd_seqno));
		assert(0 == GET_STRM_INDEX(local_jnl_seqno));
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Current Journal Seqno of the instance is "INT8_FMT" "INT8_FMTX"\n",
			local_jnl_seqno, local_jnl_seqno);
		if (recvd_seqno > local_jnl_seqno)
		{	/* Secondary journal seqno is greater than that of the Primary. We know it is ahead of the primary. */
			secondary_ahead = TRUE;
			repl_log(gtmsource_log_fp, TRUE, FALSE,
				"Secondary instance journal seqno "INT8_FMT" "INT8_FMTX" is greater than Primary "
				"instance journal seqno "INT8_FMT" "INT8_FMTX"\n",
				recvd_seqno, recvd_seqno, local_jnl_seqno, local_jnl_seqno);
			/* Since the secondary is at least multi-site, the determination of the rollback seqno involves comparing
			 * the histinfo records between the primary and secondary starting down from "local_jnl_seqno-1"
			 * (done below). In either case, the secondary has to roll back to at most "local_jnl_seqno".
			 * Reset "recvd_seqno" to this number given that we have already recorded that the secondary is
			 * ahead of the primary.
			 */
			recvd_seqno = local_jnl_seqno;
		}
		/* Before setting "next_histinfo_seqno", check if we have at least one histinfo record in the replication instance
		 * file. The only case when there can be no histinfo records is if this instance is a propagating primary. Assert
		 * that. In this case, wait for this instance's primary to send the first histinfo record before setting the
		 * next_histinfo_seqno. Note that we are fetching the value of "num_histinfo" without holding a lock on the instance
		 * file but that is ok since all we care about is if it is 0 or not. We do not rely on the actual value.
		 */
		num_histinfo = jnlpool.repl_inst_filehdr->num_histinfo;
		assert(0 <= num_histinfo);
		assert(num_histinfo || jctl->upd_disabled);
		gtmsource_local->next_histinfo_num = -1;/* Initial value. Reset by the call to "gtmsource_set_next_histinfo_seqno"
							 * invoked in turn by "gtmsource_send_new_histrec" down below */
		if (jctl->upd_disabled && !num_histinfo)
		{	/* Wait for corresponding primary to send a new histinfo record and the receiver server on this instance
			 * to write that to the replication instance file.
			 */
			assert(-1 == gtmsource_local->next_histinfo_num);
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server waiting for first history record to be written by "
				"update process\n");
			do
			{
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_FIRSTHISTINFO);
				gtmsource_poll_actions(FALSE);
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
				num_histinfo = jnlpool.repl_inst_filehdr->num_histinfo;
				if (num_histinfo)	/* Number of histinfos is non-zero */
					break;
			} while (TRUE);
			repl_log(gtmsource_log_fp, TRUE, TRUE,
				"First history record written by update process. Source server proceeding.\n");
		}
		/* Now get the latest histinfo record from the secondary. There are a few exceptions though.
		 * 	1) If we came here because of a BAD_TRANS or CMP2UNCMP message from the receiver server.
		 *		In this case, we have already been communicating with the receiver so no need to
		 *		compare the histinfo record between primary and secondary.
		 *	2) If receiver server was started with -UPDATERESYNC and receiver is running pre-V55000.
		 *		In this case there is no history record on the receiver side to compare against.
		 *		In case the receiver is post-V55000, the -UPDATERESYNC would have required an instance
		 *		file name as the value which would be used towards history record verification.
		 *	3) If receiver server was started with -UPDATERESYNC and receiver is >= V55000 and at a seqno
		 *		which is EQUAL to the earliest seqno for which we have a history record on the primary.
		 *		We have	no way of verifying histories since we definitely dont have the history record
		 *		for the receiver side seqno. Since -updateresync was used, assume they are in sync and
		 *		start replicating from the earliest seqno for which we have a history record on the primary.
		 *	4) If recvd_seqno is 1. In this case, the receiver instance has been created afresh so its instance
		 *		file is empty and we are guaranteed there is nothing to compare. So no point requesting it.
		 *		Besides, this is a very common situation in practice that requiring -updateresync in this
		 *		case seems user-unfriendly so we will let this one go by without a -updateresync particularly
		 *		because there is no harm that can happen by allowing two such instances to connect/replicate.
		 */
		assert(0 != recvd_seqno);
		if (1 == recvd_seqno)
			skip_last_histinfo_check = TRUE;
		else
		{
			if ((GTMSOURCE_WAITING_FOR_RESTART != gtmsource_state) && already_communicated)
				skip_last_histinfo_check = TRUE;
			else if (START_FLAG_UPDATERESYNC & recvd_start_flags)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_START_JNL_SEQNO message has "
					"START_FLAG_UPDATERESYNC bit set\n");
				if (REPL_PROTO_VER_SUPPLEMENTARY > remote_side->proto_ver)
					skip_last_histinfo_check = TRUE;
				else
				{
					assert(jnlpool.repl_inst_filehdr->num_histinfo); /* should be at least 1 history record */
					/* If -updateresync is specified and receiver instance seqno is exactly equal to the
					 * start_seqno of the earliest history record in the instance file, then skip last
					 * histinfo check. Note that in case both source and receiver instances are supplementary,
					 * we should be looking at the 0th stream only. Even in that case, we are guaranteed that
					 * the 0th history record in the instance file corresponds to the 0th stream. So it is
					 * safe to look at the start_seqno of just the 0th history record in all cases.
					 */
					grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
					if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
						continue;
					status = repl_inst_histinfo_get(0, &local_histinfo);
					assert(0 == status); /* Since we pass histinfo_num of 0 which is >=0 and < num_histinfo */
					rel_lock(jnlpool.jnlpool_dummy_reg);
					if (local_histinfo.start_seqno == recvd_seqno)
						skip_last_histinfo_check = TRUE;
					else
						skip_last_histinfo_check = FALSE;
				}
			} else
				skip_last_histinfo_check = FALSE;
		}
		if (!skip_last_histinfo_check)
		{	/* Find histinfo record in the local instance file corresponding to seqno "recvd_seqno-1" */
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
			if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
				continue;
			assert(recvd_seqno <= local_jnl_seqno);
			assert(recvd_seqno <= jctl->jnl_seqno);
			assert((INVALID_SUPPL_STRM == strm_index) || (0 == strm_index));
			status = repl_inst_histinfo_find_seqno(recvd_seqno, strm_index, &local_histinfo);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			assert((0 != status) || (local_histinfo.start_seqno < recvd_seqno));
			if (0 != status)
			{	/* If recvd_seqno is the earliest history record's start_seqno and -udpateresync was
				 * specified, assume the two instances are in sync. Otherwise issue error and close connection.
				 * Send this error status to the receiver server before closing the connection.
				 * This way the receiver will know to shut down rather than loop back trying to
				 * reconnect. This avoids an infinite loop of connection open and closes
				 * between the source server and receiver server.
				 */
				assert(ERR_REPLINSTNOHIST == status); /* only error returned by "repl_inst_histinfo_find_seqno" */
				assert((INVALID_HISTINFO_NUM == local_histinfo.histinfo_num)
						|| (local_histinfo.start_seqno >= recvd_seqno));
				if (!(START_FLAG_UPDATERESYNC & recvd_start_flags)
					|| (INVALID_HISTINFO_NUM == local_histinfo.histinfo_num)
					|| (local_histinfo.start_seqno > recvd_seqno))
				{	/* recvd_seqno is PRIOR to the starting seqno of the instance file.
					 * In that case, issue error and close the connection.
					 */

					SPRINTF(histdetail, "seqno "INT8_FMT" "INT8_FMTX, recvd_seqno - 1, recvd_seqno - 1);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLINSTNOHIST, 4,
							LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
					instnohist_msg.type = REPL_INST_NOHIST;
					instnohist_msg.len = MIN_REPL_MSGLEN;
					memset(&instnohist_msg.msg[0], 0, SIZEOF(instnohist_msg.msg));
					gtmsource_repl_send((repl_msg_ptr_t)&instnohist_msg, "REPL_INST_NOHIST",
						MAX_SEQNO, INVALID_SUPPL_STRM);
					repl_log(gtmsource_log_fp, TRUE, TRUE,
					       "Connection reset due to above REPLINSTNOHIST error\n");
					repl_close(&gtmsource_sock_fd);
					SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
					gtmsource_state = gtmsource_local->gtmsource_state
						= GTMSOURCE_WAITING_FOR_CONNECTION;
					continue;
				}
				assert((0 == local_histinfo.histinfo_num) && (local_histinfo.start_seqno == recvd_seqno));
			}
			if (local_histinfo.start_seqno < recvd_seqno)
			{
				/* Find histinfo record in the remote instance file corresponding to seqno "recvd_seqno-1" */
				retval = gtmsource_get_remote_histinfo(recvd_seqno, &remote_histinfo);
				if (retval)
				{
					assert(remote_histinfo.start_seqno < recvd_seqno);
					/* Check if primary and secondary have same histinfo for "recvd_seqno-1" */
					rollback_first = !gtmsource_is_histinfo_identical(&remote_histinfo, &local_histinfo,
												recvd_seqno, OK_TO_LOG_TRUE);
					/* If local and remote sides are supplementary (i.e. P->Q replication), verify each
					 * stream level history as well. Do this only if the remote side is a receiver server
					 * (i.e. not rollback) and if we still intend on sending a REPL_WILL_RESTART_WITH_INFO
					 * message.
					 */
					assert(this_side->is_supplementary == jnlpool.repl_inst_filehdr->is_supplementary);
					if (this_side->is_supplementary && remote_side->is_supplementary
							&& (REPL_START_JNL_SEQNO == recvd_msg_type)
							&& !rollback_first && !secondary_ahead)
						retval = gtmsource_check_remote_strm_histinfo(recvd_seqno, &rollback_first);
				}
				if (!retval)
				{
					if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						return (SS_NORMAL);
					else if ((GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
							|| (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state))
						continue;
					else
					{	/* Got a REPL_XOFF_ACK_ME from receiver. Restart the initial handshake */
						assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state);
						continue;
					}
				}
			}
		}
		QWASSIGN(sav_read_jnl_seqno, gtmsource_local->read_jnl_seqno);
		reply_msgp = (repl_start_reply_msg_ptr_t)gtmsource_msgp;
		memset(reply_msgp, 0, SIZEOF(*reply_msgp)); /* to identify older releases in the future */
		reply_msgp->len = MIN_REPL_MSGLEN;
		reply_msgp->proto_ver = REPL_PROTO_VER_THIS;
		reply_msgp->node_endianness = NODE_ENDIANNESS;
		reply_msgp->is_supplementary = jnlpool.repl_inst_filehdr->is_supplementary;
		assert((1 != recvd_seqno) || !rollback_first);
		if ((GTMSOURCE_SEARCHING_FOR_RESTART == gtmsource_state) || (REPL_START_JNL_SEQNO == recvd_msg_type))
		{
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SEARCHING_FOR_RESTART;
			/* If the last histinfo record in both instances are NOT the same ("rollback_first" is TRUE)
			 * (possible only if the secondary is multi-site), or if secondary is ahead of the primary
			 * ("secondary_ahead" is TRUE) we do want the secondary to rollback first. Issue message to
			 * do rollback fetchresync. There is one exception though. And that is if -NORESYNC was
			 * specified on the receiver side. In this case, determine the resync/common point by comparing
			 * local and remote histinfo records from the tail of the instance file until we reach
			 * one seqno whose histinfo information is identical in both.
			 * Use this as the common point to send a REPL_WILL_RESTART_WITH_INFO message.
			 */
			if (!rollback_first && !secondary_ahead)
				resync_seqno = recvd_seqno;
			else if (START_FLAG_NORESYNC & recvd_start_flags)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_START_JNL_SEQNO message has "
					"START_FLAG_NORESYNC bit set\n");
				assert(!skip_last_histinfo_check);
				assert(1 != recvd_seqno);
				if (!rollback_first)
				{
					assert(secondary_ahead);
					assert(recvd_seqno == local_jnl_seqno);
					/* The primary and secondary are in sync as of "recvd_seqno" the jnl seqno of the
					 * primary. So that is the common point. Send it across.
					 */
					resync_seqno = recvd_seqno;
				} else
				{
					resync_seqno = gtmsource_find_resync_seqno(&local_histinfo, &remote_histinfo);
					assert((MAX_SEQNO != resync_seqno) || (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						|| (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
						|| (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state));
					rollback_first = FALSE;
				}
			} else
			{	/* Ask secondary to issue a fetchresync rollback */
				repl_log(gtmsource_log_fp, TRUE, FALSE,
					"Secondary instance needs to first do MUPIP JOURNAL ROLLBACK FETCHRESYNC\n");
				resync_seqno = local_jnl_seqno;
				rollback_first = TRUE;
			}
			if (MAX_SEQNO != resync_seqno)
			{
				QWASSIGN(*(seq_num *)&reply_msgp->start_seqno[0], resync_seqno);
				if (!rollback_first)
				{
					assert(NULL != gd_header);
					assert(0 < gd_header->n_regions);
					grab_gtmsource_srv_latch(gtmsource_srv_latch, 2 * gd_header->n_regions * max_epoch_interval,
									HANDLE_CONCUR_ONLINE_ROLLBACK);
					if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
						continue;
					srch_status = gtmsource_srch_restart(resync_seqno, recvd_start_flags);
					rel_gtmsource_srv_latch(&gtmsource_local->gtmsource_srv_latch);
					assert(resync_seqno == gtmsource_local->read_jnl_seqno);
					assert(SS_NORMAL == srch_status);
					reply_msgp->type = REPL_WILL_RESTART_WITH_INFO;
					reply_msgp->jnl_ver = this_side->jnl_ver;
					temp_ulong = (0 == this_side->is_std_null_coll) ?  START_FLAG_NONE : START_FLAG_COLL_M;
					GTMTRIG_ONLY(
						assert(this_side->trigger_supported);
						temp_ulong |= START_FLAG_TRIGGER_SUPPORT;
					)
					PUT_ULONG(reply_msgp->start_flags, temp_ulong);
					recvd_start_flags = START_FLAG_NONE;
					gtmsource_repl_send((repl_msg_ptr_t)reply_msgp, "REPL_WILL_RESTART_WITH_INFO",
						resync_seqno, INVALID_SUPPL_STRM);
				} else
				{	/* Secondary needs to first do FETCHRESYNC rollback to synchronize with primary */
					reply_msgp->type = REPL_ROLLBACK_FIRST;
					gtmsource_repl_send((repl_msg_ptr_t)reply_msgp, "REPL_ROLLBACK_FIRST",
						resync_seqno, INVALID_SUPPL_STRM);
				}
			}
		} else
		{	/* REPL_FETCH_RESYNC received and state is WAITING_FOR_RESTART */
			if (rollback_first || secondary_ahead)
			{	/* Primary and Secondary are currently not in sync */
				if (!rollback_first)
				{	/* We know the secondary is ahead of the primary in terms of journal seqno but the last
					 * histinfo records are identical. This means that the secondary is in sync with the
					 * primary until the primary's journal seqno ("local_jnl_seqno") which should be the new
					 * resync seqno.
					 */
					resync_seqno = local_jnl_seqno;
				} else
				{	/* Determine the resync seqno between this primary and secondary by comparing
					 * local and remote histinfo records from the tail of the instance file until we reach
					 * one seqno whose histinfo information is identical in both.
					 */
					assert(1 != recvd_seqno);
					resync_seqno = gtmsource_find_resync_seqno(&local_histinfo, &remote_histinfo);
					assert((MAX_SEQNO != resync_seqno) || (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						|| (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
						|| (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state));
				}
			} else
			{	/* Primary and Secondary are in sync upto "recvd_seqno". Send it back as the new resync seqno. */
				resync_seqno = recvd_seqno;
			}
			if (MAX_SEQNO != resync_seqno)
			{
				assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state && REPL_FETCH_RESYNC == recvd_msg_type);
				reply_msgp->type = REPL_RESYNC_SEQNO;
				QWASSIGN(*(seq_num *)&reply_msgp->start_seqno[0], resync_seqno);
				gtmsource_repl_send((repl_msg_ptr_t)reply_msgp, "REPL_RESYNC_SEQNO",
					resync_seqno, INVALID_SUPPL_STRM);
			}
		}
		if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
			return (SS_NORMAL);	/* "gtmsource_repl_send" or "gtmsource_find_resync_seqno" did not complete */
		if ((GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state) || (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state))
			continue;	/* "gtmsource_repl_send" or "gtmsource_find_resync_seqno" did not complete */
		assert(MAX_SEQNO != resync_seqno);
		/* After having established connection, initialize a few fields in the gtmsource_local
		 * structure and flush those changes to the instance file on disk.
		 */
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
			continue;
		gtmsource_local->connect_jnl_seqno = jctl->jnl_seqno;
		gtmsource_local->send_losttn_complete = jctl->send_losttn_complete;
		/* Now that "connect_jnl_seqno" has been updated, flush it to corresponding gtmsrc_lcl on disk */
		repl_inst_flush_gtmsrc_lcl();	/* this requires the jnlpool lock to be held */
		rel_lock(jnlpool.jnlpool_dummy_reg);
		if (REPL_WILL_RESTART_WITH_INFO != reply_msgp->type)
		{
			assert(reply_msgp->type == REPL_RESYNC_SEQNO || reply_msgp->type == REPL_ROLLBACK_FIRST);
			if ((REPL_RESYNC_SEQNO == reply_msgp->type) && secondary_was_rootprimary)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Sent REPL_RESYNC_SEQNO message with SEQNO "
					INT8_FMT" "INT8_FMTX"\n",
					(*(seq_num *)&reply_msgp->start_seqno[0]), (*(seq_num *)&reply_msgp->start_seqno[0]));
				region_top = gd_header->regions + gd_header->n_regions;
				assert(NULL != jnlpool.jnlpool_dummy_reg);
				grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
				if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
					continue;
				zqgblmod_seqno = jctl->max_zqgblmod_seqno;
				if (0 == zqgblmod_seqno)
				{	/* If zqgblmod_seqno in all file headers is 0, it implies that this is the first
					 * FETCHRESYNC rollback after the most recent MUPIP REPLIC -LOSTTNCOMPLETE command.
					 * Therefore reset zqgblmod_seqno to the rollback seqno. If not the first rollback,
					 * then zqgblmod_seqno will be reset only if the new rollback seqno is lesser
					 * than the current value.
					 */
					zqgblmod_seqno = MAX_SEQNO;	/* actually 0xFFFFFFFFFFFFFFFF (max possible seqno) */
					/* Reset any pending MUPIP REPLIC -SOURCE -LOSTTNCOMPLETE */
					jctl->send_losttn_complete = FALSE;
					gtmsource_local->send_losttn_complete = jctl->send_losttn_complete;
				}
				rel_lock(jnlpool.jnlpool_dummy_reg);
				REPL_DPRINT2("BEFORE FINDING RESYNC - zqgblmod_seqno is "INT8_FMT, zqgblmod_seqno);
				REPL_DPRINT2(", curr_seqno is "INT8_FMT"\n", jctl->jnl_seqno);
				if (zqgblmod_seqno > resync_seqno)
				{	/* reset "zqgblmod_seqno" and "zqgblmod_tn" in all fileheaders to "resync_seqno" */
					if (SS_NORMAL != gtmsource_update_zqgblmod_seqno_and_tn(resync_seqno))
					{
						assert(GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state);
						if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
							continue;
					}

				}
			}
			/* Could send a REPL_CLOSE_CONN message here */
			/* It is expected that on receiving this msg, the Receiver Server will break the connection and exit. */
		 	repl_close(&gtmsource_sock_fd);
		 	LONG_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT); /* may not be needed after REPL_CLOSE_CONN is sent */
		 	gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
		 	continue;
		}
		/* Now that REPL_WILL_RESTART_WITH_INFO message has been sent, if compression of the replication stream is
		 * requested, check if the receiver server supports ability to decompress. Dont do this if this receiver has
		 * previously sent a REPL_CMP2UNCMP message.
		 */
		gtmsource_local->repl_zlib_cmp_level = repl_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* no compression by default */
		if (!gtmsource_received_cmp2uncmp_msg && (ZLIB_CMPLVL_NONE != gtm_zlib_cmp_level))
		{
			if (REPL_PROTO_VER_MULTISITE_CMP <= remote_side->proto_ver)
			{	/* Receiver server is running a version of GT.M that supports compression of replication stream.
				 * Send test message with compressed data to check if it is able to decompress properly. If so,
				 * enable compression on the replication pipe. Compression level set in repl_zlib_cmp_level.
				 */
				if (!gtmsource_get_cmp_info(&repl_zlib_cmp_level))
				{
					if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						return (SS_NORMAL);
					else if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
						continue;
					else
					{	/* Got a REPL_XOFF_ACK_ME from receiver. Restart the initial handshake */
						assert(GTMSOURCE_WAITING_FOR_RESTART == gtmsource_state);
						continue;
					}
				}
				/* Note down replication cmp_level in this source-server specific structure in journal pool */
				gtmsource_local->repl_zlib_cmp_level = repl_zlib_cmp_level;
			} else
			{
				repl_log(gtmsource_log_fp, TRUE, FALSE,
					"Receiver server does not support compressed data on the replication pipe\n");
				repl_log(gtmsource_log_fp, TRUE, FALSE, "Defaulting to NO compression\n");
			}
		}
		if (QWLT(gtmsource_local->read_jnl_seqno, sav_read_jnl_seqno) && (NULL != repl_ctl_list))
		{	/* The journal files may have been positioned ahead of the read_jnl_seqno for the next read.
			 * Indicate that they have to be repositioned into the past.
			 */
			assert(READ_FILE == gtmsource_local->read_state);
			gtmsource_set_lookback();
		}
		/* The variable poll_time indicates if we should wait for the receive pipe to be I/O ready and should be set to
		 * a non-zero value ONLY if the source server has nothing to send. At this point we have data to send and so
		 * set poll_time to no-wait.
		 */
		poll_time = REPL_POLL_NOWAIT;
		gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
		assert(1 <= gtmsource_local->read_jnl_seqno);
		/* Now that "gtmsource_local->read_jnl_seqno" is initialized, ensure the first send gets logged. */
		gtmsource_reinit_logseqno();
		gtmsource_init_heartbeat();
		/* Internal filters are needed as long as the filter format of the originating side is greater or equal to the
		 * filter format of the secondary side
		 */
		if ((this_side->jnl_ver >= remote_side->jnl_ver)
			&& (IF_NONE != repl_filter_cur2old[remote_side->jnl_ver - JNL_VER_EARLIEST_REPL]))
		{
			assert(IF_INVALID != repl_filter_cur2old[remote_side->jnl_ver - JNL_VER_EARLIEST_REPL]);
			assert(IF_INVALID != repl_filter_old2cur[remote_side->jnl_ver - JNL_VER_EARLIEST_REPL]);
			/* reverse transformation should exist */
			assert(IF_NONE != repl_filter_old2cur[remote_side->jnl_ver - JNL_VER_EARLIEST_REPL]);
			if (this_side->is_std_null_coll != remote_side->is_std_null_coll)
				remote_side->null_subs_xform = (this_side->is_std_null_coll ?
							STDNULL_TO_GTMNULL_COLL : GTMNULL_TO_STDNULL_COLL);
			else
				remote_side->null_subs_xform = FALSE;
			gtmsource_filter |= INTERNAL_FILTER;
			/* Any time the ^#t global format version is bumped, the below assert will trip. This way, anyone who bumps
			 * the trigger label ensures that the internal filter routines in repl_filter.c are accordingly changed to
			 * downgrade triggers before sending them across to a receiver that understands ONLY the prior ^#t format.
			 */
			assert(0 == MEMCMP_LIT(HASHT_GBL_CURLABEL, "2"));
			gtmsource_alloc_filter_buff(gtmsource_msgbufsiz);
		} else
		{
			gtmsource_filter &= ~INTERNAL_FILTER;
			if (NO_FILTER == gtmsource_filter)
				gtmsource_free_filter_buff();
		}
		xon_wait_logged = FALSE;
		/* Flush "gtmsource_local->read_jnl_seqno" to disk right now. This will serve as a reference point for next timed
		 * flush to occur.
		 */
		gtmsource_flush_fh(gtmsource_local->read_jnl_seqno);
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
			continue;
		gtmsource_local->send_new_histrec = TRUE;	/* Send new histinfo unconditionally at start of connection */
		gtmsource_local->next_histinfo_seqno = MAX_SEQNO; /* Initial value. Reset by "gtmsource_send_new_histrec" below */
		assert(-1 == gtmsource_local->next_histinfo_num);
		prev_now = gtmsource_now;
		while (TRUE)
		{
			gtmsource_poll_actions(TRUE);
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;
			if (gtmsource_local->send_losttn_complete)
			{	/* Send LOSTTNCOMPLETE across to the secondary and reset flag to FALSE */
				losttncomplete_msg.type = REPL_LOSTTNCOMPLETE;
				losttncomplete_msg.len = MIN_REPL_MSGLEN;
				gtmsource_repl_send((repl_msg_ptr_t)&losttncomplete_msg, "REPL_LOSTTNCOMPLETE",
					MAX_SEQNO, INVALID_SUPPL_STRM);
				grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
				if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
					break; /* the outerloop will continue */
				gtmsource_local->send_losttn_complete = FALSE;
				rel_lock(jnlpool.jnlpool_dummy_reg);
			}
			if (gtmsource_local->send_new_histrec)
			{	/* We are at the beginning of a new histinfo record boundary. Send a REPL_HISTREC message
				 * before sending journal records for seqnos corresponding to this histinfo.
				 */
				assert(REPL_PROTO_VER_MULTISITE <= remote_side->proto_ver);
				/* Remote version supports multi-site functionality. Send REPL_HISTREC and friends */
				gtmsource_send_new_histrec();
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
				if ((GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
					|| (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state))
					break;
				assert(FALSE == gtmsource_local->send_new_histrec);
			}
			if (prev_now != gtmsource_now)
			{
				prev_now = gtmsource_now;
				if (gtmsource_msgbufsiz - MAX_REPL_MSGLEN > 2 * OS_PAGE_SIZE)
				{	/* We have expanded the buffer by too much (could have been avoided had we sent one
					 * transaction at a time while reading from journal files); let's revert back to our
					 * initial buffer size. If we don't reduce our buffer, it is possible that the buffer keeps
					 * growing (while reading * from journal file) thus making the size of sends while reading
					 * from journal pool very large (> 1 MB).
					 */
					gtmsource_free_filter_buff();
					gtmsource_free_msgbuff();
					gtmsource_alloc_msgbuff(MAX_REPL_MSGLEN, TRUE); /* will also allocate filter buffer */
				}
			}
			/* Check if receiver sent us any control message. Typically, the traffic from receiver to source is very
			 * low compared to traffic in the other direction. More often than not, there will be nothing on the pipe
			 * to receive. Ideally, we should let TCP notify us when there is data on the pipe (async I/O on Unix and
			 * VMS). But, we are not there yet. Since we do a select() before a recv(), we won't block if there is
			 * nothing in the pipe. So, it shouldn't be an expensive operation even if done before every send. Also,
			 * in doing so, we react to an XOFF sooner than later.
			 */
			/* Make sure we don't sleep for an extended period of time if there is something to be sent across */
			assert((GTMSOURCE_SENDING_JNLRECS != gtmsource_state)
					|| ((0 == poll_time) || (GTMSOURCE_IDLE_POLL_WAIT == poll_time)));
			REPL_RECV_LOOP(gtmsource_sock_fd, gtmsource_msgp, MIN_REPL_MSGLEN, poll_time)
			{
				if (0 == recvd_len) /* nothing received in the first attempt, let's try again later */
					break;
				gtmsource_poll_actions(TRUE);
				if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
					return (SS_NORMAL);
				if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
					break;
			}
			if ((SS_NORMAL == status) && (0 != recvd_len))
			{	/* Process the received control message */
				assert(MIN_REPL_MSGLEN == recvd_len);
				REPL_DPRINT3("gtmsource_process: %d bytes received, type is %d\n", recvd_len, gtmsource_msgp->type);
				/* One is not always guaranteed the received message is in source native endian format.
				 * See endianness related comments in gtmsource_recv_restart for why. So be safe and handle
				 * it just like how gtmsource_recv_restart does. The below check works as all messages we
				 * expect at this point have a fixed length of MIN_REPL_MSGLEN.
				 */
				msg_is_cross_endian = (((unsigned)MIN_REPL_MSGLEN < (unsigned)gtmsource_msgp->len)
						&& ((unsigned)MIN_REPL_MSGLEN == GTM_BYTESWAP_32((unsigned)gtmsource_msgp->len)));
				if (msg_is_cross_endian)
				{
					gtmsource_msgp->type = GTM_BYTESWAP_32(gtmsource_msgp->type);
					gtmsource_msgp->len = GTM_BYTESWAP_32(gtmsource_msgp->len);
				}
				assert(MIN_REPL_MSGLEN == gtmsource_msgp->len);
				assert(remote_side->endianness_known);
				switch(gtmsource_msgp->type)
				{
					case REPL_XOFF:
					case REPL_XOFF_ACK_ME:
						gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_XON;
						poll_time = REPL_POLL_WAIT; /* because we are waiting for a REPL_XON */
						repl_log(gtmsource_log_fp, TRUE, TRUE,
							 "REPL_XOFF/REPL_XOFF_ACK_ME received. Send stalled...\n");
						xon_wait_logged = FALSE;
						if (REPL_XOFF_ACK_ME == gtmsource_msgp->type)
						{
							xoff_ack.type = REPL_XOFF_ACK;
							tmp_seqno = *(seq_num *)&gtmsource_msgp->msg[0];
							if (msg_is_cross_endian)
								tmp_seqno = GTM_BYTESWAP_64(tmp_seqno);
							*(seq_num *)&xoff_ack.msg[0] = tmp_seqno;
							xoff_ack.len = MIN_REPL_MSGLEN;
							gtmsource_repl_send((repl_msg_ptr_t)&xoff_ack, "REPL_XOFF_ACK",
								MAX_SEQNO, INVALID_SUPPL_STRM);
							if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
								return (SS_NORMAL);	/* "gtmsource_repl_send" did not complete */
							if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
								break;	/* "gtmsource_repl_send" did not complete */
						}
						break;
					case REPL_XON:
						gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
						poll_time = REPL_POLL_NOWAIT; /* because we received XON and data ready for send */
						repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_XON received\n");
						heartbeat_stalled = FALSE;
						REPL_DPRINT1("Restarting HEARTBEAT\n");
						break;
					case REPL_BADTRANS:
					case REPL_CMP2UNCMP:
					case REPL_START_JNL_SEQNO:
						QWASSIGN(recvd_seqno, *(seq_num *)&gtmsource_msgp->msg[0]);
						if (msg_is_cross_endian)
							recvd_seqno = GTM_BYTESWAP_64(recvd_seqno);
						gtmsource_state = gtmsource_local->gtmsource_state
							= GTMSOURCE_SEARCHING_FOR_RESTART;
						if ((REPL_BADTRANS == gtmsource_msgp->type)
							|| (REPL_CMP2UNCMP == gtmsource_msgp->type))
						{
							already_communicated = TRUE;
							recvd_start_flags = START_FLAG_NONE;
							if (REPL_BADTRANS == gtmsource_msgp->type)
								repl_log(gtmsource_log_fp, TRUE, TRUE, "Received REPL_BADTRANS "
									"message with SEQNO "INT8_FMT" "INT8_FMTX"\n",
									recvd_seqno, recvd_seqno);
							else
							{
								repl_log(gtmsource_log_fp, TRUE, TRUE, "Received REPL_CMP2UNCMP "
									"message with SEQNO "INT8_FMT" "INT8_FMTX"\n",
									recvd_seqno, recvd_seqno);
								repl_log(gtmsource_log_fp, TRUE, FALSE,
									"Defaulting to NO compression for this connection\n");
								gtmsource_received_cmp2uncmp_msg = TRUE;
							}
						} else
						{
							recvd_start_flags = ((repl_start_msg_ptr_t)gtmsource_msgp)->start_flags;
							if (msg_is_cross_endian)
								recvd_start_flags = GTM_BYTESWAP_32(recvd_start_flags);
							already_communicated = FALSE;
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Received REPL_START_JNL_SEQNO message with SEQNO "INT8_FMT" "
								INT8_FMTX". Possible crash of recvr/update process\n",
								recvd_seqno, recvd_seqno);
						}
						break;
					case REPL_HEARTBEAT:
						if (msg_is_cross_endian)
						{
							heartbeat_msg = (repl_heartbeat_msg_ptr_t)gtmsource_msgp;
							tmp_seqno = *(seq_num *)&heartbeat_msg->ack_seqno[0];
							tmp_seqno = GTM_BYTESWAP_64(tmp_seqno);
							*(seq_num *)&heartbeat_msg->ack_seqno[0] = tmp_seqno;
							tmp_time4 = *(gtm_time4_t *)&heartbeat_msg->ack_time[0];
							tmp_time4 = GTM_BYTESWAP_32(tmp_time4);
							*(gtm_time4_t *)&heartbeat_msg->ack_time[0] = tmp_time4;
						}
						gtmsource_process_heartbeat((repl_heartbeat_msg_ptr_t)gtmsource_msgp);
						break;
					default:
						repl_log(gtmsource_log_fp, TRUE, TRUE, "Message of unknown type %d of length %d "
							"bytes received; hex dump follows\n", gtmsource_msgp->type, recvd_len);
						for (index = 0; index < MIN(recvd_len, gtmsource_msgbufsiz - REPL_MSG_HDRLEN); )
						{
							repl_log(gtmsource_log_fp, FALSE, FALSE, "%.2x ",
									gtmsource_msgp->msg[index]);
							if ((++index) % MAX_HEXDUMP_CHARS_PER_LINE == 0)
								repl_log(gtmsource_log_fp, FALSE, TRUE, "\n");
						}
						repl_log(gtmsource_log_fp, FALSE, TRUE, "\n"); /* flush BEFORE the assert */
						assert(FALSE);
						break;
				}
			} else if (SS_NORMAL != status)
			{
				if (EREPL_RECV == repl_errno)
				{
					if (REPL_CONN_RESET(status))
					{
						/* Connection reset */
						repl_log(gtmsource_log_fp, TRUE, TRUE,
							"Connection reset while attempting to receive from secondary."
							" Status = %d ; %s\n", status, STRERROR(status));
						repl_close(&gtmsource_sock_fd);
						SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
						gtmsource_state = gtmsource_local->gtmsource_state
							= GTMSOURCE_WAITING_FOR_CONNECTION;
						break;
					} else
					{
						SNPRINTF(err_string, SIZEOF(err_string),
								"Error receiving Control message from Receiver. Error in recv : %s",
								STRERROR(status));
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								 LEN_AND_STR(err_string));
					}
				} else if (EREPL_SELECT == repl_errno)
				{
					SNPRINTF(err_string, SIZEOF(err_string),
							"Error receiving Control message from Receiver. Error in select : %s",
							STRERROR(status));
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
							 LEN_AND_STR(err_string));
				}
			}
			if (GTMSOURCE_WAITING_FOR_XON == gtmsource_state)
			{
				if (!xon_wait_logged)
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE, "Waiting to receive XON\n");
					heartbeat_stalled = TRUE;
					REPL_DPRINT1("Stalling HEARTBEAT\n");
					xon_wait_logged = TRUE;
				}
				if (GTMSOURCE_FH_FLUSH_INTERVAL <= difftime(gtmsource_now, gtmsource_last_flush_time))
				{
					gtmsource_flush_fh(gtmsource_local->read_jnl_seqno);
					if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
						break; /* the outerloop will continue */
				}
				continue;
			}
			if ((GTMSOURCE_SEARCHING_FOR_RESTART  == gtmsource_state)
				|| (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state))
				break;
			assert(gtmsource_state == GTMSOURCE_SENDING_JNLRECS);
			pre_read_seqno = gtmsource_local->read_jnl_seqno;
			grab_gtmsource_srv_latch(gtmsource_srv_latch, 2 * gd_header->n_regions * max_epoch_interval,
							HANDLE_CONCUR_ONLINE_ROLLBACK);
			if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
				break; /* the outerloop will continue */
			tot_tr_len = gtmsource_get_jnlrecs(&gtmsource_msgp->msg[0], &data_len,
							   gtmsource_msgbufsiz - REPL_MSG_HDRLEN,
							   !(gtmsource_filter & EXTERNAL_FILTER));
			rel_gtmsource_srv_latch(&gtmsource_local->gtmsource_srv_latch);
			/* It is safe to send the journal records as we are guaranteed NO online rollback happened in between
			 * and so we won't be sending garbage
			 */
			if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
				return (SS_NORMAL);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				break;
			if (GTMSOURCE_SEND_NEW_HISTINFO == gtmsource_state)
			{	/* This is a signal from "gtmsource_get_jnlrecs" to send a REPL_HISTREC message first
				 * before sending any more seqnos across. Set "gtmsource_local->send_new_histrec" to TRUE.
				 */
				assert(0 == tot_tr_len);
				gtmsource_local->send_new_histrec = TRUE; /* Will cause a new histinfo record to be sent first */
				gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SENDING_JNLRECS;
				continue;	/* Send a REPL_HISTREC message first and then send journal records */
			}
			post_read_seqno = gtmsource_local->read_jnl_seqno;
			if (0 <= tot_tr_len)
			{
				if (0 < data_len)
				{
					APPLY_EXT_FILTER_IF_NEEDED(gtmsource_filter, gtmsource_msgp, data_len, tot_tr_len);
					gtmsource_msgp->type = REPL_TR_JNL_RECS;
					gtmsource_msgp->len = data_len + REPL_MSG_HDRLEN;
					send_msgp = gtmsource_msgp;
					send_tr_len = tot_tr_len;
					intfilter_error = FALSE;
					if (gtmsource_filter & INTERNAL_FILTER)
					{
						in_buff = gtmsource_msgp->msg;
						in_buflen = data_len; /* size of the first journal record in the converted buffer */
						out_buffmsg = repl_filter_buff;
						out_buff = out_buffmsg + REPL_MSG_HDRLEN;
						out_bufsiz = repl_filter_bufsiz - REPL_MSG_HDRLEN;
						remaining_len = tot_tr_len;
						while (JREC_PREFIX_SIZE <= remaining_len)
						{
							filter_seqno = ((struct_jrec_null *)(in_buff))->jnl_seqno;
							DEBUG_ONLY(
								save_inbuff = in_buff;
								save_outbuff = out_buff;
							)
							APPLY_INT_FILTER(in_buff, in_buflen, out_buff, out_buflen,
											out_bufsiz, status);
							/* Internal filters should not modify the incoming pointers. Assert that. */
							assert((save_inbuff == in_buff) && (save_outbuff == out_buff));
							if (SS_NORMAL == status)
							{	/* adjust various pointers and book-keeping values to move to next
								 * record.
								 */
								((repl_msg_ptr_t)(out_buffmsg))->type = REPL_TR_JNL_RECS;
								((repl_msg_ptr_t)(out_buffmsg))->len = out_buflen + REPL_MSG_HDRLEN;
								out_buffmsg = (out_buff + out_buflen);
								remaining_len -= (in_buflen + REPL_MSG_HDRLEN);
								assert(0 <= remaining_len);
								if (0 >= remaining_len)
									break;
								in_buff += in_buflen;
								in_buflen = ((repl_msg_ptr_t)(in_buff))->len - REPL_MSG_HDRLEN;
								in_buff += REPL_MSG_HDRLEN;
								out_buff = (out_buffmsg + REPL_MSG_HDRLEN);
								out_bufsiz -= (out_buflen + REPL_MSG_HDRLEN);
								assert(0 <= (int)out_bufsiz);
							} else if (EREPL_INTLFILTER_NOSPC == repl_errno)
							{
								REALLOCATE_INT_FILTER_BUFF(out_buff, out_buffmsg, out_bufsiz);
								/* note that in_buff and in_buflen is not changed so that we can
								 * start from where we left
								 */
							} else /* fatal error from the internal filter */
							{
								intfilter_error = TRUE;
								break;
							}
						}
						assert((0 == remaining_len) || intfilter_error);
						GTMTRIG_ONLY(ISSUE_TRIG2NOTRIG_IF_NEEDED;)
						send_msgp = (repl_msg_ptr_t)repl_filter_buff;
						send_tr_len = out_buffmsg - repl_filter_buff;
						if (0 == send_tr_len)
						{	/* This is possible ONLY if the first transaction in the buffer read from
							 * journal pool or disk encountered error while doing internal filter
							 * conversion. Issue rts_error right away as there is nothing much we can
							 * do at this point.
							 */
							assert(intfilter_error);
							assert((EREPL_INTLFILTER_BADREC == repl_errno)
								|| (EREPL_INTLFILTER_REPLGBL2LONG == repl_errno)
								|| (EREPL_INTLFILTER_SECNODZTRIGINTP == repl_errno)
								|| (EREPL_INTLFILTER_MULTILINEXECUTE == repl_errno));
							assert(filter_seqno == pre_read_seqno);
							INT_FILTER_RTS_ERROR(filter_seqno); /* no return */
						}
					}
					assert(send_tr_len && (0 == (send_tr_len % REPL_MSG_ALIGN)));
					/* ensure that the head of the buffer has the correct type and len */
					assert((REPL_TR_JNL_RECS == send_msgp->type)
							&& (0 == (send_msgp->len % JNL_REC_START_BNDRY)));
					/* At this point send_msgp is the buffer to be sent and send_tr_len is the send size */
					assert(remote_side->endianness_known);
					if (remote_side->cross_endian && (this_side->jnl_ver < remote_side->jnl_ver))
					{	/* Cross-endian replication with GT.M version on primary being lesser than that
						 * the secondary. Do the endian conversion in the primary so that the secondary
						 * can consume it as-is.
						 * No return if the below call to repl_tr_endian_convert fails.
						 */
						repl_tr_endian_convert(send_msgp, send_tr_len, pre_read_seqno);
					}
					pre_cmpmsglen = send_tr_len; /* send_tr_len will be updated below */
					if (ZLIB_CMPLVL_NONE != repl_zlib_cmp_level)
					{	/* Compress the journal records before replicating them across the pipe.
						 * Depending on whether the total data length to be sent is within a threshold
						 * or not (see repl_msg.h before REPL_TR_CMP_THRESHOLD #define for why), send
						 * either a REPL_TR_CMP_JNL_RECS or REPL_TR_CMP_JNL_RECS2 message
						 */
						msghdrlen = (REPL_TR_CMP_THRESHOLD > send_tr_len)
									? REPL_MSG_HDRLEN : REPL_MSG_HDRLEN2;
						cmpbuflen = gtmsource_cmpmsgbufsiz - msghdrlen;
						cmpbufptr = ((Bytef *)gtmsource_cmpmsgp) + msghdrlen;
						ZLIB_COMPRESS(cmpbufptr, cmpbuflen, send_msgp, send_tr_len,
								repl_zlib_cmp_level, cmpret);
						BREAK_IF_CMP_ERROR(cmpret, send_tr_len); /* Note: break stmt. inside the macro */
						if (Z_OK == cmpret)
						{	/* Send compressed buffer */
							send_msgp = gtmsource_cmpmsgp;
							if (REPL_TR_CMP_THRESHOLD > send_tr_len)
							{	/* Send REPL_TR_CMP_JNL_RECS message with 8-byte header */
								SET_8BYTE_CMP_MSGHDR(send_msgp, send_tr_len, cmpbuflen, msghdrlen);
							} else
							{	/* Send REPL_TR_CMP_JNL_RECS2 message with 16-byte header */
								SET_16BYTE_CMP_MSGHDR(send_msgp, send_tr_len, cmpbuflen, msghdrlen);
							}
						} else
						{	/* Send normal buffer */
							repl_log(gtmsource_log_fp, TRUE, FALSE, "Defaulting to NO compression\n");
							repl_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* no compression */
							gtmsource_local->repl_zlib_cmp_level = repl_zlib_cmp_level;
						}

					}
					assert((send_tr_len == pre_cmpmsglen) || (ZLIB_CMPLVL_NONE != repl_zlib_cmp_level));
					assert(0 == (send_tr_len % REPL_MSG_ALIGN));
					/* The following loop tries to send multiple seqnos in one shot. resync_seqno gets
					 * updated once the send is completely successful. If an error occurs in the middle
					 * of the send, it is possible that we successfully sent a few seqnos to the other side.
					 * In this case resync_seqno should be updated to reflect those seqnos. Not doing so
					 * might cause the secondary to get ahead of the primary in terms of resync_seqno.
					 * Although it is possible to determine the exact seqno where the send partially failed,
					 * we update resync_seqno as if all seqnos were successfully sent (It is ok for the
					 * resync_seqno on the primary side to be a little more than the actual value as long as
					 * the secondary side has an accurate value of resync_seqno. This is because the
					 * resync_seqno of the system is the minimum of the resync_seqno of both primary
					 * and secondary). This is done by the call to gtmsource_flush_fh() done within the
					 * REPL_SEND_LOOP macro as well as in the (SS_NORMAL != status) if condition below.
					 * Note that all of this is applicable only in a dualsite replication scenario. In
					 * case of a multisite scenario, it is always the receiver server that tells the
					 * sequence number from where the source server should start sending. So, even if
					 * the source server notes down a higher value of journal sequence number in
					 * jnlpool.gtmsource_local->read_jnl_seqno, it is not a problem since the receiver
					 * server will communicate the appropriate sequence number as part of the histinfo
					 * exchange.
					 */
					REPL_SEND_LOOP(gtmsource_sock_fd, send_msgp, send_tr_len, REPL_POLL_WAIT)
					{
						gtmsource_poll_actions(FALSE);
						if (GTMSOURCE_CHANGING_MODE == gtmsource_state)
						{
							gtmsource_flush_fh(post_read_seqno);
							if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
								break;
							return (SS_NORMAL);
						}
					}
					if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
						break; /* the outerloop will continue */
					if (SS_NORMAL != status)
					{
						gtmsource_flush_fh(post_read_seqno);
						if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
							break; /* the outerloop will continue */
						if (REPL_CONN_RESET(status) && EREPL_SEND == repl_errno)
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Connection reset while sending seqno data from "
								INT8_FMT" "INT8_FMTX" to "INT8_FMT" "INT8_FMTX
								". Status = %d ; %s\n", pre_read_seqno, pre_read_seqno,
								post_read_seqno, post_read_seqno, status, STRERROR(status));
							repl_close(&gtmsource_sock_fd);
							SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);
							gtmsource_state = gtmsource_local->gtmsource_state
								= GTMSOURCE_WAITING_FOR_CONNECTION;
							break;
						}
						if (EREPL_SEND == repl_errno)
						{
							SNPRINTF(err_string, SIZEOF(err_string),
								"Error sending DATA. Error in send : %s", STRERROR(status));
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  LEN_AND_STR(err_string));
						}
						if (EREPL_SELECT == repl_errno)
						{
							SNPRINTF(err_string, SIZEOF(err_string),
								"Error sending DATA. Error in select : %s", STRERROR(status));
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
								  LEN_AND_STR(err_string));
						}
					}
					if (intfilter_error)
					{	/* Now that we are done sending whatever buffer was filter converted, issue
						 * the error. This will bring down the source server (due to the rts_error).
						 * At this point, jnlpool.gtmsource_local->read_jnl_seqno could effectively
						 * be behind the receiver server's journal sequence number. But, that is
						 * okay since as part of reconnection (when the source server comes back up),
						 * the receiever server will communicate the appropriate sequence number as
						 * part of the histinfo exchange.
						 */
						assert((EREPL_INTLFILTER_BADREC == repl_errno)
							|| (EREPL_INTLFILTER_REPLGBL2LONG == repl_errno)
							|| (EREPL_INTLFILTER_SECNODZTRIGINTP == repl_errno)
							|| (EREPL_INTLFILTER_MULTILINEXECUTE == repl_errno));
						assert(filter_seqno <= post_read_seqno);
						INT_FILTER_RTS_ERROR(filter_seqno); /* no return */
					}
					jnlpool.gtmsource_local->read_jnl_seqno = post_read_seqno;
					if (GTMSOURCE_FH_FLUSH_INTERVAL <= difftime(gtmsource_now, gtmsource_last_flush_time))
					{
						gtmsource_flush_fh(post_read_seqno);
						if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
							break; /* the outerloop will continue */
					}
					repl_source_cmp_sent += (qw_num)send_tr_len;
					repl_source_msg_sent += (qw_num)pre_cmpmsglen;
					repl_source_data_sent += (qw_num)(pre_cmpmsglen)
								- (post_read_seqno - pre_read_seqno) * REPL_MSG_HDRLEN;
					log_seqno = post_read_seqno - 1; /* post_read_seqno is the "next" seqno to be sent,
									      * not the last one we sent */
					if (gtmsource_logstats || (log_seqno - lastlog_seqno >= log_interval))
					{	/* print always when STATSLOG is ON, or when the log interval has passed */
						trans_sent_cnt += (log_seqno - lastlog_seqno);
						/* jctl->jnl_seqno >= post_read_seqno is the most common case;
						 * see gtmsource_readpool() for when the rare case can occur */
						jnl_seqno = jctl->jnl_seqno;
						if (jnl_seqno >= post_read_seqno - 1)
						{
							diff_seqno = (jnl_seqno >= post_read_seqno) ?
									(jnl_seqno - post_read_seqno) : 0;
							repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO - Seqno : "INT8_FMT" "
								INT8_FMTX"  Jnl Total : "INT8_FMT" "INT8_FMTX
								"  Msg Total : "INT8_FMT" "INT8_FMTX"  CmpMsg Total : "
								INT8_FMT" "INT8_FMTX"  Current backlog : "INT8_FMT" "INT8_FMTX"\n",
								log_seqno, log_seqno, repl_source_data_sent, repl_source_data_sent,
								repl_source_msg_sent, repl_source_msg_sent,
								repl_source_cmp_sent, repl_source_cmp_sent, diff_seqno, diff_seqno);
							/* gtmsource_now is updated by the heartbeat protocol every heartbeat
							 * interval. To cut down on calls to time(), we use gtmsource_now as the
							 * time to figure out if we have to log statistics. This works well as the
							 * logging interval generally is larger than the heartbeat interval, and
							 * that the heartbeat protocol is running when we are sending data. The
							 * consequence although is that we may defer logging when we would have
							 * logged. We can live with that given the benefit of not calling time
							 * related system calls. Currently, the logging interval is not changeable
							 * by users. When/if we provide means of choosing log interval, this code
							 * may have to be re-examined. Vinaya 2003, Sep 08
							 */
							assert(0 != gtmsource_now); /* must hold if we are sending data */
							repl_source_this_log_time = gtmsource_now; /* approximate time, in the
												    * worst case, behind by
												    * heartbeat interval */
							assert(repl_source_this_log_time >= repl_source_prev_log_time);
							time_elapsed = difftime(repl_source_this_log_time,
											repl_source_prev_log_time);
							if ((double)GTMSOURCE_LOGSTATS_INTERVAL <= time_elapsed)
							{
								delta_sent_cnt = trans_sent_cnt - last_log_tr_sent_cnt;
								delta_data_sent = repl_source_data_sent
											- repl_source_lastlog_data_sent;
								delta_msg_sent = repl_source_msg_sent
											- repl_source_lastlog_msg_sent;
								repl_log(gtmsource_log_fp, TRUE, FALSE,
									"REPL INFO since last log : "
									"Time elapsed : %00.f  Tr sent : "INT8_FMT" "INT8_FMTX"  "
									"Tr bytes : "INT8_FMT" "INT8_FMTX
									"  Msg bytes : "INT8_FMT" "INT8_FMTX"\n", time_elapsed,
									delta_sent_cnt, delta_sent_cnt, delta_data_sent,
									delta_data_sent, delta_msg_sent, delta_msg_sent);
								repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL INFO since last log : "
									"Time elapsed : %00.f  Tr sent/s : %f  Tr bytes/s : %f  "
									"Msg bytes/s : %f\n", time_elapsed,
									(float)delta_sent_cnt / time_elapsed,
									(float)delta_data_sent / time_elapsed,
									(float)delta_msg_sent / time_elapsed);
								repl_source_lastlog_data_sent = repl_source_data_sent;
								repl_source_lastlog_msg_sent = repl_source_msg_sent;
								last_log_tr_sent_cnt = trans_sent_cnt;
								repl_source_prev_log_time = repl_source_this_log_time;
							}
							lastlog_seqno = log_seqno;
						} /* else an online rollback occurred. Fall-through and the subsequent iteration
						   * will take care of re-establishing the connection
						   */
					}
					/* Because we sent data to the other side and there might be more to be sent across, don't
					 * wait for the receive pipe to be ready.
					 */
					poll_time = REPL_POLL_NOWAIT;
				} else /* data_len == 0 */
				{	/* nothing to send */
					gtmsource_flush_fh(post_read_seqno);
					if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
						break; /* the outerloop will continue */
					/* Sleep for a while (as part of the next REPL_RECV_LOOP) to avoid spinning when there is no
					 * data to be sent
					 */
					poll_time = GTMSOURCE_IDLE_POLL_WAIT;
				}
			} else /* else tot_tr_len < 0, error */
			{
				if (0 < data_len) /* Insufficient buffer space, increase the buffer space */
					gtmsource_alloc_msgbuff(data_len + REPL_MSG_HDRLEN, TRUE);
				else
					GTMASSERT; /* Major problems */
			}
		}
	}
}
