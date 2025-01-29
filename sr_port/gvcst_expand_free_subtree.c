/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"		/* atleast for cdb_sc_* codes */
#include "copy.h"		/* atleast for the GET_USHORT macros */
#include "gdsroot.h"		/* atleast for gds_file_id used by sgmnt_data in gdsfhead.h */
#include "gdskill.h"		/* atleast for the kill_set and blk_ident structures */
#include "gdsblk.h"		/* atleast for the blk_hdr and rec_hdr structures */
#include "gtm_facility.h"	/* atleast for gdsfhead.h */
#include "fileinfo.h"		/* atleast for gdsfhead.h */
#include "gdsbt.h"		/* atleast for gdsfhead.h */
#include "gdsfhead.h"		/* atleast for cs_addrs, cs_data etc. */
#include "filestruct.h"		/* atleast for the FILE_INFO macro */
#include "gdscc.h"		/* atleast for cw_set_element in tp.h */
#include "jnl.h"		/* atleast for tp.h */
#include "buddy_list.h"		/* atleast for tp.h */
#include "tp.h"			/* atleast for off_chain */
#include "t_qread.h"
#include "gvcst_bmp_mark_free.h"
#include "gvcst_delete_blk.h"
#include "gvcst_kill_sort.h"
#include "gvcst_expand_free_subtree.h"
#include "rc_cpt_ops.h"
#include "wcs_phase2_commit_wait.h"
#include "min_max.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			bml_save_dollar_tlevel;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			update_trans;
GBLREF	unsigned char		rdfail_detail;

error_def(ERR_GVKILLFAIL);
error_def(ERR_IGNBMPMRKFREE);

