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

#define	DELIMIT_CURR	*curr++ = '\\';
#define ZERO_TIME_DELIM	"0,0\\"
#define PIDS_DELIM	"0\\0\\"

/*
 * Generic function to convert a journal record into an extract format record.
 * Expects a pointer to the journal record and the length of the buffer and
 * returns the result in ext_buff. If there is a bad format record in between,
 * it returns NULL. It sets jb_stop to the offset of the jnl_buff where
 * the last jnlrecord was processed. On successful conversion, it returns a value
 * ptr, such that ptr - ext_buff would be the length of the extracted buffer.
 * If the ext_len is not enough for the conversion, it returns ext_buff + ext_len
 */

GBLREF	char		*jb_stop;
GBLREF	char		muext_code[][2];
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];

static	boolean_t	first_tstart = FALSE;
static	int4		num_tstarts = 0;
static	int4		num_tcommits = 0;


char 	*jnl2extcvt(jnl_record *rec, int4 jnl_len, char *ext_buff)
{
	int4		rec_len;

	for ( ; jnl_len > JREC_PREFIX_UPTO_LEN_SIZE && jnl_len >= (rec_len = rec->prefix.forwptr) && rec_len > MIN_JNLREC_SIZE; )
	{
		ext_buff = jnl2ext((char *)rec, ext_buff);
		jnl_len -= rec_len;
		rec = (jnl_record *)((char *)rec + rec_len);
	}

	jb_stop = (char *)rec;
	return ext_buff;
}


char	*jnl2ext(char *jnl_buff, char *ext_buff)
{
	char		*curr, *val_ptr, *ptr, rectype, key_buff[sizeof(gv_key) + MAX_KEY_SZ + 7];
	jnl_record	*rec;
	gv_key		*key;
	jnl_string	*keystr;
	int		val_extr_len, val_len, rec_len;

	rec = (jnl_record *)jnl_buff;
	rectype = rec->prefix.jrec_type;
	rec_len = rec->prefix.forwptr;
	if (rec_len != REC_LEN_FROM_SUFFIX(jnl_buff, rec_len))
	{
		assert(FALSE);
		return ext_buff;
	}
	if (!IS_REPLICATED(rectype))
	{
		assert(FALSE);
		return ext_buff;
	}
	curr = ext_buff;
	if (IS_TUPD(rectype))
	{
		if (FALSE == first_tstart)
		{
			GET_SHORTP(curr, &muext_code[MUEXT_TSTART][0]);
			curr += 2;
			DELIMIT_CURR;
			MEMCPY_LIT(curr, ZERO_TIME_DELIM);
			curr += STR_LIT_LEN(ZERO_TIME_DELIM);
			curr = (char *)i2asc((uchar_ptr_t)curr, rec->jrec_kill.prefix.tn);
			DELIMIT_CURR;
			MEMCPY_LIT(curr, PIDS_DELIM);
			curr += STR_LIT_LEN(PIDS_DELIM);
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_kill.token_seq.jnl_seqno);
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
			MEMCPY_LIT(curr, ZERO_TIME_DELIM);
			curr += STR_LIT_LEN(ZERO_TIME_DELIM);
			curr = (char *)i2asc((uchar_ptr_t)curr, rec->jrec_tcom.prefix.tn);
			DELIMIT_CURR;
			MEMCPY_LIT(curr, PIDS_DELIM);
			curr += STR_LIT_LEN(PIDS_DELIM);
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_tcom.token_seq.jnl_seqno);
			DELIMIT_CURR;
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_tcom.participants);
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
	else /* if (JRT_NULL == rectype) */
	{
		assert(JRT_NULL == rectype);
		GET_SHORTP(curr, &muext_code[MUEXT_NULL][0]);
	}
	curr += 2;
	DELIMIT_CURR;
	MEMCPY_LIT(curr, ZERO_TIME_DELIM);
	curr += STR_LIT_LEN(ZERO_TIME_DELIM);
	curr = (char *)i2asc((uchar_ptr_t)curr, rec->jrec_kill.prefix.tn);
	DELIMIT_CURR;
	MEMCPY_LIT(curr, PIDS_DELIM);
	curr += STR_LIT_LEN(PIDS_DELIM);
	curr = (char *)i2ascl((uchar_ptr_t)curr, rec->jrec_kill.token_seq.jnl_seqno);
	if (rectype == JRT_NULL)
	{
		*curr++ = '\n';
		*curr='\0';
		return curr;
	}
	assert(IS_SET_KILL_ZKILL(rectype));
	DELIMIT_CURR;
	keystr = (jnl_string *)&rec->jrec_kill.mumps_node;
	ptr = (char *)ROUND_UP((uint4)key_buff, 8);
	key = (gv_key *)ptr;
	key->top = MAX_KEY_SZ;
	key->end = keystr->length;
	if (key->end > key->top)
	{
		assert(FALSE);
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
		val_ptr += sizeof(mstr_len_t);
		format2zwr((sm_uc_ptr_t)val_ptr, val_len, (uchar_ptr_t)curr, &val_extr_len);
		curr += val_extr_len;
	}
	*curr++ = '\n';
	*curr='\0';
	return curr;
}
