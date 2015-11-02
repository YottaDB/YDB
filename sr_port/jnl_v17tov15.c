/****************************************************************
 *								*
 *	Copyright 2005, 2007 Fidelity Information Services, Inc	*
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
#include "collseq.h"

LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];
LITREF	int		jrt_update[JRT_RECTYPES];
GBLREF	boolean_t	null_subs_xform;

/*
Convert a transaction from jnl version  17 (V5) to 15  (GT.M versions V4.4-002 through V4.4-004)
Null susbcripts transfromation may be needed and need to check whether any global name length > 8
*/

int jnl_v17tov15(uchar_ptr_t jnl_buff, uint4 *jnl_len, uchar_ptr_t conv_buff, uint4 *conv_len, uint4 conv_bufsiz)
{
	unsigned char		*jb, *cb, *cstart, *jstart, *ptr;
	enum	jnl_record_type	rectype;
	int			status, reclen, conv_reclen;
	uint4			jlen;
	jrec_prefix 		*prefix;
	v15_jrec_prefix		*v15_jrec_prefix_ptr;
	jrec_suffix		*suffix_ptr;

	jb = jnl_buff;
	cb = conv_buff;
	status = SS_NORMAL;
	jlen = *jnl_len;
	assert(0 == ((UINTPTR_T)jb % sizeof(uint4)));
   	while (JREC_PREFIX_SIZE <= jlen)
	{
		assert(0 == ((UINTPTR_T)jb % sizeof(uint4)));
		prefix = (jrec_prefix *)jb;
		rectype = (enum	jnl_record_type)prefix->jrec_type;
		cstart = cb;
		jstart = jb;
   		if (0 != (reclen = prefix->forwptr))
		{
   			if (reclen <= jlen)
			{
				assert(IS_REPLICATED(rectype));
				if (IS_ZTP(rectype))
					GTMASSERT;	/* ZTP not supported */
				conv_reclen = prefix->forwptr - 8 ;
				if (cb - conv_buff + conv_reclen > conv_bufsiz)
				{
					repl_errno = EREPL_INTLFILTER_NOSPC;
					status = -1;
					break;
				}
				/* Check whether the record contains long name */
				if (IS_SET_KILL_ZKILL(rectype))
				{
					ptr = jb + FIXED_UPD_RECLEN + sizeof(jnl_str_len_t);
					if (strlen((char *)ptr) > PRE_V5_MAX_MIDENT_LEN)
					{
						repl_errno = EREPL_INTLFILTER_REPLGBL2LONG;
						assert(FALSE);
						status = -1;
						break;
					}
				}
				v15_jrec_prefix_ptr = (v15_jrec_prefix *)cb;
				v15_jrec_prefix_ptr->jrec_type = rectype;
				v15_jrec_prefix_ptr->forwptr = prefix->forwptr - 8;
				v15_jrec_prefix_ptr->pini_addr = 0;
				v15_jrec_prefix_ptr->time = 0;
				v15_jrec_prefix_ptr->tn = 0;
				cb = cb + sizeof(v15_jrec_prefix);
				jb = jb + JREC_PREFIX_SIZE;
				memcpy(cb, jb, reclen - JREC_PREFIX_SIZE - JREC_SUFFIX_SIZE);
				if (IS_SET_KILL_ZKILL(rectype) && null_subs_xform)
				{
					ptr = cb + (FIXED_UPD_RECLEN - sizeof(v15_jrec_prefix)) + sizeof(jnl_str_len_t);
					/* Prior to V16, GT.M supports only GTM NULL collation */
					assert(STDNULL_TO_GTMNULL_COLL == null_subs_xform);
					STD2GTMNULLCOLL(ptr, *((jnl_str_len_t *)(cb + FIXED_UPD_RECLEN - sizeof(v15_jrec_prefix))));
				}
				cb = cb + conv_reclen - sizeof(v15_jrec_prefix) - JREC_SUFFIX_SIZE ;
				jb = jb + reclen - JREC_PREFIX_SIZE - JREC_SUFFIX_SIZE;
				suffix_ptr = (jrec_suffix *)cb;
				suffix_ptr->backptr = v15_jrec_prefix_ptr->forwptr;
				suffix_ptr->suffix_code = JNL_REC_SUFFIX_CODE;

				cb = cb + JREC_SUFFIX_SIZE;
				jb = jb + JREC_SUFFIX_SIZE;
				assert(cb == cstart + conv_reclen);
				assert(jb == jstart + reclen);
				jlen -= reclen;
				continue;
			}
			repl_errno = EREPL_INTLFILTER_INCMPLREC;
			assert(FALSE);
			status = -1;
			break;
		}
		repl_errno = EREPL_INTLFILTER_BADREC;
		assert(FALSE);
		status = -1;
		break;
	}
	if ((-1 != status) && (0 != jlen))
	{
		repl_errno = EREPL_INTLFILTER_INCMPLREC;
		assert(FALSE);
		status = -1;
	}
	assert(0 == jlen || -1 == status);
	*jnl_len = (uint4)(jb - jnl_buff);
	*conv_len = (uint4)(cb - conv_buff);
	return(status);
}
