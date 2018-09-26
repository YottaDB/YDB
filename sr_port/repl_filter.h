/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_FILTER_H
#define _REPL_FILTER_H

#define MAX_FILTER_ARGS		31
#define	READ_END		0
#define	WRITE_END		1

#define FILTERSTART_ERR			-1
#define FILTER_CMD_ARG_DELIM_TOKENS	" \t"

/* Define the maximum jnl extract byte length of ONE line of journal extract.
 * Note that for a TP transaction that has N updates, the journal extract length needed at most is N times this value.
 */
#define MAX_ONE_JREC_EXTRACT_BUFSIZ	10 * MAX_LOGI_JNL_REC_SIZE	/* Since we might need up to 9X + 11
									 * of MAX_LOGI_JNL_REC_SIZE */

#define	JNL2EXTCVT_EXPAND_FACTOR	2  /* # of max-sized journal records by which jnl2extcvt expands buffer if not enough */
#define	EXT2JNLCVT_EXPAND_FACTOR	8  /* # of max-sized journal records by which ext2jnlcvt expands buffer if not enough */

#define NO_FILTER			0
#define INTERNAL_FILTER			0x00000001
#define EXTERNAL_FILTER			0x00000002
#define JNL_EXTRACT_DELIM		'\\'
#define TCOM_EXTR_JNLSEQNO_FIELDNUM	4
#define FILTER_EOT			"99\n"

#define	GTMNULL_TO_STDNULL_COLL		1
#define	STDNULL_TO_GTMNULL_COLL		2

#define	REPL_FILTER_TIMEOUT_MIN		32	/* in seconds */
#define	REPL_FILTER_TIMEOUT_DEF		64	/* in seconds */
#define	REPL_FILTER_TIMEOUT_MAX		131072	/* 2^17 seconds */

typedef int (*intlfltr_t)(uchar_ptr_t, uint4 *, uchar_ptr_t, uint4 *, uint4);

/* The following is the list of filter-format version number versus the earliest GT.M version number that used it.
 * Whenever the filter-format version number changes, it is set to the journal-format version number (JNL_LABEL_TEXT macro)
 * for that version. Although usually a change in the journal file format (i.e. change to JNL_LABEL_TEXT macro in jnl.h)
 * implies a change in the filter format, it is not always the case. This is because the filter format changes only when
 * the format of any REPLICATED jnl record changes as opposed to the jnl format which can change if the jnl file hdr format
 * changes or if the format of a non-replicated journal record (e.g. EPOCH record) changes.
 *
 * We support replication with versions as old as 5 years. To help with that determination every time the journal/filter
 * format changes, the release date of each version is included in the MM/YY column.
 *
 * ------ ------  ------- --------
 * MM/YY  Filter  Journal GT.M
 *        format  format  version
 * ------ ------  ------- --------
 *         V11     V11    V4.2-000
 *         V12     V12    V4.3-000
 *         V12     V13    V4.3-001
 *         V12     V14    V4.3-001A
 *         V15     V15    V4.4-002
 *         V16     V16    V5.0-FT01
 *         V17     V17    V5.0-000  <--- filter support starts here
 *         V17     V18    V5.3-003  <--- (EPOCH record format changed so no filter format change)
 *         V19     V19    V5.4-000  <--- (SET/KILL records have nodeflags, New ZTWORMHOLE record, file header max_jrec_len changes)
 *         V19     V20    V5.4-001  <--- 64K journal file header change in Unix but V20 change for VMS too; No jnlrec format change
 *         V21     V21    V5.4-002  <--- Added replicated ZTRIGGER jnl record type
 * 02/2012 V22     V22    V5.5-000  <--- strm_seqno added to all logical records (supplementary instances)
 * 09/2012 V22     V23    V6.0-000  <--- Various journaling-related limits have changed, allowing for much larger journal records
 * 09/2014 V24     V24    V6.2-000  <--- New logical trigger journal record (TLGTRIG and ULGTRIG jnl records)
 * 12/2014 V24     V25    V6.2-001  <--- No new jnl record but bump needed to replicate logical trigger jnl records (GTM-7509)
 * 05/2015 V24     V26    V6.2-002  <--- No new jnl record but bump needed because of different encryption method
 * 03/2017 V24     V27    V6.3-001  <--- JRT_ALIGN record size reduced from min of 32 bytes to min of 16 bytes.
 *                                       The extract format though did not change as we extract a 0 tn now in -detail extract.
 *                                       The filter format did not change because ALIGN record is not replicated.
 * ??/???? V??     V28    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V29    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V30    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V31    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V32    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V33    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V34    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V35    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V36    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V37    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V38    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V39    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V40    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V41    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V42    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * ??/???? V??     V43    V?.?-???  <--- Space reserved for GT.M changes to jnl record format and potentially filter format
 * 11/2018 V44     V44    r1.24     <--- NULL record has "salvaged" bit indicating auto-generated record (not user-generated)
 */

