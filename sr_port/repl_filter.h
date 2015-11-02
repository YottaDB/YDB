/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

#ifdef UNIX
#  define MAX_EXTRACT_BUFSIZ		10 * MAX_LOGI_JNL_REC_SIZE	/* Since we might need up to 9X + 11
									 * of MAX_LOGI_JNL_REC_SIZE */
#else
#  define MAX_EXTRACT_BUFSIZ		1 * 1024 * 1024
#endif

#define NO_FILTER			0
#define INTERNAL_FILTER			0x00000001
#define EXTERNAL_FILTER			0x00000002
#define JNL_EXTRACT_DELIM		'\\'
#define TCOM_EXTR_JNLSEQNO_FIELDNUM	4
#define FILTER_EOT			"99\n"

#define	GTMNULL_TO_STDNULL_COLL		1
#define	STDNULL_TO_GTMNULL_COLL		2

typedef int (*intlfltr_t)(uchar_ptr_t, uint4 *, uchar_ptr_t, uint4 *, uint4);

/* The following is the list of filter-format version number versus the earliest GT.M version number that used it.
 * Whenever the filter-format version number changes, it is set to the journal-format version number (JNL_LABEL_TEXT macro)
 * for that version. Although usually a change in the journal file format (i.e. change to JNL_LABEL_TEXT macro in jnl.h)
 * implies a change in the filter format, it is not always the case. This is because the filter format changes only when
 * the format of any REPLICATED jnl record changes as opposed to the jnl format which can change if the jnl file hdr format
 * changes or if the format of a non-replicated journal record (e.g. EPOCH record) changes.
 *
 *	------	-------	--------
 *	Filter	Journal	GT.M
 *	format	format	version
 *	------	-------	--------
 *	V11	V11	GT.M V4.2-000
 *	V12	V12	GT.M V4.3-000
 *	V12	V13	GT.M V4.3-001
 *	V12	V14	GT.M V4.3-001A
 *	V15	V15	GT.M V4.4-002
 *	V16	V16	GT.M V5.0-FT01
 *	V17	V17	GT.M V5.0-000	<--- filter support starts here
 *	V17	V18	GT.M V5.3-003	(EPOCH record format changed so no filter format change)
 *	V19	V19	GT.M V5.4-000	(SET/KILL records have nodeflags, New ZTWORMHOLE record, file header max_jrec_len changes)
 *	V19	V20	GT.M V5.4-001	64K journal file header change in Unix but V20 change for VMS too; No jnlrec format change
 *	V21	V21	GT.M V5.4-002	Added replicated ZTRIGGER jnl record type
 *	V22	V22	GT.M V5.5-000	strm_seqno added to all logical records (supplementary instances)
 *	V22	V23	GT.M V6.0-000	Various journaling-related limits have changed, allowing for much larger journal records
 */

typedef enum
{
	REPL_FILTER_VNONE = 0,
	REPL_FILTER_V17,	/* filter version corresponding to journal format V17 */
	REPL_FILTER_V19,	/* filter version corresponding to journal format V19 */
	REPL_FILTER_V21,	/* filter version corresponding to journal format V21 */
	REPL_FILTER_V22,	/* filter version corresponding to journal format V22 */
	REPL_FILTER_MAX
} repl_filter_t;

typedef enum
{
	REPL_JNL_V17,		/* enum corresponding to journal format V17 */
	REPL_JNL_V18,		/* enum corresponding to journal format V18 */
	REPL_JNL_V19,		/* enum corresponding to journal format V19 */
	REPL_JNL_V20,		/* enum corresponding to journal format V20 */
	REPL_JNL_V21,		/* enum corresponding to journal format V21 */
	REPL_JNL_V22,		/* enum corresponding to journal format V22 */
	REPL_JNL_V23,		/* enum corresponding to journal format V23 */
	REPL_JNL_MAX
} repl_jnl_t;

#define IF_INVALID	((intlfltr_t)0L)
#define IF_NONE		((intlfltr_t)(-1L))
#define IF_22TO17	(intlfltr_t)jnl_v22TOv17
#define IF_22TO19	(intlfltr_t)jnl_v22TOv19
#define IF_22TO21	(intlfltr_t)jnl_v22TOv21
#define IF_17TO22	(intlfltr_t)jnl_v17TOv22
#define IF_19TO22	(intlfltr_t)jnl_v19TOv22
#define IF_21TO22	(intlfltr_t)jnl_v21TOv22
#define IF_22TO22	(intlfltr_t)jnl_v22TOv22

