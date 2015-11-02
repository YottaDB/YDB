/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "dse_is_blk_free.h"
#include "t_qread.h"

GBLREF sgmnt_addrs	*cs_addrs;

bool dse_is_blk_free (block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr)
{
	uint4		status;
	uint4		index, offset;
	sm_uc_ptr_t	bp;
	error_def(ERR_DSEBLKRDFAIL);

	index =  (blk / cs_addrs->hdr->bplmap) * cs_addrs->hdr->bplmap;
	offset = blk - index;

	if(!(bp = t_qread (index, cycle, cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);

	status = dse_lm_blk_free(offset * BML_BITS_PER_BLK, bp + SIZEOF(blk_hdr));
	if (0 == status)	/* status == 00 => blk is busy */
		return FALSE;
	else 			/* status == 01 and 11 => blk is free.
				Also illegal value is considered as free  */
		return TRUE;
}