typedef enum
{
	REPL_JNL_V24,		/* enum corresponding to journal format V24 */
	REPL_JNL_V25,		/* enum corresponding to journal format V25 */
	REPL_JNL_V26,		/* enum corresponding to journal format V26 */
	REPL_JNL_V27,		/* enum corresponding to journal format V27 */
	REPL_JNL_V28,		/* enum corresponding to journal format V28 */
	REPL_JNL_V29,		/* enum corresponding to journal format V29 */
	REPL_JNL_V30,		/* enum corresponding to journal format V30 */
	REPL_JNL_V31,		/* enum corresponding to journal format V31 */
	REPL_JNL_V32,		/* enum corresponding to journal format V32 */
	REPL_JNL_V33,		/* enum corresponding to journal format V33 */
	REPL_JNL_V34,		/* enum corresponding to journal format V34 */
	REPL_JNL_V35,		/* enum corresponding to journal format V35 */
	REPL_JNL_V36,		/* enum corresponding to journal format V36 */
	REPL_JNL_V37,		/* enum corresponding to journal format V37 */
	REPL_JNL_V38,		/* enum corresponding to journal format V38 */
	REPL_JNL_V39,		/* enum corresponding to journal format V39 */
	REPL_JNL_V40,		/* enum corresponding to journal format V40 */
	REPL_JNL_V41,		/* enum corresponding to journal format V41 */
	REPL_JNL_V42,		/* enum corresponding to journal format V42 */
	REPL_JNL_V43,		/* enum corresponding to journal format V43 */
	REPL_JNL_V44,		/* enum corresponding to journal format V44 */
	REPL_JNL_MAX
} repl_jnl_t;

#define IF_INVALID	((intlfltr_t)0L)
#define IF_NONE		((intlfltr_t)(-1L))
#define IF_22TO44	(intlfltr_t)jnl_v22TOv44
#define IF_44TO22	(intlfltr_t)jnl_v44TOv22
#define IF_24TO44	(intlfltr_t)jnl_v24TOv44
#define IF_44TO24	(intlfltr_t)jnl_v44TOv24
#define IF_44TO44	(intlfltr_t)jnl_v44TOv44