void	gvcst_expand_free_subtree(kill_set *ks_head)
{
	blk_hdr_ptr_t		bp;
	blk_ident		*ksb;
	block_id		blk, temp_blk;
	boolean_t		flush_cache = FALSE, was_crit, long_blk_id;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr;
	inctn_opcode_t		save_inctn_opcode;
	int4			cnt, cycle, kill_error, blk_id_sz;
	kill_set		*ks;
	off_chain		chain;
	rec_hdr_ptr_t		rp, rp1, rtop;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	trans_num		ret_tn;
	uint4			bsiz, level, save_update_trans;
	unsigned char		temp_buff[MAX_DB_BLK_SIZE];
	unsigned short		temp_ushort;
#	ifdef DEBUG
	uint4			save_dollar_tlevel;
#	endif

	csa = cs_addrs;
	csd = cs_data;
	/* If ever the following assert is removed, "flush_cache" shouldn't be set to FALSE unconditionally as it is now */
	assert(!csd->dsid);	/* see related comment in gvcst_kill before the call to this routine */
	assert(MAX_DB_BLK_SIZE >= cs_data->blk_size);
	for (ks = ks_head; NULL != ks; ks = ks->next_kill_set)
	{
		for (cnt = 0; cnt < ks->used; ++cnt)
		{
			ksb = &ks->blk[cnt];
			if (0 != ksb->level)
			{
				if (!(was_crit = csa->now_crit)) /* needed so t_qread does not return NULL below */
					grab_crit_encr_cycle_sync(gv_cur_region, WS_13);
#				ifdef UNIX
				if (csa->onln_rlbk_cycle != csa->nl->onln_rlbk_cycle)
				{	/* Concurrent online rollback. We don't want to continue with rest of the logic to add more
					 * blocks to the kill-set and do the gvcst_bmp_mark_free. Return to the caller. Since we
					 * haven't sync'ed the cycles, the next tranasction commit will detect the online rollback
					 * and the restart logic will handle it appropriately.
					 */
					rel_crit(gv_cur_region);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_IGNBMPMRKFREE, 4, REG_LEN_STR(gv_cur_region),
							DB_LEN_STR(gv_cur_region));
					return;
				}
#				endif
				if (dollar_tlevel && ksb->flag)
				{
					chain.flag = 1;
					chain.next_off = 0;
					assert(ksb->block < (1LL << CW_INDEX_MAX_BITS));
					chain.cw_index = ksb->block;
					assert(SIZEOF(chain) == SIZEOF(blk));
					blk = ((block_ref *)&chain)->id;
				} else
					blk = ksb->block;
				if (!(bp = (blk_hdr_ptr_t)t_qread(blk, (sm_int_ptr_t)&cycle, &cr)))
				{	/* This should have worked because t_qread was done in crit */
					RTS_ERROR_CSA_ABT(csa, VARLSTCNT(4) ERR_GVKILLFAIL, 2, 1, &rdfail_detail);
				}
				long_blk_id = IS_64_BLK_ID(bp);
				blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
				if (NULL != cr)
				{	/* It is possible that t_qread returned a buffer from first_tp_srch_status.
					 * In that case, t_qread does not wait for cr->in_tend to be zero since
					 * there is no need to wait as long as all this is done inside of the TP
					 * transaction. But the gvcst_expand_free_subtree logic is special in that it
					 * is done AFTER the TP transaction is committed but with dollar_tlevel still
					 * set to non-zero. So it is possible that cr->in_tend is non-zero in this case.
					 * Hence we need to check if cr->in_tend is non-zero and if so wait for commit
					 * to complete before scanning the block for child-block #s to free.
					 */
					if (dollar_tlevel && cr->in_tend)
						wcs_phase2_commit_wait(csa, cr);
					assert(!cr->twin || cr->bt_index);
					assert((NULL == (bt = bt_get(blk)))
						|| (CR_NOTVALID == bt->cache_index)
						|| (cr == (cache_rec_ptr_t)GDS_REL2ABS(bt->cache_index)) && (0 == cr->in_tend));
				}
				assert(MAX_DB_BLK_SIZE >= bp->bsiz);
				bsiz = MIN(bp->bsiz, MAX_DB_BLK_SIZE);	/* avoid buffer overflows */
				memcpy(temp_buff, bp, bsiz);
				if (!was_crit)
					rel_crit(gv_cur_region);
				for (rp = (rec_hdr_ptr_t)(temp_buff + SIZEOF(blk_hdr)), rtop = (rec_hdr_ptr_t)(temp_buff + bsiz);
					rp < rtop; rp = rp1)
				{
					GET_USHORT(temp_ushort, &rp->rsiz);
					rp1 = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + temp_ushort);
					if ((sm_uc_ptr_t)rp1 < ((sm_uc_ptr_t)(rp + 1) + blk_id_sz))
					{	/* This should have worked because a local copy was made while crit */
						assert(FALSE);
						kill_error = cdb_sc_rmisalign;
						RTS_ERROR_CSA_ABT(csa, VARLSTCNT(4) ERR_GVKILLFAIL, 2, 1, &kill_error);
					}
					READ_BLK_ID(long_blk_id, &temp_blk, (sm_uc_ptr_t)rp1 - blk_id_sz);
					if (dollar_tlevel)
					{
						assert(sgm_info_ptr->tp_csa == cs_addrs);
						chain = ((block_ref *)&temp_blk)->chain;
						assert((SIZEOF(int) * 8) >= CW_INDEX_MAX_BITS);
						assertpro(!((1 == chain.flag) &&
							((int)chain.cw_index >= sgm_info_ptr->cw_set_depth)));
						assert(chain.flag || (temp_blk < csa->ti->total_blks));
					}
					level = ((blk_hdr_ptr_t)temp_buff)->levl;
					gvcst_delete_blk(temp_blk, level - 1, TRUE);
					if ((1 == level) && !dollar_tlevel && cs_data->dsid && !flush_cache)
						rc_cpt_entry(temp_blk);	/* Invalidate single block */
				}
				ksb->level = 0;
			} else
			{
				if (!dollar_tlevel && cs_data->dsid && !flush_cache)
					rc_cpt_entry(ksb->block);
			}
		}
		gvcst_kill_sort(ks);
		assert(!bml_save_dollar_tlevel);
		DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
		bml_save_dollar_tlevel = dollar_tlevel;
		/* Resetting and restoring of update_trans is necessary to avoid blowing an assert in t_begin that it is 0. */
		save_update_trans = update_trans;
		assert(1 >= dollar_tlevel);
		dollar_tlevel = 0;	/* temporarily for gvcst_bmp_mark_free */
		update_trans = 0;
		GVCST_BMP_MARK_FREE(ks, ret_tn, inctn_invalid_op, inctn_bmp_mark_free_gtm, inctn_opcode, csa)
		update_trans = save_update_trans;
		dollar_tlevel = bml_save_dollar_tlevel;
		assert(dollar_tlevel == save_dollar_tlevel);
		bml_save_dollar_tlevel = 0;
	}
}
