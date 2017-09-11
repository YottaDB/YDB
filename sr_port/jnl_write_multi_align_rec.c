/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

void	jnl_write_multi_align_rec(sgmnt_addrs *csa, uint4 align_filler_len, jnl_tm_t time)
{
	jnl_buffer_ptr_t	jbp;
	uint4			alignsize, max_filler_len, max_jrec_len, orig_align_filler_len;
	uint4			filler_len1, filler_len2;

	jbp = csa->jnl->jnl_buff;
	alignsize = jbp->alignsize;
	max_jrec_len = jbp->max_jrec_len;
	assert(max_jrec_len < alignsize);
	max_filler_len = max_jrec_len - MIN_ALIGN_RECLEN;
	DEBUG_ONLY(orig_align_filler_len = align_filler_len);
	while (align_filler_len > max_jrec_len)
	{
		jnl_write_align_rec(csa, max_filler_len, time);	/* this will write an ALIGN record of size
								 * MIN_ALIGN_RECLEN + max_filler_len == max_jrec_len.
								 */
		align_filler_len -= max_jrec_len;
	}
	/* At this point, "align_filler_len <= max_jrec_len" */
	if (max_filler_len >= align_filler_len)
	{	/* Simple case. Write one JRT_ALIGN and return */
		jnl_write_align_rec(csa, align_filler_len, time);
		return;
	}
	/* At this point, "max_filler_len < align_filler_len <= max_jrec_len". This is an edge case.
	 * We need to write two ALIGN records to ensure that
	 *	a) Record size of each of the two ALIGN records stays within "max_jrec_len" AND
	 *	b) Total size of the two ALIGN records == align_filler_len + MIN_ALIGN_RECLEN;
	 */
	assert(MIN_ALIGN_RECLEN < max_filler_len);
	filler_len1 = max_filler_len - MIN_ALIGN_RECLEN; /* align rec_size1 = filler_len1 + MIN_ALIGN_RECLEN == max_filler_len */
	filler_len2 = align_filler_len - max_filler_len; /* align rec_size2 = filler_len2 + MIN_ALIGN_RECLEN */
	/* rec_size1 + rec_size2 = max_filler_len + ((align_filler_len - max_filler_len) + MIN_ALIGN_RECLEN)
	 *                       == align_filler_len + MIN_ALIGN_RECLEN
	 */
	jnl_write_align_rec(csa, filler_len1, time);
	jnl_write_align_rec(csa, filler_len2, time);
	return;
}
