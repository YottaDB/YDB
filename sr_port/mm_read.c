/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "mm_read.h"
#include "gtmimagename.h"

GBLREF	sgmnt_addrs 	*cs_addrs;
GBLREF	unsigned char	rdfail_detail;

sm_uc_ptr_t mm_read(block_id blk)
{
	/* --- extended or dse (dse is able to edit any header fields freely) --- */
	assert((cs_addrs->total_blks <= cs_addrs->ti->total_blks) || !IS_MCODE_RUNNING);
	assert(blk >= 0);

	INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_dsk_read, 1);
	if (blk < cs_addrs->total_blks) 		/* use the private copy of total_blks */
		return (MM_BASE_ADDR(cs_addrs) + (off_t)cs_addrs->hdr->blk_size * blk);

	rdfail_detail = (blk < cs_addrs->ti->total_blks) ? cdb_sc_helpedout : cdb_sc_blknumerr;
	return (sm_uc_ptr_t)NULL;
}
