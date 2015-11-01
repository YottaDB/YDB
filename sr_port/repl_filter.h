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

typedef int (*intlfltr_t)(uchar_ptr_t, uint4 *, uchar_ptr_t, uint4 *, uint4);

#define IF_INVALID	((intlfltr_t)0)
#define IF_NONE		((intlfltr_t)(-1))
#define IF_11TO12	(intlfltr_t)jnl_v11tov12
#define IF_12TO11	(intlfltr_t)jnl_v12tov11
#define IF_11TO15	(intlfltr_t)jnl_v11tov15
#define IF_12TO15	(intlfltr_t)jnl_v12tov15
#define IF_15TO11	(intlfltr_t)jnl_v15tov11
#define IF_15TO12	(intlfltr_t)jnl_v15tov12

extern int jnl_v11tov12(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v12tov11(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v11tov15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v12tov15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v15tov11(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v15tov12(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
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
 * In all error cases, conv_len will be the offset at which processing was stopped.
 */

#define V11_JNL_VER			'\013'
#define V11_JREC_TYPE_OFFSET		0
#define V11_TCOM_TOKEN_OFFSET		24
#define V11_JNL_SEQNO_OFFSET		(2 * sizeof(uint4) + sizeof(trans_num) + sizeof(int4))
#define V11_REC_SEQNO_OFFSET		(2 * sizeof(uint4) + sizeof(trans_num))
#define V11_TCOM_PARTICIPANTS_OFFSET	32
#define V11_TCOM_TS_SHORT_TIME_OFFSET	36
#define V11_JNL_REC_START_BNDRY		8
#define V11_JRT_RECTYPES		27
#define V11_JREC_PREFIX_SIZE		8
#define V11_JREC_SUFFIX_SIZE		8
#define V11_MUMPS_NODE_OFFSET	 	(2 * sizeof(uint4) + sizeof(trans_num) + sizeof(int4) + sizeof(seq_num))
#define V11_TP_SET_KILL_TOKEN_OFFSET	V11_MUMPS_NODE_OFFSET

#define V12_JNL_VER			'\014'
#define V12_JRT_RECTYPES		27
#define V12_JREC_PREFIX_SIZE		8
#define V12_JREC_SUFFIX_SIZE		8
#define V12_JNL_REC_START_BNDRY		8
#define V12_JREC_TYPE_OFFSET		0
#define V12_MUMPS_NODE_OFFSET		(V11_MUMPS_NODE_OFFSET + 2 * sizeof(uint4))
#define V12_TCOM_TOKEN_OFFSET		(V12_MUMPS_NODE_OFFSET)
#define V12_TCOM_PARTICIPANTS_OFFSET	(V12_TCOM_TOKEN_OFFSET + sizeof(token_num))
#define V12_JNL_SEQNO_OFFSET		V11_JNL_SEQNO_OFFSET
#define V12_TP_SET_KILL_TOKEN_OFFSET	(4 * sizeof(uint4) + sizeof(trans_num) + sizeof(int4) + sizeof(seq_num))
#define V12_REC_SEQNO_OFFSET		V11_REC_SEQNO_OFFSET

#define TP_TOKEN_TID_SIZE		(sizeof(token_num) + 8)
#define	TOKEN_PARTICIPANTS_TS_SHORT_TIME_SIZE (sizeof(token_num) + 2 * sizeof(uint4))

#define V13_JNL_VER			'\015'

#define V15_JNL_SEQNO_OFFSET 		sizeof(jrec_prefix)
#define V15_TCOM_PARTICIPANTS_OFFSET 	(V15_JNL_SEQNO_OFFSET + sizeof(token_num) + 8)
#define V15_ZTCOM_PARTICIPANTS_OFFSET 	(V15_JNL_SEQNO_OFFSET + sizeof(token_num))
#define PRE_V15_JNL_REC_TRAILER		0xFE

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
