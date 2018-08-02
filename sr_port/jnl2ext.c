/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>	/* for offsetof macro */
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"		/* for filestruct.h */
#include "gdsbt.h"		/* for gdsfhead.h */
#include "gtm_facility.h"	/* for fileinfo.h */
#include "fileinfo.h"		/* for gdsfhead.h */
#include "gdsfhead.h"		/* for filestruct.h */
#include "filestruct.h"		/* for jnl.h */
#include "jnl.h"
#include "copy.h"		/* for REF_CHAR macro */
#include "repl_filter.h"
#include "format_targ_key.h"
#include "mlkdef.h"
#include "zshow.h"
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "real_len.h"		/* for real_len() prototype */

#define	DELIMIT_CURR	*curr++ = '\\';
#define PIDS_DELIM	"0\\0\\"

#define	JNL2EXT_STRM_SEQNO(CURR, STRM_SEQNO)				\
{									\
	seq_num		lcl_strm_seqno;					\
	uint4		lcl_strm_num;					\
									\
	DELIMIT_CURR;							\
	lcl_strm_seqno = STRM_SEQNO;					\
	lcl_strm_num = GET_STRM_INDEX(lcl_strm_seqno);			\
	CURR = (char *)i2ascl((uchar_ptr_t)CURR, lcl_strm_num);		\
	DELIMIT_CURR;							\
	lcl_strm_seqno = GET_STRM_SEQ60(lcl_strm_seqno);		\
	CURR = (char *)i2ascl((uchar_ptr_t)CURR, lcl_strm_seqno);	\
}

#ifdef DEBUG
GBLREF	char		*jb_stop;
#endif
GBLREF	char		muext_code[][2];

static	boolean_t	first_tstart = FALSE;
static	int4		num_tstarts = 0;
static	int4		num_tcommits = 0;

/* Generic function to convert a journal record into an extract format record.
 * Expects a pointer to the journal record and the length of the buffer and
 * returns the result in ext_buff. If there is a bad format record in between,
 * it does an assertpro. It sets jb_stop to the offset of the jnl_buff where
 * the last jnlrecord was processed. On successful conversion, it returns a value
 * ptr, such that ptr - ext_buff would be the length of the extracted buffer.
 */
char	*jnl2extcvt(jnl_record *rec, int4 jnl_len, char **ext_buff, int *extract_bufsiz)
{
	int	rec_len, tmpbufsiz, tmpsize;
	char	*extbuf, *exttop, *tmp, *origbuf;

	extbuf = *ext_buff;
	exttop = extbuf + *extract_bufsiz;
	for ( ; jnl_len > JREC_PREFIX_UPTO_LEN_SIZE && jnl_len >= (rec_len = rec->prefix.forwptr) && (MIN_JNLREC_SIZE <= rec_len); )
	{
		if (MAX_ONE_JREC_EXTRACT_BUFSIZ > (exttop - extbuf))
		{	/* Remaining space not enough to hold the worst-case journal extract of ONE jnl record. Expand linearly */
			tmpsize = *extract_bufsiz;
			tmpbufsiz = tmpsize + (JNL2EXTCVT_EXPAND_FACTOR * MAX_ONE_JREC_EXTRACT_BUFSIZ);
			tmp = malloc(tmpbufsiz);
			origbuf = *ext_buff;
			tmpsize = extbuf - origbuf;
			memcpy(tmp, origbuf, tmpsize);
			free(origbuf);
			*ext_buff = tmp;
			*extract_bufsiz = tmpbufsiz;
			extbuf = tmp + tmpsize;
			exttop = tmp + tmpbufsiz;
		}
		extbuf = jnl2ext((char *)rec, extbuf);
		jnl_len -= rec_len;
		rec = (jnl_record *)((char *)rec + rec_len);
	}
	DEBUG_ONLY(jb_stop = (char *)rec;)
	return extbuf;
}

