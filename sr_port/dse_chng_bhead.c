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

#include "gtm_time.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "cli.h"
#include "send_msg.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "process_deferred_stale.h"
#include "util.h"
#include "t_abort.h"
#include "gvcst_blk_build.h"	/* for the BUILD_AIMG_IF_JNL_ENABLED macro */
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF	char			*update_array, *update_array_ptr;
GBLREF	uint4			update_array_size;
GBLREF	srch_hist		dummy_hist;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	block_id		patch_curr_blk;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gd_addr			*gd_header;
GBLREF	cache_rec		*cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	cw_set_element		cw_set[];

error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEFAIL);
error_def(ERR_DBRDONLY);

void dse_chng_bhead(void)
{
	block_id		blk;
	int4			x;
	trans_num		tn;
	cache_rec_ptr_t		cr;
	blk_hdr			new_hdr;
	blk_segment		*bs1, *bs_ptr;
	int4			blk_seg_cnt, blk_size;	/* needed for BLK_INIT,BLK_SEG and BLK_FINI macros */
	boolean_t		ismap;
	boolean_t		chng_blk;
	boolean_t		was_crit;
	boolean_t		was_hold_onto_crit;
	uint4			mapsize;
	srch_blk_status		blkhist;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
#	ifdef GTM_CRYPT
	int			req_enc_blk_size;
	int			gtmcrypt_errno;
	blk_hdr_ptr_t		bp, save_bp, save_old_block;
	gd_segment		*seg;
#	endif

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	chng_blk = FALSE;
	csa = cs_addrs;
	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
			return;
		if (blk < 0 || blk > csa->ti->total_blks)
		{
			util_out_print("Error: invalid block number.", TRUE);
			return;
		}
		patch_curr_blk = blk;
	}
	csd = csa->hdr;
	assert(csd == cs_data);
	blk_size = csd->blk_size;
	ismap = (patch_curr_blk / csd->bplmap * csd->bplmap == patch_curr_blk);
	mapsize = BM_SIZE(csd->bplmap);

	t_begin_crit(ERR_DSEFAIL);
	blkhist.blk_num = patch_curr_blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	new_hdr = *(blk_hdr_ptr_t)blkhist.buffaddr;

	if (cli_present("LEVEL") == CLI_PRESENT)
	{
		if (!cli_get_hex("LEVEL", (uint4 *)&x))
		{
			t_abort(gv_cur_region, csa);
			return;
		}
		if (ismap && (unsigned char)x != LCL_MAP_LEVL)
		{
			util_out_print("Error: invalid level for a bit map block.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
		if (!ismap && (x < 0 || x > MAX_BT_DEPTH + 1))
		{
			util_out_print("Error: invalid level.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
	 	new_hdr.levl = (unsigned char)x;

		chng_blk = TRUE;
		if (new_hdr.bsiz < SIZEOF(blk_hdr))
			new_hdr.bsiz = SIZEOF(blk_hdr);
		if (new_hdr.bsiz  > blk_size)
			new_hdr.bsiz = blk_size;
	}
	if (cli_present("BSIZ") == CLI_PRESENT)
	{
		if (!cli_get_hex("BSIZ", (uint4 *)&x))
		{
			t_abort(gv_cur_region, csa);
			return;
		}
		if (ismap && x != mapsize)
		{
			util_out_print("Error: invalid bsiz.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		} else if (x < SIZEOF(blk_hdr) || x > blk_size)
		{
			util_out_print("Error: invalid bsiz.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
		chng_blk = TRUE;
		new_hdr.bsiz = x;
	}
	if (!chng_blk)
		t_abort(gv_cur_region, csa);
	else
	{
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, blkhist.buffaddr + SIZEOF(new_hdr), new_hdr.bsiz - SIZEOF(new_hdr));
		if (!BLK_FINI(bs_ptr, bs1))
		{
			util_out_print("Error: bad block build.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
		t_write(&blkhist, (unsigned char *)bs1, 0, 0, new_hdr.levl, TRUE, FALSE, GDS_WRITE_KILLTN);
		BUILD_AIMG_IF_JNL_ENABLED(csd, csa->ti->curr_tn);
		t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);
	}
	if (cli_present("TN") == CLI_PRESENT)
	{
		if (!cli_get_hex64("TN", &tn))
			return;
		was_crit = csa->now_crit;
		t_begin_crit(ERR_DSEFAIL);
		CHECK_TN(csa, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
		assert(csa->ti->early_tn == csa->ti->curr_tn);
		if (NULL == (blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		{
			util_out_print("Error: Unable to read buffer.", TRUE);
			t_abort(gv_cur_region, csa);
			return;
		}
		if (new_hdr.bsiz < SIZEOF(blk_hdr))
			new_hdr.bsiz = SIZEOF(blk_hdr);
		if (new_hdr.bsiz  > blk_size)
			new_hdr.bsiz = blk_size;
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, blkhist.buffaddr + SIZEOF(new_hdr), new_hdr.bsiz - SIZEOF(new_hdr));
		BLK_FINI(bs_ptr, bs1);
		t_write(&blkhist, (unsigned char *)bs1, 0, 0,
			((blk_hdr_ptr_t)blkhist.buffaddr)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
		/* Pass the desired tn as argument to bg_update/mm_update below */
		BUILD_AIMG_IF_JNL_ENABLED(csd, tn);
		was_hold_onto_crit = csa->hold_onto_crit;
		csa->hold_onto_crit = TRUE; /* need this so t_end doesn't release crit (see below comment for why) */
		t_end(&dummy_hist, NULL, tn);
#		ifdef GTM_CRYPT
		if (csd->is_encrypted && (tn < csa->ti->curr_tn))
		{	/* BG and db encryption is enabled and the DSE update caused the block-header to potentially have a tn
			 * that is LESS than what it had before. At this point, the global buffer (corresponding to blkhist.blk_num)
			 * reflects the contents of the block AFTER the dse update (bg_update would have touched this) whereas
			 * the corresponding encryption global buffer reflects the contents of the block BEFORE the update.
			 * Normally wcs_wtstart takes care of propagating the tn update from the regular global buffer to the
			 * corresponding encryption buffer. But if before it gets a chance, let us say a process goes to t_end
			 * as part of a subsequent transaction and updates this same block. Since the  blk-hdr-tn potentially
			 * decreased, it is possible that the PBLK writing check (comparing blk-hdr-tn with the epoch_tn) decides
			 * to write a PBLK for this block (even though a PBLK was already written for this block as part of a
			 * previous DSE CHANGE -BL -TN in the same epoch). In this case, since the db is encrypted, the logic
			 * will assume there were no updates to this block since the last time wcs_wtstart updated the encryption
			 * buffer and therefore use that to write the pblk, which is incorrect since it does not yet contain the
			 * tn update. The consequence of this is would be writing an older before-image PBLK) record to the
			 * journal file. To prevent this situation, we update the encryption buffer here (before releasing crit)
			 * using logic like that in wcs_wtstart to ensure it is in sync with the regular global buffer. To ensure
			 * that t_end doesn't release crit, we set csa->hold_onto_crit to TRUE
			 * Note:
			 * Although we use cw_set[0] to access the global buffer corresponding to the block number being updated,
			 * cw_set_depth at this point is 0 because t_end resets it. This is considered safe since cw_set is a
			 * static array (as opposed to malloc'ed memory) and hence is always available and valid until it gets
			 * overwritten by subsequent updates.
			 */
			bp = (blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cw_set[0].cr->buffaddr);
			DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, (sm_uc_ptr_t)bp);
			save_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, csa);
			DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, (sm_uc_ptr_t)save_bp);
			assert((bp->bsiz <= csd->blk_size) && (bp->bsiz >= SIZEOF(*bp)));
			req_enc_blk_size = MIN(csd->blk_size, bp->bsiz) - SIZEOF(*bp);
			if (BLK_NEEDS_ENCRYPTION(bp->levl, req_enc_blk_size))
			{
				ASSERT_ENCRYPTION_INITIALIZED;
				memcpy(save_bp, bp, SIZEOF(blk_hdr));
				GTMCRYPT_ENCRYPT(csa, csa->encr_key_handle, (char *)(bp + 1), req_enc_blk_size,
							(char *)(save_bp + 1), gtmcrypt_errno);
				if (0 != gtmcrypt_errno)
				{
					seg = gv_cur_region->dyn.addr;
					GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
				}
			} else
				memcpy(save_bp, bp, bp->bsiz);
		}
#		endif
		csa->hold_onto_crit = was_hold_onto_crit;
		if (!was_crit)
			rel_crit(gv_cur_region);
		if (unhandled_stale_timer_pop)
			process_deferred_stale();
	}
	return;
}
