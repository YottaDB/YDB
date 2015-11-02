/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "gdsbgtr.h"
#include "iosp.h"
#include "min_max.h"

/* Include prototypes */
#include "dse_simulate_t_end.h"
#include "wcs_flu.h"
#include "wcs_timer_start.h"
#include "mm_update.h"
#include "bg_update.h"
#include "jnl_write_aimg_rec.h"
#include "gvcst_blk_build.h"
#include "mupipbckup.h"
#include "jnl_write_pblk.h"
#include "jnl_get_checksum.h"

GBLREF	unsigned char		*non_tp_jfb_buff_ptr;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	boolean_t		dse_running;
GBLREF	boolean_t		write_after_image;
GBLREF	boolean_t		block_saved;
GBLREF	unsigned int		cr_array_index;
GBLREF	unsigned char		cw_set_depth;
GBLREF	cw_set_element   	cw_set[];
GBLREF	cache_rec		*cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			process_id;

/* A lot of this code is copied from t_end.c so any changes here need to be reflected there and vice versa */
void	dse_simulate_t_end(gd_region *reg, sgmnt_addrs *csa, trans_num ctn)
{
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	uint4			jnl_status;
	cw_set_element		*cse;
	uint4                   total_jnl_rec_size;
	boolean_t		is_mm;
	blk_hdr_ptr_t		old_block;
	sgm_info		*dummysi = NULL;
	unsigned int		bsiz;
	sgmnt_data_ptr_t	csd;
#	ifdef GTM_CRYPT
	int			req_enc_blk_size;
	int			crypt_status;
	blk_hdr_ptr_t		bp, save_bp, save_old_block;
#	endif

	error_def(ERR_JNLFLUSH);
	error_def(ERR_JNLEXTEND);
	error_def(ERR_BUFFLUFAILED);

	assert(dse_running && write_after_image);
	assert(1 == cw_set_depth);
	csd = csa->hdr;
	cse = (cw_set_element *)(&cw_set[0]);
	is_mm = (dba_mm == csd->acc_meth);
	cr_array_index = 0;
	block_saved = FALSE;
	if (JNL_ENABLED(csd))
	{
		SET_GBL_JREC_TIME;	/* needed for jnl_ensure_open, jnl_put_jrt_pini and jnl_write_aimg_rec */
		jpc = csa->jnl;
		jbp = jpc->jnl_buff;
		/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl
		 * records. This needs to be done BEFORE the jnl_ensure_open as that could write journal records
		 * (if it decides to switch to a new journal file)
		 */
		ADJUST_GBL_JREC_TIME(jgbl, jbp);
		jnl_status = jnl_ensure_open();
		if (0 == jnl_status)
		{
			cse->new_buff = non_tp_jfb_buff_ptr;
			gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, ctn);
			cse->done = TRUE;
			non_tp_jfb_ptr->record_size = 0;	/* used by the TOTAL_NONTPJNL_REC_SIZE macro */
			TOTAL_NONTPJNL_REC_SIZE(total_jnl_rec_size, non_tp_jfb_ptr, csa, cw_set_depth);
			if (DISK_BLOCKS_SUM(jbp->freeaddr, total_jnl_rec_size) > jbp->filesize)
			{	/* Moved as part of change to prevent journal records splitting
				 * across multiple generation journal files. */
				if (SS_NORMAL != (jnl_status = jnl_flush(reg)))
				{
					assert((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa)));
					rts_error(VARLSTCNT(5) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd), jnl_status);
				} else if (-1 == jnl_file_extend(jpc, total_jnl_rec_size))
				{
					assert(csd == csa->hdr); /* jnl_file_extend() shouldn't reset csd in MM */
					assert((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa)));
					rts_error(VARLSTCNT(4) ERR_JNLEXTEND, 2, JNL_LEN_STR(csd));
				}
				assert(csd == csa->hdr);	/* If MM, csd shouldn't have been reset */
			}
			if (0 == jpc->pini_addr)
				jnl_put_jrt_pini(csa);
			if (JNL_HAS_EPOCH(jbp)
				&& ((jbp->next_epoch_time <= jgbl.gbl_jrec_time) UNCONDITIONAL_EPOCH_ONLY(|| TRUE)))
			{	/* Flush the cache. Since we are in crit, defer syncing epoch */
				if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_IN_COMMIT))
				{
					SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_wcsflu);
					rts_error(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("DSE"), DB_LEN_STR(reg));
				}
			}
			assert(0 != jpc->pini_addr);	/* ensure wcs_flu above did not switch jnl file */
			if (jbp->before_images)
			{
				old_block = (blk_hdr_ptr_t)cse->old_block;
				assert(NULL != old_block);
				ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)old_block, csa);
				DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd);
				if (old_block->tn < jbp->epoch_tn)
				{
					assert(sizeof(bsiz) == sizeof(old_block->bsiz));
					bsiz = old_block->bsiz;
					/* It is possible that the block has a bad block-size.
					 * Before computing checksum ensure bsiz passed is safe.
					 */
					bsiz = MIN(bsiz, csd->blk_size);
					assert(!cse->blk_checksum
						|| (cse->blk_checksum == jnl_get_checksum((uint4 *)old_block, csa, bsiz)));
					if (!cse->blk_checksum)
						cse->blk_checksum = jnl_get_checksum((uint4 *)old_block, csa, bsiz);
#					ifdef GTM_CRYPT
					if (csd->is_encrypted)
					{
						DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, (sm_uc_ptr_t)old_block);
						DEBUG_ONLY(save_old_block = old_block;)
						old_block = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(old_block, csa);
						/* Ensure that the unencrypted block and it's twin counterpart are in sync */
						assert(save_old_block->tn == old_block->tn);
						assert(save_old_block->bsiz == old_block->bsiz);
						assert(save_old_block->levl == old_block->levl);
						DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, (sm_uc_ptr_t)old_block);
					}