char	*jnl2ext(char *jnl_buff, char *ext_buff)
{
  	char		*curr, *val_ptr, rectype;
	unsigned char	*ptr;
	jnl_record	*rec;
	gv_key		*key;
	jnl_string	*keystr, *ztwormstr;
	int		val_extr_len, val_len, rec_len, tid_len;
	char		key_buff[SIZEOF(gv_key) + MAX_KEY_SZ + 7];

	rec = (jnl_record *)jnl_buff;
	rectype = rec->prefix.jrec_type;
	rec_len = rec->prefix.forwptr;
	if ((ROUND_DOWN2(rec_len, JNL_REC_START_BNDRY) != rec_len) || rec_len != REC_LEN_FROM_SUFFIX(jnl_buff, rec_len))
	{
		assertpro(FALSE);
		return ext_buff;
	}
	if (!IS_REPLICATED(rectype))
	{
		assertpro(FALSE);
		return ext_buff;
	}
	curr = ext_buff;
	/* The following assumes the journal extract format is "GDSJEX07". Whenever that changes (in mur_jnl_ext.c),
	 * the below code as well as ext2jnl.c will need to change. Add an assert to let us know of that event.
	 */
	assert(!MEMCMP_LIT(JNL_EXTR_LABEL,"GDSJEX07"));
	if (IS_TUPD(rectype))
	{
		if (FALSE == first_tstart)
		{
			GET_SHORTP(curr, &muext_code[MUEXT_TSTART][0]);
			curr += 2;
			DELIMIT_CURR;
			curr += exttime(rec->prefix.time, curr, 0);
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_set_kill.prefix.tn);
			DELIMIT_CURR;
			MEMCPY_LIT(curr, PIDS_DELIM);
			curr += STR_LIT_LEN(PIDS_DELIM);
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_set_kill.token_seq.jnl_seqno);
			JNL2EXT_STRM_SEQNO(curr, rec->jrec_set_kill.strm_seqno);	/* Note: updates "curr" */
			*curr++ = '\n';
			*curr = '\0';
			first_tstart = TRUE;
		}
		num_tstarts++;
	} else if (JRT_TCOM == rectype)
	{
		num_tcommits++;
		if (num_tcommits == num_tstarts)
		{
			num_tcommits = num_tstarts = 0;
			first_tstart = FALSE;
			GET_SHORTP(curr, &muext_code[MUEXT_TCOMMIT][0]);
			curr += 2;
			DELIMIT_CURR;
			curr += exttime(rec->prefix.time, curr, 0);
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_tcom.prefix.tn);
			DELIMIT_CURR;
			MEMCPY_LIT(curr, PIDS_DELIM);
			curr += STR_LIT_LEN(PIDS_DELIM);
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_tcom.token_seq.jnl_seqno);
			JNL2EXT_STRM_SEQNO(curr, rec->jrec_tcom.strm_seqno);	/* Note: updates "curr" */
			DELIMIT_CURR;
			*curr = '1'; /* Only ONE TSTART..TCOM in the external filter format */
			curr++;
			DELIMIT_CURR;
			ptr = (unsigned char *)rec->jrec_tcom.jnl_tid;
			tid_len = real_len(SIZEOF(rec->jrec_tcom.jnl_tid), ptr);
			memcpy(curr, ptr, tid_len);
			curr += tid_len;
			*curr++ = '\n';
			*curr = '\0';
			return curr;
		}
		return ext_buff;
	}
	if (IS_SET(rectype))
		GET_SHORTP(curr, &muext_code[MUEXT_SET][0]);
	else if (IS_KILL(rectype))
		GET_SHORTP(curr, &muext_code[MUEXT_KILL][0]);
	else if (IS_ZKILL(rectype))
		GET_SHORTP(curr, &muext_code[MUEXT_ZKILL][0]);
	else if (IS_ZTWORM(rectype))
		GET_SHORTP(curr, &muext_code[MUEXT_ZTWORM][0]);
	else if (IS_LGTRIG(rectype))
		GET_SHORTP(curr, &muext_code[MUEXT_LGTRIG][0]);
	else if (IS_ZTRIG(rectype))
		GET_SHORTP(curr, &muext_code[MUEXT_ZTRIG][0]);
	else /* if (JRT_NULL == rectype) */
	{
		assert(JRT_NULL == rectype);
		GET_SHORTP(curr, &muext_code[MUEXT_NULL][0]);
	}
	curr += 2;
	DELIMIT_CURR;
	curr += exttime(rec->prefix.time, curr, 0);
	curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_set_kill.prefix.tn);
	DELIMIT_CURR;
	MEMCPY_LIT(curr, PIDS_DELIM);
	curr += STR_LIT_LEN(PIDS_DELIM);
	curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_set_kill.token_seq.jnl_seqno);
	JNL2EXT_STRM_SEQNO(curr, rec->jrec_set_kill.strm_seqno);	/* Note: updates "curr" */
	if (rectype == JRT_NULL)
	{
		*curr++ = '\n';
		*curr='\0';
		return curr;
	}
	DELIMIT_CURR;
	/* print "update_num" */
	assert(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype));
	assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
	assert(&rec->jrec_set_kill.update_num == &rec->jrec_lgtrig.update_num);
	curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_set_kill.update_num);
	DELIMIT_CURR;
	if (IS_ZTWORM(rectype) || IS_LGTRIG(rectype))
	{
		assert(&rec->jrec_ztworm.ztworm_str == &rec->jrec_lgtrig.lgtrig_str);
		ztwormstr = &rec->jrec_ztworm.ztworm_str;
		val_len = ztwormstr->length;
		val_ptr = &ztwormstr->text[0];
		format2zwr((sm_uc_ptr_t)val_ptr, val_len, (uchar_ptr_t)curr, &val_extr_len);
		curr += val_extr_len;
		*curr++ = '\n';
		*curr='\0';
		return curr;
	}
	/* print "nodeflags" */
	keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
	curr = (char *)i2ascl((uchar_ptr_t)curr, keystr->nodeflags);
	DELIMIT_CURR;
	/* print "node" */
	key = (gv_key *)ROUND_UP((unsigned long)key_buff, 8);
	key->top = MAX_KEY_SZ;
	key->end = keystr->length;
	if (key->end > key->top)
	{
		assertpro(FALSE);
		return ext_buff;
	}
	memcpy(key->base, &keystr->text[0], keystr->length);
	key->base[key->end] = 0;
	curr = (char *)format_targ_key((uchar_ptr_t)curr, MAX_ZWR_KEY_SZ, key, TRUE);
	if (IS_SET(rectype))
	{
		*curr++ = '=';
		val_ptr = &keystr->text[keystr->length];
		GET_MSTR_LEN(val_len, val_ptr);
		val_ptr += SIZEOF(mstr_len_t);
		format2zwr((sm_uc_ptr_t)val_ptr, val_len, (uchar_ptr_t)curr, &val_extr_len);
		curr += val_extr_len;
	}
	*curr++ = '\n';
	*curr='\0';
	return curr;
}
