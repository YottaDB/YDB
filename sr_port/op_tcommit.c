/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "gdsbml.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "min_max.h"		/* needed for MM extend */
#include "gdsblkops.h"		/* needed for MM extend */

#include "tp.h"
#include "tp_frame.h"
#include "copy.h"
#include "interlock.h"

/* Include proto-types.. */
#include "t_qread.h"
#include "tp_timeout.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "gvcst_expand_free_subtree.h"
#include "format_targ_key.h"
#include "bm_getfree.h"
#include "tp_unwind.h"
#include "wcs_mm_recover.h"
#include "add_inter.h"
#include "tp_incr_commit.h"

error_def(ERR_GBLOFLOW);
error_def(ERR_GVIS);
error_def(ERR_TLVLZERO);
error_def(ERR_GVKILLFAIL);
error_def(ERR_TPRETRY);

GBLREF  short                   dollar_tlevel, dollar_trestart;
GBLREF  jnl_fence_control       jnl_fence_ctl;
GBLREF  tp_frame                *tp_pointer;
GBLREF  gd_region               *gv_cur_region;
GBLREF  sgmnt_addrs             *cs_addrs;
GBLREF  sgmnt_data_ptr_t        cs_data;
GBLREF  sgm_info                *first_sgm_info, *sgm_info_ptr;
GBLREF  char                    *update_array, *update_array_ptr;
GBLREF  unsigned char           rdfail_detail;
GBLREF  cw_set_element          cw_set[];
GBLREF  gd_addr                 *gd_header;
GBLREF  bool                    tp_kill_bitmaps;
GBLREF	gv_namehead		*gv_target;
GBLREF  unsigned char           t_fail_hist[CDB_MAX_TRIES];
GBLREF  unsigned int            t_tries;
GBLREF  boolean_t               is_updproc;
GBLREF	void			(*tp_timeout_clear_ptr)(void);


