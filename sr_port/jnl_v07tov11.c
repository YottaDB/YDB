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

#include "gtm_string.h"

#include <stddef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "copy.h"
#include "iosp.h"
#include "repl_filter.h"
#include "repl_errno.h"
int4 v07_jnl_record_length(jnl_record *rec, int4 top);

GBLREF unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLREF unsigned char	jnl_source_rectype, jnl_dest_maxrectype;

LITREF int      	v07_jnl_fixed_size[], jnl_fixed_size[];

int jnl_v07tov11(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	/* Convert a transaction from jnl version 07 (V4.1) to jnl version 11 (V4.2-FT10) */

	/* Differences between ver 07, and 11 :
	 *					       07			        11
	 *------------------------------------------------------------------------------------------------------------
	 * {tcom,ztcom}.token			4 bytes				8 bytes
	 * {tcom,ztcom}.participants		offset 28th byte		offset 32nd byte
	 * {tcom,ztcom}.ts_short_time		NA				offset 36th byte, size 4 bytes
	 *
	 * mumps data value for SET type	ushort(2 bytes), char[]		uint(4 bytes), char[]
	 * records (len, string) for non VAX
	 */

	unsigned char		*jb, *cb, *cstart, *jstart, rectype;
	int			status, reclen;
	unsigned short		key_len, short_data_len;
	unsigned int		data_len, jlen, more_space, total_data, nzeros, conv_reclen, clen_without_sfx, total_key;
	boolean_t		is_set, is_com;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	while (0 < jlen)
	{
		if (0 < (reclen = v07_jnl_record_length((jnl_record *)jb, jlen)))
		{
			if (reclen <= jlen)
			{
				rectype = REF_CHAR(jb + V07_JREC_TYPE_OFFSET);
				is_set = is_com = FALSE;
				more_space = total_key = total_data = 0;
				if ((is_set = (JRT_SET == rectype || JRT_TSET == rectype || JRT_USET == rectype
							|| JRT_FSET == rectype || JRT_GSET == rectype))
					|| (JRT_KILL == rectype || JRT_TKILL == rectype || JRT_UKILL == rectype
						|| JRT_FKILL == rectype || JRT_GKILL == rectype)
					|| (JRT_ZKILL == rectype || JRT_FZKILL == rectype || JRT_GZKILL == rectype
						|| JRT_TZKILL == rectype || JRT_UZKILL == rectype))
				{
					GET_USHORT(key_len, jb + V07_JREC_PREFIX_SIZE + v07_jnl_fixed_size[rectype]);
					total_key = key_len + sizeof(unsigned short);
					if (is_set)
					{
						more_space = sizeof(mstr_len_t) - sizeof(unsigned short);
						GET_USHORT(short_data_len, jb + V07_JREC_PREFIX_SIZE +
									   v07_jnl_fixed_size[rectype] + total_key);
						total_data = short_data_len + sizeof(unsigned short);
					}
				}
				else if ((is_com = (JRT_TCOM == rectype || JRT_ZTCOM == rectype)))
					more_space = jnl_fixed_size[rectype] - v07_jnl_fixed_size[rectype];

				assert(V07_JNL_REC_START_BNDRY == V11_JNL_REC_START_BNDRY);
				assert(V07_JREC_PREFIX_SIZE == V11_JREC_PREFIX_SIZE);
				assert(v07_jnl_fixed_size[rectype] == jnl_fixed_size[rectype] || (is_com && (0 == total_key)));
				clen_without_sfx = ROUND_UP(V07_JREC_PREFIX_SIZE + v07_jnl_fixed_size[rectype] + total_key +
							    total_data + more_space, V11_JNL_REC_START_BNDRY);
				conv_reclen = clen_without_sfx + V11_JREC_SUFFIX_SIZE;
				if (cb - conv_buff + conv_reclen > conv_bufsiz)
				{
					repl_errno = EREPL_INTLFILTER_NOSPC;
					status = -1;
					break;
				}
				cstart = cb;
				jstart = jb;
				memcpy(cb, jb, V07_JREC_PREFIX_SIZE + v07_jnl_fixed_size[rectype] + total_key);
				cb += (V07_JREC_PREFIX_SIZE + v07_jnl_fixed_size[rectype] + total_key);
				jb += (V07_JREC_PREFIX_SIZE + v07_jnl_fixed_size[rectype] + total_key);
				if (is_set)
				{	/* Length of data changed from ushort (V4.1) to uint (V4.2-FT10) (for non VAX) */
					data_len = short_data_len;
					PUT_MSTR_LEN(cb, data_len);
					cb += sizeof(mstr_len_t);
					jb += sizeof(unsigned short);
					memcpy(cb, jb, data_len);
					cb += data_len;
					jb += data_len;
				} else if (is_com)
				{
					assert(0 == ((unsigned long)(cstart + V11_JREC_PREFIX_SIZE + V11_TCOM_TOKEN_OFFSET) &
						     (sizeof(token_num) - 1)));
					assert(0 == ((unsigned long)(jstart + V07_JREC_PREFIX_SIZE + V07_TCOM_TOKEN_OFFSET) &
						     (sizeof(uint4) - 1)));
					QWASSIGNDW(*(token_num *)(cstart + V11_JREC_PREFIX_SIZE + V11_TCOM_TOKEN_OFFSET),
							*(uint4 *)(jstart + V07_JREC_PREFIX_SIZE + V07_TCOM_TOKEN_OFFSET));
					assert(0 == ((unsigned long)(cstart + V11_JREC_PREFIX_SIZE + V11_TCOM_PARTICIPANTS_OFFSET) &
						     (sizeof(uint4) - 1)));
					assert(0 == ((unsigned long)(jstart + V07_JREC_PREFIX_SIZE + V07_TCOM_PARTICIPANTS_OFFSET) &
						     (sizeof(uint4) - 1)));
					*(uint4 *)(cstart + V11_JREC_PREFIX_SIZE + V11_TCOM_PARTICIPANTS_OFFSET) =
						*(uint4 *)(jstart + V07_JREC_PREFIX_SIZE + V07_TCOM_PARTICIPANTS_OFFSET);
					assert(0 == ((unsigned long)(cstart + V11_JREC_PREFIX_SIZE +V11_TCOM_TS_SHORT_TIME_OFFSET) &
						     (sizeof(uint4) - 1)));
					*(uint4 *)(cstart +V11_JREC_PREFIX_SIZE +V11_TCOM_TS_SHORT_TIME_OFFSET) = 0;/* don't care */
					cb += more_space;
				}
				nzeros = (cstart + clen_without_sfx - cb);
				if (nzeros > 0)
				{
					memset(cb, 0, nzeros);
					cb += nzeros;
				}
				jb = jstart + reclen;
				assert(V07_JREC_SUFFIX_SIZE == V11_JREC_SUFFIX_SIZE);
				memcpy(cb, jb - V07_JREC_SUFFIX_SIZE, V07_JREC_SUFFIX_SIZE);
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
	*jnl_len = jb - jnl_buff;
	*conv_len = cb - conv_buff;
	return(status);
}
