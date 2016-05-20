/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h> /* for offsetof() macro */
#ifdef VMS
#include <descrip.h>		/* required for gtmsource.h */
#endif

#include "gtm_ctype.h"

#include "mlkdef.h"
#include "gtm_string.h"
#include "subscript.h"
#include "gdsroot.h"		/* for filestruct.h */
#include "gdsbt.h"		/* for gdsfhead.h */
#include "gtm_facility.h"	/* for fileinfo.h */
#include "fileinfo.h"		/* for gdsfhead.h */
#include "gdsfhead.h"		/* for filestruct.h */
#include "filestruct.h"		/* for jnl.h */
#include "jnl.h"
#include "repl_dbg.h"
#include "copy.h"
#include "zshow.h"
#include "mvalconv.h"
#include "str2gvkey.h"
#include "repl_filter.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gdsblk.h"

GBLREF	char		*ext_stop;
GBLREF	gv_key		*gv_currkey;
GBLREF	boolean_t	is_src_server;
GBLREF	repl_msg_ptr_t	gtmsource_msgp;
GBLREF	int		gtmsource_msgbufsiz;
GBLREF	volatile boolean_t	timer_in_handler;

static	boolean_t	in_tp;
static	int4		num_records;

/* callers please set up the proper condition-handlers */
/* expects a null-terminated ext_buff. does the equivalent but inverse of jnl2ext */

unsigned char *ext2jnlcvt(char *ext_buff, int4 ext_len, unsigned char **tr, int *tr_bufsiz,
						seq_num saved_jnl_seqno, seq_num saved_strm_seqno)
{
	char		*ext_next;
	unsigned char	*rec, *rectop, *tmp, *origbuf, *temp_rec;
	int		tmpbufsiz, tmpsize;

	rec = *tr;
	rectop = rec + *tr_bufsiz;
	temp_rec = rec;
	for ( ; (NULL != (ext_next = strchr(ext_buff, '\n'))); )
	{
		*ext_next++ = '\0';
		if (MAX_JNL_REC_SIZE > (rectop - rec))
		{	/* Remaining space not enough to hold ONE max-sized jnl record. Expand linearly */
			tmpsize = *tr_bufsiz;
			tmpbufsiz = tmpsize + (EXT2JNLCVT_EXPAND_FACTOR * MAX_JNL_REC_SIZE);
			origbuf = *tr;
			tmpsize = rec - origbuf;
			if (is_src_server)
			{	/* In the case of the source server, the pointer "tr" passed in is actually
				 * 8 bytes after gtmsource_msgp and so the malloc/realloc needs to happen
				 * 8 bytes before. Also compression buffers need to be reallocated so hardcode
				 * all of this even though it is violation of information hiding in this generic
				 * routine. The alternative is to return an out-of-space status and bubble it up
				 * through all the callers until the caller of "repl_filter" and do the reallocation
				 * there and reinvoke through the same caller graph to come back here and resume
				 * operation. That is tricky and not considered worth the effort since there are only
				 * two callers of this function (one through source server and one through the receiver
				 * server). Hence this choice.
				 */
				assert((unsigned char *)&gtmsource_msgp->msg[0] == *tr);
				assert(*tr_bufsiz == gtmsource_msgbufsiz);
				UNIX_ONLY(gtmsource_alloc_msgbuff(tmpbufsiz, FALSE);)
				VMS_ONLY(gtmsource_alloc_msgbuff(tmpbufsiz);)
				*tr_bufsiz = gtmsource_msgbufsiz;
				*tr = &gtmsource_msgp->msg[0];
				rec = *tr + tmpsize;
				rectop = (unsigned char *)gtmsource_msgp + gtmsource_msgbufsiz;
			} else
			{
				tmp = malloc(tmpbufsiz);
				memcpy(tmp, origbuf, tmpsize);
				free(origbuf);
				*tr = tmp;
				*tr_bufsiz = tmpbufsiz;
				rec = tmp + tmpsize;
				rectop = tmp + tmpbufsiz;
			}
		}
		rec = (unsigned char *)ext2jnl(ext_buff, (jnl_record *)rec, saved_jnl_seqno, saved_strm_seqno);
		assert(0 == (INTPTR_T)rec % JNL_REC_START_BNDRY);
		if (ext_stop == ext_buff)
			break;
		ext_buff = ext_next;
	}
	assertpro(rec != temp_rec);
	ext_stop = ext_buff;
	return rec;
}

