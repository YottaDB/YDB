/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <stddef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "copy.h"
#include "iosp.h"
#include "repl_filter.h"
#include "repl_errno.h"
#include "jnl_typedef.h"

int4 v11_jnl_record_length(jnl_record *rec, int4 top);

GBLREF unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF unsigned char	jnl_source_rectype, jnl_dest_maxrectype;
GBLREF char		jn_tid[8];

LITREF int      	v11_jnl_fixed_size[];
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];

int jnl_v11tov15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	/* Convert a transaction from jnl version 11 (V4.2-002) to 15 (V.4.4-002)  */

	unsigned char		*jb, *cb, *cstart, *jstart, rectype;
	int			status, reclen;
	unsigned short		key_len;
	unsigned int		long_data_len, jlen, total_data, nzeros, conv_reclen, clen_without_sfx, total_key;
	jrec_prefix		prefix;
	jrec_suffix		suffix;
	seq_num			jsno;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	while (0 < jlen)
	{
		if (0 < (reclen = v11_jnl_record_length((jnl_record *)jb, jlen)))
		{
			if (reclen <= jlen)
			{
				rectype = REF_CHAR(jb + V11_JREC_TYPE_OFFSET);
				total_key = total_data = 0;
				assert(IS_REPLICATED(rectype));
				if (IS_ZTP(rectype))
					GTMASSERT;	/* ZTP not supported */
				if (IS_SET_KILL_ZKILL(rectype))
				{
					GET_USHORT(key_len, jb + V11_JREC_PREFIX_SIZE + v11_jnl_fixed_size[rectype]);
					total_key = key_len + sizeof(unsigned short);
					if (IS_SET(rectype))
					{
						GET_MSTR_LEN(long_data_len, jb + V11_JREC_PREFIX_SIZE +
								   v11_jnl_fixed_size[rectype] + total_key);
						total_data = long_data_len + sizeof(mstr_len_t);
					}
					conv_reclen = JREC_PREFIX_SIZE + FIXED_UPD_RECLEN +
						total_key + total_data + JREC_SUFFIX_SIZE;
					conv_reclen = ROUND_UP2(conv_reclen, JNL_REC_START_BNDRY);
				} else if (IS_COM(rectype))
					conv_reclen = JREC_PREFIX_SIZE + TCOM_RECLEN + JREC_SUFFIX_SIZE;
				clen_without_sfx = conv_reclen - JREC_SUFFIX_SIZE;
				if (cb - conv_buff + conv_reclen > conv_bufsiz)
				{
					repl_errno = EREPL_INTLFILTER_NOSPC;
					status = -1;
					break;
				}
				cstart = cb;
				jstart = jb;
				prefix.jrec_type = rectype;
				suffix.backptr = prefix.forwptr = conv_reclen;
				prefix.pini_addr = 0;
				prefix.time = 0;
				prefix.tn = 0;
				suffix.suffix_code = JNL_REC_SUFFIX_CODE;
				memcpy(cb, (unsigned char*)&prefix, JREC_PREFIX_SIZE);
				cb += JREC_PREFIX_SIZE;
				memcpy(cb, jb + V11_JREC_PREFIX_SIZE + V11_JNL_SEQNO_OFFSET, sizeof(seq_num));
				cb += sizeof(seq_num);
				if (IS_SET_KILL_ZKILL(rectype))
				{
					PUT_JNL_STR_LEN(cb, key_len);
					jb += (V11_JREC_PREFIX_SIZE + V11_MUMPS_NODE_OFFSET + sizeof(unsigned short));
					if (IS_FENCED(rectype))
						jb += TP_TOKEN_TID_SIZE;
					cb += sizeof(jnl_str_len_t);
					memcpy(cb, jb, key_len);
					cb += key_len;
					jb += key_len;
					if (IS_SET(rectype))
					{
						PUT_MSTR_LEN(cb, long_data_len);
						cb += sizeof(mstr_len_t);
						jb += sizeof(mstr_len_t);
						memcpy(cb, jb, long_data_len);
						cb += long_data_len;
					}
				} else if (IS_COM(rectype))
				{
					assert(JRT_TCOM == rectype);
					memset(cb, 0, TID_STR_SIZE);
					cb += TID_STR_SIZE;
					memcpy(cb, jb + V11_JREC_PREFIX_SIZE + V11_TCOM_PARTICIPANTS_OFFSET, sizeof(uint4));
					cb += sizeof(uint4);
				} else
					assert(FALSE);
				nzeros = (cstart + clen_without_sfx - cb);
				if (nzeros > 0)
				{
					memset(cb, 0, nzeros);
					cb += nzeros;
				}
				jb = jstart + reclen;
				memcpy(cb, (unsigned char*)&suffix, JREC_SUFFIX_SIZE);
				cb += JREC_SUFFIX_SIZE;
				assert(cb == cstart + conv_reclen);
				jlen -= reclen;
				continue;
			}
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			status = -1;
			break;
		}
		repl_errno = EREPL_INTLFILTER_BADREC;
		status = -1;
		break;
	}
	assert(0 == jlen || -1 == status);
	*jnl_len = jb - jnl_buff;
	*conv_len = cb - conv_buff;
	return(status);
}
