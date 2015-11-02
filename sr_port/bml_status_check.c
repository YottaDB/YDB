/****************************************************************
 *								*
 *	Copyright 2007, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdscc.h"
#include "mm_read.h"
#include "bml_status_check.h"

#define MAX_FFS_SIZE 32

GBLREF	sgmnt_addrs	*cs_addrs;

/* Checks that a block that we have acquired is marked RECYCLED/FREE in the database
 * and that an existing block that we are updating is marked BUSY in the database.
 * For BG, the check is done only if the bitmap block is available in the cache AND is not being concurrently updated.
 */
void	bml_status_check(cw_set_element *cs)
{
	block_id	bmlblk, blk;
	cache_rec_ptr_t	bmlcr;
	blk_hdr_ptr_t	bmlbuff;
	boolean_t	is_mm;
	int4		bml_status;

	is_mm = (dba_mm == cs_addrs->hdr->acc_meth);
	assert(gds_t_create != cs->mode);
	if ((gds_t_acquired == cs->mode) || (gds_t_write == cs->mode))
	{
		bmlblk = ROUND_DOWN2(cs->blk, BLKS_PER_LMAP);
		assert(IS_BITMAP_BLK(bmlblk));
		blk = cs->blk - bmlblk;
		assert(blk);
		if (!is_mm)
		{
			bmlcr = db_csh_get(bmlblk);
			bmlbuff = ((NULL != bmlcr) && (CR_NOTVALID != (sm_long_t)bmlcr)
					&& (0 > bmlcr->read_in_progress) && !bmlcr->in_tend)
				? (blk_hdr_ptr_t)GDS_REL2ABS(bmlcr->buffaddr)
				: NULL;
		} else
		{
			bmlbuff = (blk_hdr_ptr_t)mm_read(bmlblk);
			/* mm_read would have incremented the GVSTATS n_dsk_read counter. But we dont want that to happen
			 * because this function is invoked only in debug builds and yet we want the same counter
			 * value for both pro and dbg builds. So undo that action immediately.
			 */
			INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_dsk_read, (gtm_uint64_t)-1);/* note the -1 causes the undo */
			assert(NULL != bmlbuff);
		}
		if (NULL != bmlbuff)
		{
			assert(LCL_MAP_LEVL == bmlbuff->levl);
			assert(BM_SIZE(cs_addrs->hdr->bplmap) == bmlbuff->bsiz);
			GET_BM_STATUS(bmlbuff, blk, bml_status);
			assert(BLK_MAPINVALID != bml_status);
			assert((gds_t_acquired != cs->mode) || (BLK_BUSY != bml_status));
			assert((gds_t_acquired == cs->mode) || (BLK_BUSY == bml_status));
		}
	}
}
