/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

#define MAX_EXTRACT_BUFSIZ		1 * 1024 * 1024

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
 *	V12	V12	GT.M V4.3-000	<--- filter support starts here (in Unix)
 *	V12	V13	GT.M V4.3-001	<--- filter support starts here (in VMS )
 *	V12	V14	GT.M V4.3-001A
 *	V15	V15	GT.M V4.4-002
 *	V16	V16	GT.M V5.0-FT01
 *	V17	V17	GT.M V5.0-000
 *	V17	V18	GT.M V5.3-003	(EPOCH record format changed so no filter format change)
 */

typedef enum
{
	REPL_FILTER_V12,
	REPL_FILTER_V15,
	REPL_FILTER_V16,
	REPL_FILTER_V17,
	REPL_FILTER_MAX
} repl_filter_t;

typedef enum
{
	REPL_JNL_V12,
	REPL_JNL_V13,
	REPL_JNL_V14,
	REPL_JNL_V15,
	REPL_JNL_V16,
	REPL_JNL_V17,
	REPL_JNL_V18,
	REPL_JNL_MAX
} repl_jnl_t;

GBLREF	int	jnl2filterfmt[];	/* Add row to this array in repl_filter.c whenever new REPL_JNL_Vnn gets added */

#define IF_INVALID	((intlfltr_t)0L)
#define IF_NONE		((intlfltr_t)(-1L))
#define IF_12TO15	(intlfltr_t)jnl_v12tov15
#define IF_15TO12	(intlfltr_t)jnl_v15tov12
#define IF_16TO12	(intlfltr_t)jnl_v16tov12
#define IF_16TO15	(intlfltr_t)jnl_v16tov15
#define IF_15TO16	(intlfltr_t)jnl_v15tov16
#define IF_12TO16	(intlfltr_t)jnl_v12tov16
#define IF_16TO16	(intlfltr_t)jnl_v16tov16
#define IF_17TO12	(intlfltr_t)jnl_v17tov12
#define IF_17TO15	(intlfltr_t)jnl_v17tov15
#define IF_15TO17	(intlfltr_t)jnl_v15tov17
#define IF_12TO17	(intlfltr_t)jnl_v12tov17
#define IF_17TO17	(intlfltr_t)jnl_v17tov17

extern int jnl_v12tov15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v15tov12(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v16tov12(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v16tov15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v15tov16(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v12tov16(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v16tov16(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v17tov12(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v17tov15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v15tov17(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v12tov17(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v17tov17(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern void repl_check_jnlver_compat(void);
GBLREF intlfltr_t repl_internal_filter[JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1][JNL_VER_THIS - JNL_VER_EARLIEST_REPL + 1];

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


#define V12_JNL_VER			'\014' /* V4.3-000*/
#define V12_JRT_RECTYPES		27
#define V12_JREC_PREFIX_SIZE		8
#define V12_JREC_SUFFIX_SIZE		8
#define V12_JNL_REC_START_BNDRY		8
#define V12_JREC_TYPE_OFFSET		0
#define V12_MUMPS_NODE_OFFSET	(2 * sizeof(uint4) + sizeof(trans_num_4byte) + sizeof(int4) + sizeof(seq_num) + 2 * sizeof(uint4))
#define V12_TCOM_TOKEN_OFFSET		(V12_MUMPS_NODE_OFFSET)
#define V12_TCOM_PARTICIPANTS_OFFSET	(V12_TCOM_TOKEN_OFFSET + sizeof(token_num))
#define V12_JNL_SEQNO_OFFSET		(2 * sizeof(uint4) + sizeof(trans_num_4byte) + sizeof(int4))
#define V12_TP_SET_KILL_TOKEN_OFFSET	(4 * sizeof(uint4) + sizeof(trans_num_4byte) + sizeof(int4) + sizeof(seq_num))
#define V12_REC_SEQNO_OFFSET		(2 * sizeof(uint4) + sizeof(trans_num_4byte))

#define TP_TOKEN_TID_SIZE		(sizeof(token_num) + 8)
#define	TOKEN_PARTICIPANTS_TS_SHORT_TIME_SIZE (sizeof(token_num) + 2 * sizeof(uint4))

#define V13_JNL_VER			'\015'

#define V15_JNL_VER			'\017'
#define V15_JNL_LABEL_TEXT		"GDSJNL15"
#define V15_JNL_VER_EARLIEST_REPL	'\013' /* from GDSJNL11 (V4.2-002), octal equivalent of decimal 11 */
#define V15_JNL_SEQNO_OFFSET 		(sizeof(jrec_prefix) - 2 * sizeof(uint4))
#define V15_TCOM_PARTICIPANTS_OFFSET 	(V15_JNL_SEQNO_OFFSET + sizeof(token_num) + 8)
#define V15_ZTCOM_PARTICIPANTS_OFFSET 	(V15_JNL_SEQNO_OFFSET + sizeof(token_num))
#define PRE_V15_JNL_REC_TRAILER		0xFE
#define V16_JNL_SEQNO_OFFSET 		V15_JNL_SEQNO_OFFSET
#define V16_TCOM_PARTICIPANTS_OFFSET 	V15_TCOM_PARTICIPANTS_OFFSET
#define V16_ZTCOM_PARTICIPANTS_OFFSET 	V15_ZTCOM_PARTICIPANTS_OFFSET
#define V17_JNL_SEQNO_OFFSET 		sizeof(jrec_prefix)
#define V17_TCOM_PARTICIPANTS_OFFSET 	(V17_JNL_SEQNO_OFFSET + sizeof(token_num) + 8)
#define V17_ZTCOM_PARTICIPANTS_OFFSET 	(V17_JNL_SEQNO_OFFSET + sizeof(token_num))

typedef struct
{
	uint4			jrec_type : 8;		/* Actually, enum jnl_record_type */
	uint4			forwptr : 24;		/* Offset to beginning of next record */
	off_jnl_t		pini_addr;		/* Offset in the journal file which contains pini record */
	jnl_tm_t		time;			/* 4-byte time stamp both for UNIX and VMS */
	trans_num_4byte		tn;
} v15_jrec_prefix;	/* 16-byte */
typedef struct
{
	int4			filler_suffix;
	unsigned int		backptr : 24;
	unsigned int		suffix_code : 8;
} pre_v15_jrec_suffix;

int repl_filter_init(char *filter_cmd);
int repl_filter(seq_num tr_num, unsigned char *tr, int *tr_len, int bufsize);
int repl_stop_filter(void);
void repl_filter_error(seq_num filter_seqno, int why);

#endif
