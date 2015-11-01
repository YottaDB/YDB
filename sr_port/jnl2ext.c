/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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

/*
 * generic function to convert a journal record into an extract format record
 * expects a pointer to the journal record and the length of the buffer and
 * returns the result in ext_buff. if there is a bad format record in between,
 * it returns NULL. it sets jb_stop to the offset of the jnl_buff where
 * the last jnlrecord was processed. on successful conversion, it returns a value
 * ptr, such that ptr - ext_buff would be the length of the extracted buffer.
 * If the ext_len is not enough for the conversion, it returns ext_buff + ext_len
 */

GBLDEF	char		*jb_stop;
GBLREF	char		muext_code[][2];

static	boolean_t	first_tstart = FALSE;
static	int4		num_tstarts = 0;
static	int4		num_tcommits = 0;


char 	*jnl2extcvt(jnl_record *rec, int4 jnl_len, char *ext_buff)
{
	int4		rec_len;

	for ( ; 0 != jnl_len  &&  jnl_len >= (rec_len = jnl_record_length(rec, jnl_len))  &&  -1 != rec_len; )
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
	char		*curr, *ptr1, *ptr2, rectype, key_buff[MAX_KEY_SZ + 5];
	jnl_record	*rec;
	gv_key		*key;
	int		temp_int;

	rec = (jnl_record *)jnl_buff;
	rectype = REF_CHAR(&rec->jrec_type);

	curr = ext_buff;

	switch(rectype)
	{
	case JRT_TKILL:
	case JRT_TSET:
	case JRT_TZKILL:
		if (FALSE == first_tstart)
		{
			GET_SHORTP(curr, &muext_code[MUEXT_TSTART][0]);
			curr += 2;
			DELIMIT_CURR;
			SPRINTF(curr,"0,0\\0\\%s\\\n", rec->val.jrec_tset.jnl_tid);
			curr += 8 + strlen(rec->val.jrec_tset.jnl_tid);
			first_tstart = TRUE;
		}
		num_tstarts++;
		break;
	}

	switch(rectype)
	{
	case JRT_TCOM:
		num_tcommits++;
		if (num_tcommits == num_tstarts)
		{
			num_tcommits = num_tstarts = 0;
			first_tstart = FALSE;
			GET_SHORTP(curr, &muext_code[MUEXT_TCOMMIT][0]);
			curr += 2;
			DELIMIT_CURR;
			strcpy(curr,"0,0\\0\\");
			curr += 6;
			curr = (char *)i2ascl((uchar_ptr_t)curr, rec->val.jrec_tcom.jnl_seqno);
			DELIMIT_CURR;
			curr = (char *)i2asc((uchar_ptr_t)curr, rec->val.jrec_tcom.tn);
			DELIMIT_CURR;
			*curr++ = '1';
			*curr++ = '\n';
			*curr = '\0';
			return curr;
		}
		return ext_buff;
		break;

	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
	case JRT_SET:
		GET_SHORTP(curr, &muext_code[MUEXT_SET][0]);
		break;

	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_KILL:
		GET_SHORTP(curr, &muext_code[MUEXT_KILL][0]);
		break;

	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
	case JRT_ZKILL:
		GET_SHORTP(curr, &muext_code[MUEXT_ZKILL][0]);
		break;

	case JRT_NULL:
		GET_SHORTP(curr, &muext_code[MUEXT_NULL][0]);
		break;

	default:
		assert(FALSE);
		return ext_buff;
		break;
	}
	curr += 2;
	DELIMIT_CURR;
	strcpy(curr,"0,0\\0\\");
	curr += 6;
	curr = (char *)i2ascl((uchar_ptr_t)curr, rec->val.jrec_kill.jnl_seqno);
	DELIMIT_CURR;
	curr = (char *)i2asc((uchar_ptr_t)curr, rec->val.jrec_kill.tn);
	DELIMIT_CURR;

	switch (rectype)
	{
	case JRT_KILL:
	case JRT_SET:
	case JRT_ZKILL:
		ptr1 = (char *)&rec->val.jrec_kill.mumps_node.length;
		ptr2 = rec->val.jrec_kill.mumps_node.text;
		break;

	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		ptr1 = (char *)&rec->val.jrec_fkill.mumps_node.length;
		ptr2 = rec->val.jrec_fkill.mumps_node.text;
	}

	if (rectype != JRT_NULL)
	{
		key = (gv_key *)key_buff;
		key->top = MAX_KEY_SZ;
		GET_SHORT(key->end, ptr1);
		memcpy(key->base, ptr2, key->end);
		key->base[key->end] = '\0';
		curr = (char *)format_targ_key((uchar_ptr_t)curr, MAX_KEY_SZ + 5, key, TRUE);
	}

	switch(rectype)
	{
	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		*curr++ = '=';
		ptr1 += key->end + sizeof(short);
		GET_LONG(temp_int, ptr1);
		format2zwr((sm_uc_ptr_t)ptr1 + sizeof(int), temp_int, (uchar_ptr_t)curr, &temp_int);
		curr += temp_int;
		break;
    	}
	*curr++ = '\n';
	*curr='\0';
	return curr;
}
