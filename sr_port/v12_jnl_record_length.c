/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "v12_jnl.h"
#include "copy.h"

LITDEF	int	v12_jnl_fixed_size[JRT_RECTYPES] =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, size)	size,
#include "v12_jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

/*
   Returns:	> 0:	Length of the record, including prefix and suffix;  however, if > top,
			then the record length may be incomplete.  (It is at least as long as
			the returned value, however.)
		-1:	Record format is invalid.
*/

int4	v12_jnl_record_length(jnl_record *rec, int4 top)  /* top is maximum length of record (e.g.: end of buffer) */
{
	enum jnl_record_type	rectype;
	int4			n;
	uint4			data_len;
	unsigned short		m;
	unsigned char		lcl_jrec_type;
	mstr_len_t		mstr_len;

	if ((rectype = (enum jnl_record_type)REF_CHAR(&rec->jrec_type)) <= JRT_BAD  ||  rectype >= JRT_RECTYPES)
	{
		return -1;
	}
	n =(int4)(JREC_PREFIX_SIZE + v12_jnl_fixed_size[rectype]);

	switch (rectype)
	{
	case JRT_PINI:
	case JRT_PFIN:
	case JRT_TCOM:
	case JRT_ZTCOM:
	case JRT_EPOCH:
	case JRT_EOF:
	case JRT_NULL:
	case JRT_INCTN:
		n += (int4)JREC_SUFFIX_SIZE;
		break;

	case JRT_ALIGN:
		if (n > top)
			break;
		GET_USHORT(m, &rec->val.jrec_align.align_str.align_string.length);
		n += m + (int4)JREC_SUFFIX_SIZE;
		break;

	case JRT_KILL:
	case JRT_ZKILL:
		n += SIZEOF(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_kill.mumps_node.length);
		n += m + (int4)JREC_SUFFIX_SIZE;
		break;

	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		n += SIZEOF(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_fkill.mumps_node.length);
		n += m + (int4)JREC_SUFFIX_SIZE;
		break;

	case JRT_SET:
		n += SIZEOF(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_set.mumps_node.length);
		n += m;
		if (n + sizeof(mstr_len_t) > top)
		{
			n += SIZEOF(mstr_len_t);
			break;
		}
		GET_MSTR_LEN(mstr_len, (char *)rec + n);
		n += mstr_len + SIZEOF(mstr_len_t) + (int4)JREC_SUFFIX_SIZE;
		break;

	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		n += SIZEOF(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_fset.mumps_node.length);
		n += m;
		if (n + sizeof(mstr_len_t) > top)
		{
			n += SIZEOF(mstr_len_t);
			break;
		}
		GET_MSTR_LEN(mstr_len, (char *)rec + n);
		n += mstr_len + SIZEOF(mstr_len_t) + (int4)JREC_SUFFIX_SIZE;
		break;

	case JRT_PBLK:
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_pblk.bsiz);
		n += m + (int4)JREC_SUFFIX_SIZE;
		break;

	case JRT_AIMG:
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_aimg.bsiz);
		n += m + (int4)JREC_SUFFIX_SIZE;
		break;


	default:
		assert(FALSE);
		return -1;
	}

	n = ROUND_UP(n, JNL_REC_START_BNDRY);
	return n;
}
