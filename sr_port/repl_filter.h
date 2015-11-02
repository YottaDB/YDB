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
 *	V12	V12	GT.M V4.3-000
 *	V12	V13	GT.M V4.3-001
 *	V12	V14	GT.M V4.3-001A
 *	V15	V15	GT.M V4.4-002	<--- filter support starts here
 *	V16	V16	GT.M V5.0-FT01
 *	V17	V17	GT.M V5.0-000
 *	V17	V18	GT.M V5.3-003	(EPOCH record format changed so no filter format change)
 *	V19	V19	GT.M V5.4-000	(SET/KILL records have nodeflags, New ZTWORMHOLE record, file header max_jrec_len changes)
 *	V19	V20	GT.M V5.4-001	64K journal file header change in Unix but V20 change for VMS too; No jnlrec format change
 */

typedef enum
{
	REPL_FILTER_VNONE = 0,
	REPL_FILTER_V15,	/* filter version 1 corresponds to journal format V15 */
	REPL_FILTER_V17,	/* filter version 2 corresponds to journal format V17 */
	REPL_FILTER_V19,	/* filter version 3 corresponds to journal format V19 */
	REPL_FILTER_MAX
} repl_filter_t;

typedef enum
{
	REPL_JNL_V15,
	REPL_JNL_V16,
	REPL_JNL_V17,
	REPL_JNL_V18,
	REPL_JNL_V19,
	REPL_JNL_V20,
	REPL_JNL_MAX
} repl_jnl_t;

GBLREF	int	jnl2filterfmt[];	/* Add row to this array in repl_filter.c whenever new REPL_JNL_Vnn gets added */

#define IF_INVALID	((intlfltr_t)0L)
#define IF_NONE		((intlfltr_t)(-1L))
#define IF_19TO15	(intlfltr_t)jnl_v19TOv15
#define IF_19TO17	(intlfltr_t)jnl_v19TOv17
#define IF_15TO19	(intlfltr_t)jnl_v15TOv19
#define IF_17TO19	(intlfltr_t)jnl_v17TOv19
#define IF_19TO19	(intlfltr_t)jnl_v19TOv19

extern int jnl_v19TOv15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v15TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v19TOv17(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v17TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);
extern int jnl_v19TOv19(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz);

extern void repl_check_jnlver_compat(void);

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

#define V15_JNL_VER			15
#define V18_JNL_VER			18
#define V19_JNL_VER			19
#define V20_JNL_VER			20

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

#endif
