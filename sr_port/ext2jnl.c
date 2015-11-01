/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "copy.h"
#include "zshow.h"
#include "mvalconv.h"
#include "str2gvkey.h"

static	boolean_t	in_tp;
static	int4		num_records;
static	char		jn_tid[8];	/* for transaction-id in TSTART record */

GBLDEF	char		*ext_stop;

LITREF	int		jnl_fixed_size[];

GBLREF	gv_key		*gv_currkey;

/* callers please set up the proper condition-handlers */
/* expects a null-terminated ext_buff. does the equivalent but inverse of jnl2ext */

char	*ext2jnlcvt(char *ext_buff, int4 ext_len, jnl_record *rec)
{
	char		*ext_next;
	jnl_record	*temp_rec;

	temp_rec = rec;
	for ( ; (NULL != (ext_next = strchr(ext_buff, '\n'))); )
	{
		*ext_next++ = '\0';
		rec = (jnl_record *)ext2jnl(ext_buff, rec);
		rec = (jnl_record *)ROUND_UP((int)rec, JNL_REC_START_BNDRY);
		if (ext_stop == ext_buff)
			break;
		ext_buff = ext_next;
	}

	assert(rec != temp_rec);
	ext_stop = ext_buff;
	return (char *)rec;
}


/* expects a single null-terminated ptr (equivalent to one line in the extract-file) */

