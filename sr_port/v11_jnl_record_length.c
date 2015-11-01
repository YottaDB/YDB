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

#include "v11_mdef.h"
#include "v11_gdsroot.h"
#include "v11_gdsbt.h"
#include "v11_gtm_facility.h"
#include "v11_fileinfo.h"
#include "v11_gdsfhead.h"
#include "v11_filestruct.h"
#include "v11_jnl.h"
#include "v11_copy.h"

LITDEF	int	v11_jnl_fixed_size[JRT_RECTYPES] =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, size)	size,
#include "v11_jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

GBLREF	boolean_t	is_db_updater;

/*
   Returns:	> 0:	Length of the record, including prefix and suffix;  however, if > top,
			then the record length may be incomplete.  (It is at least as long as
			the returned value, however.)
		-1:	Record format is invalid.
*/

int4	v11_jnl_record_length(jnl_record *rec, int4 top)  /* top is maximum length of record (e.g.: end of buffer) */
{
	enum jnl_record_type	rectype;
	int4			n;
	uint4			data_len;
	unsigned short		m;
	unsigned char		lcl_jrec_type;
	mstr_len_t		mstr_len;

	if ((rectype = REF_CHAR(&rec->jrec_type)) <= JRT_BAD  ||  rectype >= JRT_RECTYPES)
	{
		return -1;
	}

	n = JREC_PREFIX_SIZE + v11_jnl_fixed_size[rectype];

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
		n += JREC_SUFFIX_SIZE;
		break;

	case JRT_ALIGN:
		if (n > top)
			break;
		GET_USHORT(m, &rec->val.jrec_align.align_str.align_string.length);
		n += m + JREC_SUFFIX_SIZE;
		break;

	case JRT_KILL:
	case JRT_ZKILL:
		n += sizeof(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_kill.mumps_node.length);
		n += m + JREC_SUFFIX_SIZE;
		break;

	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		n += sizeof(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_fkill.mumps_node.length);
		n += m + JREC_SUFFIX_SIZE;
		break;

	case JRT_SET:
		n += sizeof(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_set.mumps_node.length);
		n += m;
		if (n + sizeof(mstr_len_t) > top)
		{
			n += sizeof(mstr_len_t);
			break;
		}
		GET_MSTR_LEN(mstr_len, (char *)rec + n);
		n += mstr_len + sizeof(mstr_len_t) + JREC_SUFFIX_SIZE;
		break;

	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		n += sizeof(unsigned short);
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_fset.mumps_node.length);
		n += m;
		if (n + sizeof(mstr_len_t) > top)
		{
			n += sizeof(mstr_len_t);
			break;
		}
		GET_MSTR_LEN(mstr_len, (char *)rec + n);
		n += mstr_len + sizeof(mstr_len_t) + JREC_SUFFIX_SIZE;
		break;

	case JRT_PBLK:
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_pblk.bsiz);
		n += m + JREC_SUFFIX_SIZE;
		break;

	case JRT_AIMG:
		if (n > top)
			break;

		GET_USHORT(m, &rec->val.jrec_aimg.bsiz);
		n += m + JREC_SUFFIX_SIZE;
		break;


	default:
		assert(FALSE);
		return -1;
	}

	n = ROUND_UP(n, JNL_REC_START_BNDRY);
	return n;
}
