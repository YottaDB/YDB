/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rms.h>
#ifdef DEBUG
#include "gtm_stdio.h"
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbgtr.h"
#include "gdsfhead.h"
#include <iodef.h>
#include "efn.h"
#include <ssdef.h>
#include "iosb_disk.h"
#include "iosp.h"
#include "shmpool.h"
#include "filestruct.h"
#include "gtm_malloc.h"		/* for CHECK_CHANNEL_STATUS macro */
#include "memcoherency.h"
#include "gds_blk_downgrade.h"
#include "gdsbml.h"

/***********************************************************************************
 * WARNING: This routine does not manage the number of outstanding AST available.
 * The calling routine is responsible for that.
 ***********************************************************************************/

GBLREF	sm_uc_ptr_t	reformat_buffer;
GBLREF	int		reformat_buffer_len;
GBLREF	volatile int	reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLREF	volatile int4	fast_lock_count;
GBLREF	boolean_t	dse_running;

int4	dsk_write(gd_region *reg, block_id blk, cache_rec_ptr_t cr, void (*ast_rtn)(), int4 ast_param, io_status_block_disk *iosb)
{
	int4			size, status;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	boolean_t		asyncIO;
	sm_uc_ptr_t		buff;
	uint4			channel_id;
	DEBUG_ONLY(
		boolean_t	reformat;
		blk_hdr_ptr_t	blk_hdr;
	)

	csa = &((vms_gds_info*)(reg->dyn.addr->file_cntl->file_info))->s_addrs;
	csd = csa->hdr;
	assert(NULL != csd);
	assert(cr);
	assert(cr->buffaddr);
	assert(blk == cr->blk);
	buff = GDS_ANY_REL2ABS(csa, cr->buffaddr);
	DEBUG_ONLY(
		/* Check GDS block that is about to be written. Dont do this for DSE as it may intentionally create bad blocks */
		if (!dse_running)
		{
			blk_hdr = (blk_hdr_ptr_t)buff;
			assert((unsigned)GDSVLAST > (unsigned)blk_hdr->bver);
			assert((LCL_MAP_LEVL == blk_hdr->levl) || ((unsigned)MAX_BT_DEPTH > (unsigned)blk_hdr->levl));
			assert((unsigned)csd->blk_size >= (unsigned)blk_hdr->bsiz);
			assert(csd->trans_hist.curr_tn >= blk_hdr->tn);
		}
	)
	assert(((blk_hdr_ptr_t)buff)->bver);	/* GDSV4 (0) version uses this field as a block length so should always be > 0 */
	asyncIO = TRUE;
	DEBUG_ONLY(reformat = FALSE);
	if (IS_GDS_BLK_DOWNGRADE_NEEDED(cr->ondsk_blkver))
	{	/* Need to downgrade/reformat this block back to a previous format.
		 * Ask for a buffer from our shared memory pool buffer manager.
		 */
		DEBUG_ONLY(reformat = TRUE);
		sblkh_p = shmpool_blk_alloc(reg, SHMBLK_REFORMAT);
		if ((shmpool_blk_hdr_ptr_t)-1 == sblkh_p)
		{	/* We weren't able to get a reformat block from the pool for async IO
			   use so we will have to make do with a static buffer and a synchronous IO.
			*/
			DEBUG_DYNGRD_ONLY(PRINTF("DSK_WRITE: Block %d being dynamically downgraded on write (syncIO)\n", blk));
			asyncIO = FALSE;
			assert(0 <= fast_lock_count);
			++fast_lock_count; 	/* Prevents interrupt from using reformat buffer while we have it */
			/* reformat_buffer_in_use should always be incremented only AFTER incrementing fast_lock_count
			 * as it is the latter that prevents interrupts from using the reformat buffer. Similarly
			 * the decrement of fast_lock_count should be done AFTER decrementing reformat_buffer_in_use.
			 */
			assert(0 == reformat_buffer_in_use);
			DEBUG_ONLY(reformat_buffer_in_use++;)
			if (csd->blk_size > reformat_buffer_len)
			{	/* Buffer not big enough (or does not exist) .. get a new one releasing old if it exists */
				if (reformat_buffer)
					free(reformat_buffer);
				reformat_buffer = malloc(csd->blk_size);
				reformat_buffer_len = csd->blk_size;
			}
			gds_blk_downgrade((v15_blk_hdr_ptr_t)reformat_buffer, (blk_hdr_ptr_t)buff);
			buff = reformat_buffer;
			BG_TRACE_PRO_ANY(csa, dwngrd_refmts_syncio);
		} else
		{	/* We have a reformat buffer. Make the necessary attachments, and mark it valid */
			DEBUG_DYNGRD_ONLY(PRINTF("DSK_WRITE: Block %d being dynamically downgraded on write (asyncIO)\n", blk));
			/* note (sblkh_p + 1) is shorthand to address data portion of blk following header */
			gds_blk_downgrade((v15_blk_hdr_ptr_t)(sblkh_p + 1), (blk_hdr_ptr_t)buff);
			buff = (v15_blk_hdr_ptr_t)(sblkh_p + 1);
			sblkh_p->use.rfrmt.cr_off = GDS_ANY_ABS2REL(csa, cr);
			sblkh_p->use.rfrmt.cycle = cr->cycle;
			sblkh_p->blkid = blk;
			/* Note that this is updating this offset field in the cache record outside of crit but since this
			   field is not govered by crit, this should not be an issue as the updates to the block are always
			   well sequenced (here and in wcs_wtfini()). Also the subsequent write barriers will make sure to
			   broadcast this change to all processors.
			*/
			cr->shmpool_blk_off = GDS_ANY_ABS2REL(csa, sblkh_p);
			/* Need a write coherency fence here as we want to make sure the above info is stored and
			   reflected to other processors before we mark the block valid.
			*/
			SHM_WRITE_MEMORY_BARRIER;
			sblkh_p->valid_data = TRUE;
			BG_TRACE_PRO_ANY(csa, dwngrd_refmts_asyncio);
		}
		size = ((v15_blk_hdr_ptr_t)buff)->bsiz;
		assert(size <= csd->blk_size - SIZEOF(blk_hdr) + SIZEOF(v15_blk_hdr));
		size = (size + 1) & ~1;
		assert(SIZEOF(v15_blk_hdr) <= size);
	} else DEBUG_ONLY(if (GDSV6 == cr->ondsk_blkver))
	{
		size = (((blk_hdr_ptr_t)buff)->bsiz + 1) & ~1;
		assert(SIZEOF(blk_hdr) <= size);
	}
	DEBUG_ONLY(else GTMASSERT);
	if (csa->do_fullblockwrites)
		/* round size up to next full logical filesys block. */
		size = ROUND_UP(size, csa->fullblockwrite_len);
	assert(size <= csd->blk_size);
	assert(FALSE == reg->read_only);
	assert(dba_bg == reg->dyn.addr->acc_meth);
	assert((size / 2 * 2) == size);
	assert(0 == (((uint4) buff) & 7));	/* some disk controllers require quadword aligned xfers */
	assert(!csa->acc_meth.bg.cache_state->cache_array || buff != csd);
	/* if doing a reformat, the buffer is not in cache so points to a local reformat buffer so the check below is superfluous */
	assert(reformat || !csa->acc_meth.bg.cache_state->cache_array
	       || (buff >= (unsigned char *)csa->acc_meth.bg.cache_state->cache_array
		   + (SIZEOF(cache_rec) * (csd->bt_buckets + csd->n_bts))));
	assert(buff < (sm_uc_ptr_t)csd || reformat);	/* assumes hdr follows immediately after the buffer pool in shared memory */
	assert(size <= csd->blk_size);
	channel_id = ((vms_gds_info*)(reg->dyn.addr->file_cntl->file_info))->fab->fab$l_stv;
	if (asyncIO)
	{
		status = sys$qio(efn_bg_qio_write, channel_id, IO$_WRITEVBLK, iosb ,ast_rtn, ast_param, buff, size,
				 (csd->blk_size / DISK_BLOCK_SIZE) * blk + csd->start_vbn, 0, 0, 0);
		CHECK_CHANNEL_STATUS(status, channel_id);
	} else
	{
		status = sys$qiow(efn_bg_qio_write, channel_id, IO$_WRITEVBLK, iosb ,ast_rtn, ast_param, buff, size,
				  (csd->blk_size / DISK_BLOCK_SIZE) * blk + csd->start_vbn, 0, 0, 0);
		CHECK_CHANNEL_STATUS(status, channel_id);
		DEBUG_ONLY(reformat_buffer_in_use--;)
		assert(0 == reformat_buffer_in_use);
		--fast_lock_count;
		assert(0 <= fast_lock_count);
		/* Since we are doing synchroous IO here, if we also happen to already hold crit, then
		   invoke wcs_wtfini() to handle completion of that IO. Else it will have to be handled
		   elsewhere as grabbing crit here could create a deadlock situation.
		*/
		if (csa->now_crit)
			wcs_wtfini(reg);
	}
	return status;
}