extern int jnl_v44TOv22(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v22TOv44(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v44TOv24(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v24TOv44(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v44TOv44(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);

extern void repl_check_jnlver_compat(boolean_t same_endianness);

GBLREF	intlfltr_t repl_filter_old2cur[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1];
GBLREF	intlfltr_t repl_filter_cur2old[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1];

/*
 * Writing a filter :
 *------------------
 *
 * jnl_buff : input parameter, jnl transaction
 * jnl_len : input/output parameter, input - len of the jnl transaction, output - offset where processing was stopped
 * conv_buff : output parameter, the converted jnl transaction
 * conv_len : output parameter, the length of the converted jnl transaction
 * conv_bufsiz : input parameter, size of the output buffer.
 *
 * Return values : SS_NORMAL if the entire jnl_buff is converted.
 * 		   -1 if some error occurred (sets repl_errno)
 * repl_errno values in case of error :
 * 	EREPL_INTLFILTER_NOSPC - no space in the filter buffer.
 * 	EREPL_INTLFILTER_BADREC - bad record in the filter buffer.
 * 	EREPL_INTLFILTER_INCMPLREC - incomplete record in the filter buffer.
 *	EREPL_INTLFILTER_NEWREC - cannot convert record, the record is newer than the "to" version.
 *	EREPL_INTLFILTER_SECLESSTHANV62 - record contains #t global which is not allowed when source side is > V62000
 *	EREPL_INTLFILTER_PRILESSTHANV62 - record contains #t global which is not allowed when receiver side is > V62000
 *	EREPL_INTLFILTER_NOCONVERT - conversion error
 * In all error cases, conv_len will be the offset at which processing was stopped.
 */


#define TP_TOKEN_TID_SIZE		(SIZEOF(token_num) + 8)
#define	TOKEN_PARTICIPANTS_TS_SHORT_TIME_SIZE (SIZEOF(token_num) + 2 * SIZEOF(uint4))

#define V22_JNL_VER		22
#define V23_JNL_VER		23
#define V24_JNL_VER		24
#define V25_JNL_VER		25
#define V26_JNL_VER		26
#define V27_JNL_VER		27
#define V44_JNL_VER		44

#define	V44_NULL_RECLEN		48	/* size of a JRT_NULL record in V22/V23/V24/V44 jnl format */

#define	V44_MUMPS_NODE_OFFSET		48	/* offset of "mumps_node" member in struct_jrec_upd struct in V44 jnl format */

int repl_filter_init(char *filter_cmd);
int repl_filter(seq_num tr_num, unsigned char **tr, int *tr_len, int *tr_bufsize);
STATICFNDCL int repl_filter_recv(seq_num tr_num, unsigned char **tr, int *tr_len, int *tr_bufsize, boolean_t send_done);
STATICFNDCL int repl_filter_recv_line(char *line, int *line_len, int max_line_len, boolean_t send_done);
int repl_stop_filter(void);
void repl_filter_error(seq_num filter_seqno, int why);

#define	LOCAL_JNL_VER		this_side->jnl_ver
#define	LOCAL_TRIGGER_SUPPORT	this_side->trigger_supported
#define	REMOTE_JNL_VER		remote_side->jnl_ver
#define	REMOTE_TRIGGER_SUPPORT	remote_side->trigger_supported
#define	REMOTE_NULL_SUBS_XFORM	remote_side->null_subs_xform
#define	REMOTE_IS_CROSS_ENDIAN	remote_side->cross_endian

/* Helper macros for internal and external filters */
#define APPLY_EXT_FILTER_IF_NEEDED(GTMSOURCE_FILTER, GTMSOURCE_MSGP, DATA_LEN, TOT_TR_LEN)					\
{																\
	seq_num		filter_seqno;												\
	unsigned char	*tr;													\
																\
	if (GTMSOURCE_FILTER & EXTERNAL_FILTER)											\
	{															\
		assert(TOT_TR_LEN == DATA_LEN + REPL_MSG_HDRLEN);								\
		/* jnl2ext (invoked before sending the records to the external filter) combines multi-region TP transaction	\
		 * into a single region TP transaction. By doing so, the update_num (stored as part of the update records)	\
		 * will no longer be sorted within the single region. This property (update num within a region SHOULD always	\
		 * be sorted) is relied upon by the receiver server. To maintain this property, sort the journal records	\
		 * according to the update_num. V19 is the first journal filter format which introduced the notion of		\
		 * update_num.													\
		 */														\
		tr = GTMSOURCE_MSGP->msg;											\
		repl_sort_tr_buff(tr, DATA_LEN);										\
		DBG_VERIFY_TR_BUFF_SORTED(tr, DATA_LEN);									\
		filter_seqno = ((struct_jrec_null *)tr)->jnl_seqno;								\
		if (SS_NORMAL != (status = repl_filter(filter_seqno, &tr, &DATA_LEN, &gtmsource_msgbufsiz)))			\
			repl_filter_error(filter_seqno, status);								\
		TOT_TR_LEN = DATA_LEN + REPL_MSG_HDRLEN;									\
	}															\
}

#define APPLY_INT_FILTER(IN_BUFF, IN_BUFLEN, OUT_BUFF, OUT_BUFLEN, OUT_BUFSIZ, STATUS)						\
{																\
	STATUS = repl_filter_cur2old[REMOTE_JNL_VER - JNL_VER_EARLIEST_REPL](IN_BUFF, &IN_BUFLEN, OUT_BUFF, &OUT_BUFLEN,	\
										OUT_BUFSIZ);					\
}

#define REALLOCATE_INT_FILTER_BUFF(OUT_BUFF, OUT_BUFFMSG, OUT_BUFSIZ)				\
{												\
	uint4		converted_len;								\
												\
	converted_len = (OUT_BUFFMSG - repl_filter_buff);					\
	gtmsource_alloc_filter_buff(repl_filter_bufsiz + (repl_filter_bufsiz >> 1));		\
	assert(converted_len < repl_filter_bufsiz);						\
	OUT_BUFFMSG = repl_filter_buff + converted_len;						\
	OUT_BUFF = OUT_BUFFMSG + REPL_MSG_HDRLEN;						\
	OUT_BUFSIZ = (uint4)(repl_filter_bufsiz - (converted_len + REPL_MSG_HDRLEN));		\
	assert(0 < OUT_BUFSIZ);									\
}

/* Below error_defs are needed by the following macros */
error_def(ERR_REPLRECFMT);
error_def(ERR_REPLNOHASHTREC);

# define INT_FILTER_RTS_ERROR(FILTER_SEQNO, REPL_ERRNO)										\
{																\
	assert((EREPL_INTLFILTER_BADREC == REPL_ERRNO)										\
		|| (EREPL_INTLFILTER_PRILESSTHANV62 == REPL_ERRNO)								\
		|| (EREPL_INTLFILTER_SECLESSTHANV62 == REPL_ERRNO));								\
	if (EREPL_INTLFILTER_BADREC == REPL_ERRNO)										\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REPLRECFMT);							\
	else if (EREPL_INTLFILTER_PRILESSTHANV62 == REPL_ERRNO)									\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLNOHASHTREC, 3, &FILTER_SEQNO, LEN_AND_LIT("Source"));		\
	else if (EREPL_INTLFILTER_SECLESSTHANV62 == REPL_ERRNO)									\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLNOHASHTREC, 3, &FILTER_SEQNO, LEN_AND_LIT("Receiver"));	\
	else	/* (EREPL_INTLFILTER_INCMPLREC == REPL_ERRNO) || (EREPL_INTLFILTER_NOCONVERT == REPL_ERRNO) */			\
		assertpro(FALSE);												\
}

/* This macro is called by the source or receiver server when they need to stop an external filter.
 * CALLER_FILTER : is gtmsource_filter (if caller is source server) and gtmrecv_filter (if caller is receiver server)
 * CALLER_FP     : is gtmsource_log_fp (if caller is source server) and gtmrecv_log_fp (if caller is receiver server)
 * CALLER_DETAIL : is string that provides caller context and is printed in the corresponding log file.
 */
#define STOP_EXTERNAL_FILTER_IF_NEEDED(CALLER_FILTER, CALLER_FP, CALLER_DETAIL)			\
MBSTART {											\
	if (CALLER_FILTER & EXTERNAL_FILTER)							\
	{											\
		repl_log(CALLER_FP, TRUE, TRUE, "Stopping filter : " CALLER_DETAIL "\n");	\
		repl_stop_filter();								\
		CALLER_FILTER &= ~EXTERNAL_FILTER;						\
	}											\
} MBEND

#endif