#					endif
					jnl_write_pblk(csa, cse, old_block);
					cse->jnl_freeaddr = jbp->freeaddr;
				}
			}
			jnl_write_aimg_rec(csa, cse);
		} else
			rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
	}
	if (!is_mm)
	{
		bg_update(cse, csa->ti->curr_tn, ctn, dummysi);
#		ifdef GTM_CRYPT
		if (csd->is_encrypted && (ctn < csa->ti->curr_tn))
		{	/* BG and db encryption is enabled and the DSE update caused the block-header to potentially have a tn
			 * that is LESS than what it had before. At this point, the global buffer (corresponding to cse->blk)
			 * reflects the contents of the block AFTER the dse update (bg_update would have touched this) whereas
			 * the corresponding encryption global buffer reflects the contents of the block BEFORE the update.
			 * Normally wcs_wtstart takes care of propagating the tn update from the regular global buffer to the
			 * corresponding encryption buffer. But if before it gets a chance, let us say a process goes to t_end
			 * (or dse_simulate_t_end) as part of a subsequent transaction and updates this same block. Since the
			 * blk-hdr-tn potentially decreased, it is possible that the PBLK writing check (comparing blk-hdr-tn
			 * with the epoch_tn) decides to write a PBLK for this block (even though a PBLK was already written for
			 * this block as part of a previous DSE CHANGE -BL -TN in the same epoch). In this case, since the db is
			 * encrypted, the logic will assume there were no updates to this block since the last time wcs_wtstart
			 * updated the encryption buffer and therefore use that to write the pblk, which is incorrect since it
			 * does not yet contain the tn update. The consequence of this is would be writing an older before-image
			 * PBLK) record to the journal file. To prevent this situation, we update the encryption buffer here
			 * (before releasing crit) using logic like that in wcs_wtstart to ensure it is in sync with the regular
			 * global buffer.
			 */
			bp = (blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cse->cr->buffaddr);
			DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, (sm_uc_ptr_t)bp);
			save_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, csa);
			DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, (sm_uc_ptr_t)save_bp);
			req_enc_blk_size = bp->bsiz - (SIZEOF(*bp));
			if (BLK_NEEDS_ENCRYPTION(bp->levl, req_enc_blk_size))
			{
				ASSERT_ENCRYPTION_INITIALIZED;
				memcpy(save_bp, bp, sizeof(blk_hdr));
				GTMCRYPT_ENCODE_FAST(csa->encr_key_handle, (char *)(bp + 1), req_enc_blk_size,
						     (char *)(save_bp + 1), crypt_status);
				if (0 != crypt_status)
					GC_GTM_PUTMSG(crypt_status, reg->dyn.addr->fname);
			} else
				memcpy(save_bp, bp, bp->bsiz);
		}
#		endif
	} else
		mm_update(cse, csa->ti->curr_tn, ctn, dummysi);
	INCREMENT_CURR_TN(csd);
	/* the following code is analogous to that in t_end and should be maintained in a similar fashion */
	UNPIN_CR_ARRAY_ON_COMMIT(cr_array, cr_array_index);
	assert(!cr_array_index);
	cw_set_depth = 0;	/* signal end of active transaction to secshr_db_clnup/t_commit_clnup */
	if (block_saved)
		backup_buffer_flush(reg);
	wcs_timer_start(reg, TRUE);
	/* rel_crit is not done here. caller should take care of that and stale timer pop processing */
}
