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

#include "gtm_string.h"

#include <stddef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "v12_jnl.h"
#include "copy.h"
#include "iosp.h"
#include "repl_filter.h"
#include "repl_errno.h"

GBLREF unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF unsigned char	jnl_source_rectype, jnl_dest_maxrectype;

LITREF int      	v11_jnl_fixed_size[], v12_jnl_fixed_size[];

int jnl_v12tov11(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	/* Convert a transaction from jnl version 11 (V4.1) to jnl version 11 (V4.2-FT10) */

	/* Differences between ver 11, and 12 :
	 *					       11			        11
	 *------------------------------------------------------------------------------------------------------------
	 * {tcom,ztcom}.participants		NA				Shifted by 8 bytes due to addition of
										orig_tc_short_time
	 * {tcom,ztcom}.ts_short_time		same as above
	 * An addtional field orig_ts_short_time is added.
	 */

	unsigned char		*jb, *cb, *cstart, *jstart, rectype;
	int			status, reclen;
	unsigned short		key_len;
	unsigned int		long_data_len, total_data, jlen, less_space, nzeros, conv_reclen, clen_without_sfx, total_key;
	mstr_len_t		data_len;
	boolean_t		is_set, is_com;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	while (0 < jlen)
	{
		if (0 < (reclen = v12_jnl_record_length((jnl_record *)jb, jlen)))
		{
			if (reclen <= jlen)
			{
				rectype = REF_CHAR(jb + V12_JREC_TYPE_OFFSET);
				if (rectype >= V12_JRT_RECTYPES)
				{
					jnl_source_rectype = rectype;
					jnl_dest_maxrectype = V12_JRT_RECTYPES - 1;
					repl_errno = EREPL_INTLFILTER_NEWREC;
					status = -1;
					break;
				}
				is_set = is_com = FALSE;
				less_space = total_key = total_data = 0;
				if ((is_set = (JRT_SET == rectype || JRT_TSET == rectype || JRT_USET == rectype
							|| JRT_FSET == rectype || JRT_GSET == rectype))
					|| (JRT_KILL == rectype || JRT_TKILL == rectype || JRT_UKILL == rectype
						|| JRT_FKILL == rectype || JRT_GKILL == rectype)
					|| (JRT_ZKILL == rectype || JRT_FZKILL == rectype || JRT_GZKILL == rectype
						|| JRT_TZKILL == rectype || JRT_UZKILL == rectype))
				{
					GET_USHORT(key_len, jb + V12_JREC_PREFIX_SIZE + v12_jnl_fixed_size[rectype]);
					total_key = key_len + sizeof(unsigned short);
					if (is_set)
					{
						GET_MSTR_LEN(long_data_len,
							     jb + V12_JREC_PREFIX_SIZE + v12_jnl_fixed_size[rectype] + total_key);
						total_data = long_data_len + sizeof(mstr_len_t);
					}
				}
				if (JRT_TCOM == rectype || JRT_ZTCOM == rectype)
					is_com = TRUE;
				less_space = v12_jnl_fixed_size[rectype] - v11_jnl_fixed_size[rectype];
				assert(V11_JNL_REC_START_BNDRY == V12_JNL_REC_START_BNDRY);
				assert(V11_JREC_PREFIX_SIZE == V12_JREC_PREFIX_SIZE);
				clen_without_sfx = ROUND_UP(V12_JREC_PREFIX_SIZE + v12_jnl_fixed_size[rectype] + total_key +
							    total_data - less_space, V12_JNL_REC_START_BNDRY);
				conv_reclen = clen_without_sfx + V12_JREC_SUFFIX_SIZE;
				if (cb - conv_buff + conv_reclen > conv_bufsiz)
				{
					repl_errno = EREPL_INTLFILTER_NOSPC;
					status = -1;
					break;
				}
				cstart = cb;
				jstart = jb;
				memcpy(cb, jb, V11_JREC_PREFIX_SIZE + V11_MUMPS_NODE_OFFSET);  /* copy till Mumps Node */
				cb += (V11_JREC_PREFIX_SIZE + V11_MUMPS_NODE_OFFSET);
				jb += (V12_JREC_PREFIX_SIZE + V12_MUMPS_NODE_OFFSET);
				if ((JRT_TSET == rectype || JRT_USET == rectype || JRT_FSET == rectype || JRT_GSET == rectype)
						|| (JRT_TKILL == rectype || JRT_UKILL == rectype || JRT_FKILL == rectype
						|| JRT_GKILL == rectype) || (JRT_FZKILL == rectype || JRT_GZKILL == rectype
						|| JRT_TZKILL == rectype || JRT_UZKILL == rectype))
				{
					memcpy(cb, jb, TP_TOKEN_TID_SIZE);
					cb += TP_TOKEN_TID_SIZE;
					jb += TP_TOKEN_TID_SIZE;
				}
				memcpy(cb, jb, total_key);
				cb += total_key;
				jb += total_key;
				if (is_set)
				{
					data_len = (mstr_len_t)long_data_len;
					PUT_MSTR_LEN(cb, data_len);
					cb += sizeof(mstr_len_t);
					jb += sizeof(mstr_len_t);
					memcpy(cb, jb, data_len);
					cb += data_len;
					jb += data_len;
				} else if (is_com)
				{
					memcpy(cb, jb, TOKEN_PARTICIPANTS_TS_SHORT_TIME_SIZE);
					cb += TOKEN_PARTICIPANTS_TS_SHORT_TIME_SIZE;
					jb += TOKEN_PARTICIPANTS_TS_SHORT_TIME_SIZE;
				}
				nzeros = (cstart + clen_without_sfx - cb);
				if (nzeros > 0)
				{
					memset(cb, 0, nzeros);
					cb += nzeros;
				}
				jb = jstart + reclen;
				assert(V11_JREC_SUFFIX_SIZE == V11_JREC_SUFFIX_SIZE);
				memcpy(cb, jb - V11_JREC_SUFFIX_SIZE, V11_JREC_SUFFIX_SIZE);
				cb += V11_JREC_SUFFIX_SIZE;
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
	*conv_len = cb - conv_buff;
	*jnl_len = jb - jnl_buff;
	return(status);
}