char	*ext2jnl(char *ptr, jnl_record *rec)
{
	unsigned char	*pool_save;
	char		rectype, *ret, ch;
	int		keylength, keystate, len, i;
	bool		keepgoing;
	mstr		src, des;
	jnl_record	*temp_rec;
	muextract_type	exttype;

	ext_stop = ptr + strlen(ptr) + 1;
	temp_rec = rec;

	exttype = MUEXTRACT_TYPE(ptr);
	assert((exttype >= 0) && (exttype < MUEXT_MAX_TYPES));

	switch(exttype)
	{
	case MUEXT_SET:
		if (in_tp)
		{
			if (0 == num_records)
			{
				num_records++;
				rec->jrec_type = JRT_TSET;
			}
			else
				rec->jrec_type = JRT_USET;
		}
		else
			rec->jrec_type = JRT_SET;
		break;

	case MUEXT_KILL:
		if (in_tp)
		{
			if (0 == num_records)
			{
				num_records++;
				rec->jrec_type = JRT_TKILL;
			}
			else
				rec->jrec_type = JRT_UKILL;
		}
		else
			rec->jrec_type = JRT_KILL;
		break;

	case MUEXT_ZKILL:
		if (in_tp)
		{
			if (0 == num_records)
			{
				num_records++;
				rec->jrec_type = JRT_TZKILL;
			}
			else
				rec->jrec_type = JRT_UZKILL;
		}
		else
			rec->jrec_type = JRT_ZKILL;
		break;

	case MUEXT_TSTART:
		in_tp = TRUE;
		ptr = strtok(ptr, "\\");		/* get the rec-type field */
		ptr = strtok(NULL, "\\");		/* get the time field */
		ptr = strtok(NULL, "\\");		/* get the pid field */
		ptr = strtok(NULL, "\\");		/* get the tid */
		jn_tid[0] = '\0';
		if (NULL != ptr)
			strcpy(jn_tid, ptr);
		num_records = 0;
		return (char *)rec;
		break;

	case MUEXT_TCOMMIT:
		rec->jrec_type = JRT_TCOM;
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
		rec->jrec_type = JRT_NULL;
		break;

	default:
		assert(FALSE);
		ext_stop = ptr;
		return (char *)rec;
		break;
	}

	ptr = strtok(ptr, "\\");		/* get the rec-type field */
	ptr = strtok(NULL, "\\");		/* get the time field */
	ptr = strtok(NULL, "\\");		/* get the pid field */
	ptr = strtok(NULL, "\\");		/* get the jnl_seqno */
	assert(NULL != ptr);

	rectype = REF_CHAR(&rec->jrec_type);
	rec->val.jrec_null.jnl_seqno = asc2l((uchar_ptr_t)ptr, strlen(ptr));
	ptr = strtok(NULL, "\\");		/* get the db-tn field */
	rec->val.jrec_null.tn = asc2i((uchar_ptr_t)ptr, strlen(ptr));
	ret = ((char_ptr_t)rec) + JREC_PREFIX_SIZE + jnl_fixed_size[rectype] + JREC_SUFFIX_SIZE;

	switch(exttype)
	{
	case MUEXT_NULL:
	case MUEXT_TCOMMIT:
		return ret;
		break;
	}
	ptr = strtok(NULL, "\\");		/* get the key-value and data also */
	assert(NULL != ptr);

	/* this part is lifted from go_load. later think of having a common routine */
	len = strlen(ptr);
	keylength = 0;					/* determine length of key */
	keystate  = 0;
	keepgoing = TRUE;
	while((keylength < len) && keepgoing)		/* slightly different here from go_load since we can get kill records too */
	{
		ch = *(ptr + keylength);
		keylength++;
		switch (keystate)
		{
		case 0:						/* in global name */
			if ('=' == ch)					/* end of key */
			{
				keylength--;
				keepgoing = FALSE;
			}
			else if ('(' == ch)				/* start of subscripts */
				keystate = 1;
			break;
		case 1:						/* in subscripts area, but out of "..." or $C(...) */
			switch (ch)
			{
			case ')':					/* end of subscripts ==> end of key */
				keepgoing = FALSE;
				break;
			case '"':					/* step into "..." */
				keystate = 2;
				break;
			case '$':					/* step into $C(...) */
				assert(('C' == *(ptr + keylength)) || ('c' == *(ptr + keylength)));
				assert('(' == *(ptr + keylength + 1));
				keylength += 2;
				keystate = 3;
				break;
			}
			break;
		case 2:						/* in "..." */
			if ('"' == ch)
			{
				switch (*(ptr + keylength))
				{
				case '"':				/* "" */
					keylength++;
					break;
				case '_':				/* _$C(...) */
					assert('$' == *(ptr + keylength + 1));
					assert(('c' == *(ptr + keylength + 2)) || ('C' == *(ptr + keylength + 2)));
					assert('(' == *(ptr + keylength + 3));
					keylength += 4;
					keystate = 3;
					break;
				default:				/* step out of "..." */
					keystate = 1;
				}
			}
			break;
		case 3:						/* in $C(...) */
			if (')' == ch)
			{
				if ('_' == *(ptr + keylength))		/* step into "..." */
				{
					assert('"' == *(ptr + keylength + 1));
					keylength += 2;
					keystate = 2;
					break;
				}
				else
					keystate = 1;			/* step out of $C(...) */
			}
			break;
		default:
			assert(FALSE);
			break;
		}
	}
	assert(keylength <= len);
	str2gvkey_nogvfunc(ptr, keylength, gv_currkey);
	ret += gv_currkey->end + sizeof(rec->val.jrec_kill.mumps_node.length);

	switch(rectype)
	{
	case JRT_KILL:
	case JRT_SET:
	case JRT_ZKILL:
		rec->val.jrec_kill.mumps_node.length = gv_currkey->end;
		memcpy(rec->val.jrec_kill.mumps_node.text, gv_currkey->base, gv_currkey->end);
		break;

	case JRT_TSET:
	case JRT_TKILL:
	case JRT_TZKILL:
		strcpy(rec->val.jrec_tset.jnl_tid, jn_tid);	/* explicit fallthrough */

	case JRT_USET:
	case JRT_UKILL:
	case JRT_UZKILL:
		rec->val.jrec_tkill.mumps_node.length = gv_currkey->end;
		memcpy(rec->val.jrec_tkill.mumps_node.text, gv_currkey->base, gv_currkey->end);
		break;
	}

	switch(rectype)
	{
	case JRT_KILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_ZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		return ret;
		break;
	}

	/* we have to get the data value now */

	src.len = len - keylength - 1;
	src.addr = ptr + (keylength + 1);
	des.len = 0;
	des.addr = ret + sizeof(des.len) - JREC_SUFFIX_SIZE;
	zwr2format(&src, &des);
	memcpy(des.addr - sizeof(des.len), &des.len, sizeof(des.len));
	ret += sizeof(des.len) + des.len;
	return ret;
}