/* expects a single null-terminated ptr (equivalent to one line in the journal extract file) */
char	*ext2jnl(char *ptr, jnl_record *rec, seq_num saved_jnl_seqno, seq_num saved_strm_seqno)
{
	unsigned char	*pool_save, ch, chtmp;
	char 		*val_off, *strtokptr;
	int		keylength, keystate, len, i, reclen, temp_reclen, val_len;
	bool		keepgoing;
	mstr		src, des;
	muextract_type	exttype;
	enum jnl_record_type	rectype;
	jrec_suffix	*suffix;
	uint4		nodeflags;
	DEBUG_ONLY(uint4	tcom_num = 0;)

	ext_stop = ptr + strlen(ptr) + 1;

	exttype = (muextract_type)MUEXTRACT_TYPE(ptr);
	assert((exttype >= 0) && (exttype < MUEXT_MAX_TYPES));

	switch(exttype)
	{
	case MUEXT_SET:
		if (in_tp)
		{
			if (0 == num_records)
				rec->prefix.jrec_type = JRT_TSET;
			else
				rec->prefix.jrec_type = JRT_USET;
			num_records++;
		} else
			rec->prefix.jrec_type = JRT_SET;
		break;

	case MUEXT_KILL:
		if (in_tp)
		{
			if (0 == num_records)
				rec->prefix.jrec_type = JRT_TKILL;
			else
				rec->prefix.jrec_type = JRT_UKILL;
			num_records++;
		} else
			rec->prefix.jrec_type = JRT_KILL;
		break;

	case MUEXT_ZKILL:
		if (in_tp)
		{
			if (0 == num_records)
				rec->prefix.jrec_type = JRT_TZKILL;
			else
				rec->prefix.jrec_type = JRT_UZKILL;
			num_records++;
		} else
			rec->prefix.jrec_type = JRT_ZKILL;
		break;

#	ifdef GTM_TRIGGER
	case MUEXT_ZTWORM:
		assertpro(in_tp);
		if (0 == num_records)
			rec->prefix.jrec_type = JRT_TZTWORM;
		else
			rec->prefix.jrec_type = JRT_UZTWORM;
		num_records++;
		break;
	case MUEXT_LGTRIG:
		assertpro(in_tp);
		if (0 == num_records)
			rec->prefix.jrec_type = JRT_TLGTRIG;
		else
			rec->prefix.jrec_type = JRT_ULGTRIG;
		num_records++;
		break;
	case MUEXT_ZTRIG:
		assertpro(in_tp);
		if (0 == num_records)
			rec->prefix.jrec_type = JRT_TZTRIG;
		else
			rec->prefix.jrec_type = JRT_UZTRIG;
		num_records++;
		break;
#	endif

	case MUEXT_TSTART:
		in_tp = TRUE;
		num_records = 0;
		return (char *)rec;
		break;

	case MUEXT_TCOMMIT:
		rec->prefix.jrec_type = JRT_TCOM;
		DEBUG_ONLY(
			/* External filter format has only ONE TSTART..TCOM. The journal record received from the external filter
			 * SHOULD also have only ONE TSTART..TCOM
			 */
			tcom_num++;
			assert(1 == tcom_num);
		)
		rec->jrec_tcom.num_participants = 1; /* Only ONE TSTART..TCOM in the external filter format */
		in_tp = FALSE;
		break;

	case MUEXT_PINI:
	case MUEXT_PFIN:
	case MUEXT_EOF:
	case MUEXT_ZTSTART:
	case MUEXT_ZTCOMMIT:
		assert(FALSE);
		ext_stop = ptr;
		return (char *)rec;
		break;

	case MUEXT_NULL:
		rec->prefix.jrec_type = JRT_NULL;
		break;

	default:
		assert(FALSE);
		ext_stop = ptr;
		return (char *)rec;
		break;
	}
	rectype = (enum jnl_record_type)rec->prefix.jrec_type;
	ptr = STRTOK_R(ptr, "\\", &strtokptr);		/* get the rec-type field */
	assert(NULL != ptr);
	ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the time field */
	assert(NULL != ptr);
	ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the tn field */
	assert(NULL != ptr);
	rec->prefix.tn = asc2i((uchar_ptr_t)ptr, STRLEN(ptr));
	ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the pid field */
	assert(NULL != ptr);
	ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the client pid field */
	assert(NULL != ptr);
	ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the token/jnl_seqno field */
	assert(NULL != ptr);
	rec->jrec_null.jnl_seqno = saved_jnl_seqno;
	ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the strm_num field */
	assert(NULL != ptr);
	ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the strm_seqno field */
	assert(NULL != ptr);
	rec->jrec_null.strm_seqno = saved_strm_seqno;
	switch(exttype)
	{
		case MUEXT_NULL:
			rec->jrec_null.prefix.forwptr =  rec->jrec_null.suffix.backptr = NULL_RECLEN;
			rec->jrec_null.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
			return ((char_ptr_t)rec) + NULL_RECLEN;
		case MUEXT_TCOMMIT:
			ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the participants */
			ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the jnl_tid */
			rec->jrec_tcom.jnl_tid[0] = 0;
			if (NULL != ptr)
				strcpy(rec->jrec_tcom.jnl_tid, ptr);
			num_records = 0;
			rec->jrec_tcom.prefix.forwptr =  rec->jrec_tcom.suffix.backptr = TCOM_RECLEN;
			rec->jrec_tcom.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
			return ((char_ptr_t)rec) + TCOM_RECLEN;
		default:
			break;
	}
	assert(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype));
	ptr = STRTOK_R(NULL, "\\", &strtokptr);	/* get the update_num field */
	assert(NULL != ptr);
	assert(OFFSETOF(struct_jrec_upd, update_num) == OFFSETOF(struct_jrec_ztworm, update_num));
	rec->jrec_set_kill.update_num = num_records;
	if ((MUEXT_ZTWORM != exttype) && (MUEXT_LGTRIG != exttype))
	{
		ptr = STRTOK_R(NULL, "\\", &strtokptr);		/* get the nodeflags field */
		assert(NULL != ptr);
		rec->jrec_set_kill.mumps_node.nodeflags = asc2i((uchar_ptr_t)ptr, STRLEN(ptr));
	}
	ptr += (strlen(ptr) + 1); /* get the key-value and data also; can't use strtok since there might be '\\' in the subscript */
	assert(NULL != ptr);
	if ((MUEXT_ZTWORM != exttype) && (MUEXT_LGTRIG != exttype))
	{
		assert(IS_SET_KILL_ZKILL_ZTRIG(rectype));
		len = STRLEN(ptr);
		val_off = ptr;
		keylength = zwrkeyvallen(ptr, len, &val_off, &val_len, NULL, NULL); /* determine length of key */
		REPL_DPRINT2("ext2jnl source:KEY=DATA:%s\n", ptr);
		assert(keylength <= len);
		str2gvkey_nogvfunc(ptr, keylength, gv_currkey);
		rec->jrec_set_kill.mumps_node.length = gv_currkey->end;
		memcpy(rec->jrec_set_kill.mumps_node.text, gv_currkey->base, gv_currkey->end);
		temp_reclen = (int)(FIXED_UPD_RECLEN + rec->jrec_set_kill.mumps_node.length + SIZEOF(jnl_str_len_t));
		if (IS_KILL_ZKILL_ZTRIG(rectype))
		{
			temp_reclen += JREC_SUFFIX_SIZE;
			reclen = ROUND_UP2(temp_reclen, JNL_REC_START_BNDRY);
			memset((char_ptr_t)rec + temp_reclen - JREC_SUFFIX_SIZE, 0, reclen - temp_reclen);
			suffix = (jrec_suffix *)((char_ptr_t)rec + reclen - JREC_SUFFIX_SIZE);
			rec->prefix.forwptr = suffix->backptr = reclen;
			suffix->suffix_code = JNL_REC_SUFFIX_CODE;
			return (char_ptr_t)rec + reclen;
		}
		/* we have to get the data value now */
		src.len = val_len;
		src.addr = val_off;
	} else
	{	/* ZTWORMHOLE or LGTRIG jnl record */
		assert(IS_ZTWORM(rectype) || IS_LGTRIG(rectype));
		src.addr = ptr;
		src.len = STRLEN(ptr);
		assert(FIXED_ZTWORM_RECLEN == FIXED_LGTRIG_RECLEN);
		temp_reclen = (int)(FIXED_ZTWORM_RECLEN);
	}
	des.len = 0;
	des.addr = (char_ptr_t)rec + temp_reclen + SIZEOF(jnl_str_len_t);
	REPL_DPRINT3("ext2jnl JNL Format (before zwr2format): src : Len %d :: DATA:%s\n", src.len, src.addr);
	REPL_DPRINT3("ext2jnl JNL Format (before zwr2format): des : Len %d :: DATA:%s\n", des.len, des.addr);
	if (!zwr2format(&src, &des))
	{
		assert(FALSE);
		return (char_ptr_t)rec;
	}
	REPL_DPRINT3("ext2jnl JNL Format : src : Len %d :: DATA:%s\n", src.len, src.addr);
	REPL_DPRINT3("ext2jnl JNL Format : des : Len %d :: DATA:%s\n", des.len, des.addr);
	PUT_MSTR_LEN((char_ptr_t)rec + temp_reclen, des.len);
	temp_reclen += SIZEOF(jnl_str_len_t) + des.len + JREC_SUFFIX_SIZE;
	reclen = ROUND_UP2(temp_reclen, JNL_REC_START_BNDRY);
	memset((char_ptr_t)rec + temp_reclen - JREC_SUFFIX_SIZE, 0, reclen - temp_reclen);
	suffix = (jrec_suffix *)((char_ptr_t)rec + reclen - JREC_SUFFIX_SIZE);
	rec->prefix.forwptr = suffix->backptr = reclen;
	suffix->suffix_code = JNL_REC_SUFFIX_CODE;
	return (char_ptr_t)rec + reclen;
}