extern int jnl_v22TOv17(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v17TOv22(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v22TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v19TOv22(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v22TOv21(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v21TOv22(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v22TOv22(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);

extern void repl_check_jnlver_compat(UNIX_ONLY(boolean_t same_endianness));

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
 *	EREPL_INTLFILTER_REPLGBL2LONG - record contains global name > 8 characters, which is not supported in remote side
 * In all error cases, conv_len will be the offset at which processing was stopped.
 */


#define TP_TOKEN_TID_SIZE		(SIZEOF(token_num) + 8)
#define	TOKEN_PARTICIPANTS_TS_SHORT_TIME_SIZE (SIZEOF(token_num) + 2 * SIZEOF(uint4))

#define V15_JNL_VER		15
#define V17_JNL_VER		17
#define V18_JNL_VER		18
#define V19_JNL_VER		19
#define V20_JNL_VER		20
#define V21_JNL_VER		21
#define V22_JNL_VER		22
#define V23_JNL_VER		23

#define	V17_NULL_RECLEN		40	/* size of a JRT_NULL record in V17/V18 jnl format */
#define	V19_NULL_RECLEN		40	/* size of a JRT_NULL record in V19/V20 jnl format */
#define	V21_NULL_RECLEN		40	/* size of a JRT_NULL record in V21	jnl format */
#define	V22_NULL_RECLEN		48	/* size of a JRT_NULL record in V22/V23	jnl format */

#define	V19_UPDATE_NUM_OFFSET		32	/* offset of "update_num" member in struct_jrec_upd structure in V19 jnl format */
#define	V19_MUMPS_NODE_OFFSET		40	/* offset of "mumps_node" member in struct_jrec_upd structure in V19 jnl format */
#define	V19_TCOM_FILLER_SHORT_OFFSET	32	/* offset of "filler_short" in struct_jrec_tcom structure in V19 jnl format */
#define	V19_NULL_FILLER_OFFSET		32	/* offset of "filler" in struct_jrec_nullstructure in V19 jnl format */

#define	V22_MUMPS_NODE_OFFSET		48	/* offset of "mumps_node" member in struct_jrec_upd struct in V22/V23 jnl format */

typedef struct
{
	uint4			jrec_type : 8;		/* Actually, enum jnl_record_type */
	uint4			forwptr : 24;		/* Offset to beginning of next record */
	off_jnl_t		pini_addr;		/* Offset in the journal file which contains pini record */
	jnl_tm_t		time;			/* 4-byte time stamp both for UNIX and VMS */
	trans_num_4byte		tn;
} v15_jrec_prefix;	/* 16-byte */

int repl_filter_init(char *filter_cmd);
int repl_filter(seq_num tr_num, unsigned char *tr, int *tr_len, int bufsize);
int repl_stop_filter(void);
void repl_filter_error(seq_num filter_seqno, int why);

# ifdef UNIX
#	define	LOCAL_JNL_VER		this_side->jnl_ver
#	define	LOCAL_TRIGGER_SUPPORT	this_side->trigger_supported
#	define	REMOTE_JNL_VER		remote_side->jnl_ver
#	define	REMOTE_TRIGGER_SUPPORT	remote_side->trigger_supported
#	define	REMOTE_NULL_SUBS_XFORM	remote_side->null_subs_xform
#	define	REMOTE_IS_CROSS_ENDIAN	remote_side->cross_endian
# else
	GBLREF	unsigned char		jnl_ver, remote_jnl_ver;
	GBLREF	boolean_t		secondary_side_trigger_support;
#	define	LOCAL_JNL_VER		jnl_ver
#	define	LOCAL_TRIGGER_SUPPORT	secondary_side_trigger_support
#	define	REMOTE_JNL_VER		remote_jnl_ver
#	define	REMOTE_TRIGGER_SUPPORT	secondary_side_trigger_support
#	define	REMOTE_NULL_SUBS_XFORM	(TREF(replgbl)).null_subs_xform
#	define	REMOTE_IS_CROSS_ENDIAN	FALSE
# endif

/* Helper macros for internal and external filters */
#define APPLY_EXT_FILTER_IF_NEEDED(GTMSOURCE_FILTER, GTMSOURCE_MSGP, DATA_LEN, TOT_TR_LEN)					\
{																\
	seq_num		filter_seqno;												\
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
		if (V19_JNL_VER <= LOCAL_JNL_VER)										\
		{														\
			repl_sort_tr_buff(GTMSOURCE_MSGP->msg, DATA_LEN);							\
			DBG_VERIFY_TR_BUFF_SORTED(GTMSOURCE_MSGP->msg, DATA_LEN);						\
		}														\
		filter_seqno = ((struct_jrec_null *)(GTMSOURCE_MSGP->msg))->jnl_seqno;						\
		if (SS_NORMAL != (status = repl_filter(filter_seqno, GTMSOURCE_MSGP->msg, &DATA_LEN, gtmsource_msgbufsiz)))	\
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

#ifdef UNIX
# define INT_FILTER_RTS_ERROR(FILTER_SEQNO)							\
{												\
	if (EREPL_INTLFILTER_BADREC == repl_errno)						\
		rts_error(VARLSTCNT(1) ERR_REPLRECFMT);						\
	else if (EREPL_INTLFILTER_REPLGBL2LONG == repl_errno)					\
		rts_error(VARLSTCNT(1) ERR_REPLGBL2LONG);					\
	else if (EREPL_INTLFILTER_SECNODZTRIGINTP == repl_errno)				\
		rts_error(VARLSTCNT(3) ERR_SECNODZTRIGINTP, 1, &FILTER_SEQNO);			\
	else if (EREPL_INTLFILTER_MULTILINEXECUTE == repl_errno)				\
		rts_error(VARLSTCNT(3) ERR_REPLNOMULTILINETRG, 1, &FILTER_SEQNO);		\
	else	/* (EREPL_INTLFILTER_INCMPLREC == repl_errno) */				\
		assertpro(FALSE);								\
}
#else
# define INT_FILTER_RTS_ERROR(FILTER_SEQNO)							\
{												\
	if (EREPL_INTLFILTER_BADREC == repl_errno)						\
		rts_error(VARLSTCNT(1) ERR_REPLRECFMT);						\
	else if (EREPL_INTLFILTER_REPLGBL2LONG == repl_errno)					\
		rts_error(VARLSTCNT(1) ERR_REPLGBL2LONG);					\
	else	/* (EREPL_INTLFILTER_INCMPLREC == repl_errno) */				\
		assertpro(FALSE);								\
}
#endif
#endif
