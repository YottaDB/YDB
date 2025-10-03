/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"

GBLREF sgmnt_addrs	*cs_addrs;

error_def(ERR_DSEBLKRDFAIL);

boolean_t dse_is_blk_free (block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr)
{
	sm_uc_ptr_t	bp;
	int4		status;
	block_id	index, offset;

	index = (blk / cs_addrs->hdr->bplmap) * cs_addrs->hdr->bplmap;
	offset = blk - index;
	if (!(bp = t_qread (index, cycle, cr)))
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1, index);
	assert(offset == (int4)offset); /* offset is an index to an lmap and should never be larger then BLKS_PER_LMAP */
	status = dse_lm_blk_free((int4)offset, bp + SIZEOF(blk_hdr));
	return (0 != status);	/* status == 00 => busy;  01, 11, 10 (not currently legal) => free */
}