void    op_tcommit(void)
{
	boolean_t		wait_for_jnl_hard;
        bool                    blk_used, is_mm;
        sm_uc_ptr_t             bmp;
        unsigned char           buff[MAX_KEY_SZ * 2], *end, tp_bat[TP_BATCH_LEN];
        unsigned int            ctn;
        int                     cw_depth, cycle, len, old_cw_depth;
        sgmnt_addrs             *csa, *next_csa;
        sgmnt_data_ptr_t        csd;
        sgm_info                *si, *temp_si;
        enum cdb_sc             status;
        cw_set_element          *cse, *last_cw_set_before_maps, *csetemp, *first_cse;
        blk_ident               *blk, *blk_top, *next_blk;
        block_id                bit_map, next_bm, new_blk, temp_blk;
        cache_rec_ptr_t         cr;
	kill_set		*ks;
	off_chain		chain1;
	/* for MM extend */
	cw_set_element          *update_cse;
	blk_segment             *seg, *stop_ptr, *array;
	sm_long_t               delta;
	sm_uc_ptr_t             old_db_addrs[2];
	srch_blk_status         *t1;

        if (0 == dollar_tlevel)
                rts_error(VARLSTCNT(1) ERR_TLVLZERO);
        assert(0 == jnl_fence_ctl.level);
        status = cdb_sc_normal;
        tp_kill_bitmaps = FALSE;

        if (1 == dollar_tlevel)					/* real commit */
        {
                if (NULL != first_sgm_info)
                {
                        for (temp_si = si = first_sgm_info; (cdb_sc_normal == status) && (NULL != si);  si = si->next_sgm_info)
                        {
                                sgm_info_ptr = si;              /* for t_qread (at least) */
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
                                csa = cs_addrs;
                                csd = cs_data;
				csa->jnl_before_image = csd->jnl_before_image;
                                is_mm = dba_mm == cs_addrs->hdr->acc_meth;
                                si->cr_array_index = 0;
				if (!is_mm && si->cr_array_size < (si->num_of_blks + (si->cw_set_depth * 2)))
				{
					/* reallocate a bigger cr_array. We need atmost read-set (si->num_of_blks) +
					 * write-set (si->cw_set_depth) + bitmap-write-set (a max. of si->cw_set_depth)
					 */
					free(si->cr_array);
					si->cr_array_size = si->num_of_blks + (si->cw_set_depth * 2);
					si->cr_array = (cache_rec_ptr_ptr_t)malloc(sizeof(cache_rec_ptr_t) * si->cr_array_size);
				}
                                assert(!is_mm || (0 == si->cr_array_size && NULL == si->cr_array));
                                if (NULL != si->first_cw_set)
                                {
                                        assert(0 != si->cw_set_depth);
                                        cw_depth = si->cw_set_depth;
                                        /* The following section allocates new blocks required by the transaction
                                         * it is done before going crit in order to reduce the change of having to
					 * wait on a read while crit. The trade-off is that if a newly allocated
					 * block is "stolen," it will cause a restart.
					 */
                                        last_cw_set_before_maps = si->last_cw_set;
                                        for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
                                        {
						TRAVERSE_TO_LATEST_CSE(cse);
                                                if (gds_t_create == cse->mode)
                                                {
							old_cw_depth = si->cw_set_depth;
							old_db_addrs[0] = csa->db_addrs[0];
							old_db_addrs[1] = csa->db_addrs[1];
							first_cse = si->first_cw_set;
							TRAVERSE_TO_LATEST_CSE(first_cse);
							while (FILE_EXTENDED == (new_blk = bm_getfree(cse->blk, &blk_used,
								cw_depth, first_cse, &si->cw_set_depth)))
							{
								assert(is_mm);
								wcs_mm_recover(si->gv_cur_region);
								delta = (sm_uc_ptr_t)csa->hdr - (sm_uc_ptr_t)csd;
								csd = csa->hdr;
								/* update cse's update array and old_block */
								for (update_cse = si->first_cw_set;  NULL != update_cse;
									update_cse = update_cse->next_cw_set)
								{
									TRAVERSE_TO_LATEST_CSE(update_cse);
									if (gds_t_writemap != update_cse->mode)
									{
										array = (blk_segment *)update_cse->upd_addr;
										stop_ptr = update_cse->first_copy ?
												array : array + 1;
										seg = (blk_segment *)array->addr;
										while (seg != stop_ptr)
										{
											if ((old_db_addrs[0] < seg->addr)
											&& (old_db_addrs[1] >= seg->addr))
												seg->addr += delta;
											seg--;
										}
									}
									if (NULL != update_cse->old_block)
									{
										assert((old_db_addrs[0] < update_cse->old_block) &&
											(old_db_addrs[1] > update_cse->old_block));
										update_cse->old_block += delta;
									}
								}
								/* update all the tp_hist */
								for (t1 = si->first_tp_hist;  t1 != si->last_tp_hist; t1++)
								{
									if ((old_db_addrs[0] < t1->buffaddr)
										&& (old_db_addrs[1] >= t1->buffaddr))
										t1->buffaddr += delta;
								}
							}
							if (0 > new_blk)
							{
								GET_CDB_SC_CODE(new_blk, status); /* code is set in status */
								t_fail_hist[t_tries] = status;
								TP_RETRY_ACCOUNTING(csd, status);
								break;  /* transaction must attempt restart */
							}
							if (JNL_ENABLED(csd) && csa->jnl_before_image && (blk_used))
							{
								cse->old_block = t_qread(new_blk,
										(sm_int_ptr_t)&cse->cycle, &cse->cr);
								if (NULL == cse->old_block)
								{
									status = rdfail_detail;
									t_fail_hist[t_tries] = status;
									TP_RETRY_ACCOUNTING(csd, status);
									break;
								}
							} else
								cse->old_block = NULL;
							cse->blk = new_blk;
							cse->mode = gds_t_acquired;
                                                        assert(CDB_STAGNATE > t_tries ||
								(is_mm ? (cse->blk < csa->total_blks)
										: (cse->blk < csa->ti->total_blks)));
                                                }       /* if (gds_t_create == cse->mode) */
                                        }       /* for (all cw_set_elements) */
                                        si->first_cw_bitmap = last_cw_set_before_maps->next_cw_set;
					if (cdb_sc_normal == status && 0 != csd->dsid)
					{
						for (ks = si->kill_set_head; NULL != ks; ks = ks->next_kill_set)
						{
							if (ks->used)
							{
								tp_kill_bitmaps = TRUE;
								break;
							}
						}
					}
					if (NULL != si->kill_set_head)
					{
						/* temp_si is to maintain index into sgm_info_ptr list till which DECR_CNTs
						 * have to be done incase abnormal status or tp_tend fails/succeeds
						 */
						temp_si = si->next_sgm_info;
					}
                                } else  /* if (at least one set in segment) */
                                        assert(0 == si->cw_set_depth);
                        }       /* for (all segments in the transaction) */
                        if ((cdb_sc_normal == status) && tp_tend(FALSE))
                                ;
                        else    /* commit failed */
                        {
				assert(cdb_sc_normal != t_fail_hist[t_tries]);	/* else will go into an infinite try loop */
				DEBUG_ONLY(
					for (si = first_sgm_info;  si != temp_si; si = si->next_sgm_info)
						assert(!si->kip_incremented);
				)
                                if (cdb_sc_gbloflow == status)
                                {
                                        if (NULL == (end = format_targ_key(buff, MAX_KEY_SZ * 2, cse->blk_target->last_rec, TRUE)))
                                                end = &buff[MAX_KEY_SZ * 2 - 1];
                                        rts_error(VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
                                } else
                                        INVOKE_RESTART;
                                return;
                        }
			if ((sgmnt_addrs *)-1 != (csa = jnl_fence_ctl.fence_list))
			{
				wait_for_jnl_hard = TRUE;
				if ((TP_BATCH_SHRT == tp_pointer->trans_id.str.len)
					|| (TP_BATCH_LEN == tp_pointer->trans_id.str.len))
				{
					lower_to_upper(tp_bat, (uchar_ptr_t)tp_pointer->trans_id.str.addr,
							tp_pointer->trans_id.str.len);
					if (0 == memcmp(TP_BATCH_ID, tp_bat, tp_pointer->trans_id.str.len))
						wait_for_jnl_hard = FALSE;
				}
	       	                for (;  (sgmnt_addrs *)-1 != csa;  csa = next_csa)
        	       	        {	/* only those regions that are actively journaling will appear in the list: */
					if (wait_for_jnl_hard)
					{
						TP_CHANGE_REG_IF_NEEDED(csa->jnl->region);
						if (!is_updproc)
							jnl_wait(csa->jnl->region);
					}
        		                next_csa = csa->next_fenced;
                		        csa->next_fenced = NULL;
	                        }
			}
                }       /* if (database work in the transaction) */
                /* Commit was successful */
                dollar_trestart = 0;
                /* the following section is essentially deferred garbage collection, freeing release block a bitmap at a time */
                if (NULL != first_sgm_info)
                {
                        for (si = first_sgm_info;  si != temp_si;  si = si->next_sgm_info)
                        {
				if (NULL == si->kill_set_head)
				{
					assert(!si->kip_incremented);
                                        continue;       /* no kills in this segment */
				}
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
				sgm_info_ptr = si;	/* needed in gvcst_expand_free_subtree */
				gvcst_expand_free_subtree(si->kill_set_head);
				assert(NULL != si->kill_set_head);
				DECR_KIP(cs_data, cs_addrs, si->kip_incremented);
                        }       	/* for (all segments in the transaction) */
			assert(NULL == temp_si || NULL == si->kill_set_head);
                }       /* if (kills in the transaction) */
                tp_clean_up(FALSE);     /* Not the rollback type of cleanup */
		if (gv_target != NULL)
		{
			TP_CHANGE_REG_IF_NEEDED(gv_target->gd_reg);
		} else
		{
			gv_cur_region = NULL;
			cs_addrs = (sgmnt_addrs *)0;
			cs_data = (sgmnt_data_ptr_t)0;
		}
        } else       /* an intermediate commit */
		tp_incr_commit();
        assert(0 < dollar_tlevel);
	/* Cancel or clear any pending TP timeout. */
	(*tp_timeout_clear_ptr)();
        tp_unwind(dollar_tlevel - 1, FALSE);
}
