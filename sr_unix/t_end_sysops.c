/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"		/* for GETENV */

#include <sys/ipc.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>     	/*  for strlen() in RTS_ERROR_TEXT macro */

#include "aswp.h"
#include "gtm_facility.h"
#include "gdsroot.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "cdb_sc.h"
#include "copy.h"
#include "filestruct.h"
#include "gt_timer.h"
#include "interlock.h"
#include "jnl.h"
#include "iosp.h"
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "gdsbgtr.h"
#include "gtmio.h"
#include "min_max.h"
#include "relqueopi.h"
#include "io.h" 		/* for gtmsecshr.h */
#include "gtmsecshr.h"
#include "sleep_cnt.h"
#include "performcaslatchcheck.h"
#include "mupipbckup.h"
#include "cache.h"
#include "gtmmsg.h"
#include "error.h"		/* for gtm_fork_n_core() prototype */

/* Include prototypes */
#include "util.h"
#include "send_msg.h"
#include "bit_set.h"
#include "bit_clear.h"
#include "gvcst_blk_build.h"
#include "gvcst_map_build.h"
#include "relqop.h"
#include "change_reg.h"
#include "is_proc_alive.h"
#include "is_file_identical.h"
#include "wcs_sleep.h"
#include "wcs_backoff.h"
#include "bm_update.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#include "caller_id.h"
#include "add_inter.h"
#include "gtmimagename.h"
#include "gtcm_jnl_switched.h"

GBLDEF	cache_rec_ptr_t		get_space_fail_cr;	/* gbldefed to be accessible in a pro core */

GBLREF volatile boolean_t	crit_in_flux;
GBLREF bool             	certify_all_blocks;
GBLREF bool             	run_time;
GBLREF uint4            	process_id;
GBLREF sgmnt_addrs      	*cs_addrs;
GBLREF sgmnt_data_ptr_t 	cs_data;
GBLREF gd_region        	*gv_cur_region;
GBLREF gv_namehead      	*gv_target;
GBLREF cw_set_element   	cw_set[];
GBLREF cache_rec_ptr_t  	cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF unsigned int     	cr_array_index;
GBLREF short   			dollar_tlevel;
GBLREF sgm_info         	*sgm_info_ptr;
GBLREF volatile int4 		fast_lock_count;
GBLREF bool             	unhandled_stale_timer_pop;
GBLREF boolean_t		*lseekIoInProgress_flags;
GBLREF boolean_t        	block_saved;
GBLREF boolean_t        	write_after_image;
GBLREF boolean_t        	dse_running;
GBLREF enum gtmImageTypes	image_type;
GBLREF boolean_t		gtm_environment_init;

#define MAX_CYCLES      2

                                /* both values lifted from relqueopi.c */
#define QI_RETRY        128

#ifdef QI_STARVATION
#  undef QI_STARVATION
#endif

#define QI_STARVATION   1000

#ifdef DEBUG_QUEUE
#define VERIFY_QUEUE(base)      verify_queue(base)
#define VERIFY_QUEUE_LOCK(base,latch) verify_queue_lock(base,latch)
#else
#define VERIFY_QUEUE(base)
#define VERIFY_QUEUE_LOCK(base,latch)
#endif

void    wcs_stale(TID tid, int4 hd_len, gd_region **region);

void fileheader_sync(gd_region *reg)
{
        size_t                  flush_len, sync_size;
        int4                    high_blk, save_errno;
        unix_db_info            *udi;
        sgmnt_addrs             *csa;
        sgmnt_data_ptr_t        csd;

        error_def(ERR_DBFILERR);
        error_def(ERR_TEXT);

        udi = FILE_INFO(reg);
        csa = &udi->s_addrs;
        csd = csa->hdr;
        assert(csa->now_crit);                                  /* only way high water mark code works is if in crit */
                                                                /* Adding lock code to it would remove this restriction */

        high_blk = csa->nl->highest_lbm_blk_changed;
        csa->nl->highest_lbm_blk_changed = -1;                      /* Reset to initial value */
        flush_len = SGMNT_HDR_LEN;
        if (0 <= high_blk)                                      /* If not negative, flush at least one map block */
                flush_len += ((high_blk / csd->bplmap / DISK_BLOCK_SIZE / BITS_PER_UCHAR) + 1) * DISK_BLOCK_SIZE;
#ifdef FULLBLOCKWRITES
	flush_len = ROUND_UP(flush_len, csd->blk_size);
#endif

        if (dba_mm != csd->acc_meth)
        {
                LSEEKWRITE(udi->fd, 0, (sm_uc_ptr_t)(csa->hdr), flush_len, save_errno);
                if (0 != save_errno)
                {
                        rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Error during FileHeader Flush"), save_errno);
                }
                return;
        }
        else
        {
#if defined(UNTARGETED_MSYNC)

                if (csa->ti->last_mm_sync != csa->ti->curr_tn)
                {
			sync_size = (size_t)ROUND_UP((size_t)csa->db_addrs[0] + flush_len, MSYNC_ADDR_INCS)
					- (size_t)csa->db_addrs[0];
			if (-1 == msync((caddr_t)csa->db_addrs[0], sync_size, MS_ASYNC))
                        {
                                rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file msync for fileheader"), errno);
                        }
                        csa->ti->last_mm_sync = csa->ti->curr_tn;       /* save when did last full sync */
                }
#elif defined(TARGETED_MSYNC)
                if (-1 == msync((caddr_t)csa->db_addrs[0],
                        (size_t)ROUND_UP(csa->db_addrs[0] + flush_len, MSYNC_ADDR_INCS), MS_ASYNC))
                {
                        rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file msync for fileheader"), errno);
                }
#else
                LSEEKWRITE(udi->fd, 0, csa->db_addrs[0], flush_len, save_errno);
                if (0 != save_errno)
                {
                        rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_TEXT("Error during FileHeader Flush"), save_errno);
                }
#endif
        }
}


/* update a bitmap */
void    bm_update(cw_set_element *cs, sm_uc_ptr_t lclmap, bool is_mm)
{
        int4            bml_full, total_blks, high_blk, bplmap;
        bool            blk_used;
        sgmnt_addrs     *csa;
        sgmnt_data_ptr_t        csd;

        csa = cs_addrs;                 /* Local access copies */
        csd = csa->hdr;

	assert(csa->now_crit);
        bplmap = csd->bplmap;
        if (((csa->ti->total_blks / bplmap) * bplmap) == cs->blk)
                total_blks = (csa->ti->total_blks - cs->blk);
        else
                total_blks = bplmap;
        csa->ti->free_blocks -= cs->reference_cnt;


        if (0 < cs->reference_cnt)
        {
                bml_full = bml_find_free(0,
                                         (sizeof(blk_hdr) + (is_mm ? lclmap :
                                                             ((sm_uc_ptr_t)GDS_REL2ABS(lclmap)))),
                                         total_blks, &blk_used);
                if (NO_FREE_SPACE == bml_full)
                {
                        bit_clear(cs->blk / bplmap, csd->master_map);

                        /* The following works while all uses of these fields are in crit */
                        if (cs->blk > csa->nl->highest_lbm_blk_changed)
                                csa->nl->highest_lbm_blk_changed = cs->blk;         /* Retain high-water mark */
                }
        } else
                if (FALSE == bit_set(cs->blk / bplmap, csd->master_map))
                {
                        /* The following works while all uses of these fields are in crit */
                        if (cs->blk > csa->nl->highest_lbm_blk_changed)
                                csa->nl->highest_lbm_blk_changed = cs->blk;         /* Retain high-water mark */
                }

        return;
}


enum cdb_sc     mm_update(cw_set_element *cs, cw_set_element *cs_top, trans_num ctn, trans_num effective_tn, sgm_info *si)
{
        off_chain       chain;
        cw_set_element  *cs_ptr, *nxt;
        sm_uc_ptr_t     chain_ptr, db_addr[2];
        bool            earlier_dirty = FALSE;

#if (!defined(UNTARGETED_MSYNC))
        mmblk_rec_ptr_t         mmblkr;
#endif

        error_def(ERR_DBFILERR);
        error_def(ERR_TEXT);

	INCR_DB_CSH_COUNTER(cs_addrs, n_bg_updates, 1);
        assert((0 <= cs->blk) && (cs->blk <= cs_addrs->ti->total_blks));
        db_addr[0] = cs_addrs->acc_meth.mm.base_addr + (sm_off_t)cs_data->blk_size * (cs->blk);

#if (!defined(UNTARGETED_MSYNC))
        if (0 < cs_data->defer_time)
        {
                int4                    lcnt, n, blk, blk_start, blk_end;
                uint4                   max_ent;

#if defined(TARGETED_MSYNC)
                sm_uc_ptr_t             desired_start, desired_end;

		desired_start = db_addr[0];
                desired_end = desired_start + (sm_off_t)(cs_data->blk_size) - 1;
                blk_start = DIVIDE_ROUND_DOWN(desired_start - cs_addrs->db_addrs[0], MSYNC_ADDR_INCS);
                blk_end = DIVIDE_ROUND_DOWN(desired_end - cs_addrs->db_addrs[0], MSYNC_ADDR_INCS);
#else
                blk_start = cs->blk;
                blk_end = cs->blk;
#endif
                /* Because of the calculations done above, blk_end - blk_start should be <= 1. But
                        still preserve the following loop for its generality */

                for (blk = blk_start; blk <= blk_end; blk++)
                {
                        mmblkr = (mmblk_rec_ptr_t)db_csh_get(blk);

                        if (NULL == mmblkr)
                        {
                                mmblk_rec_ptr_t         hdr, cur_mmblkr, start_mmblkr, q0;

                                max_ent = cs_addrs->hdr->n_bts;
                                cur_mmblkr = (mmblk_rec_ptr_t)GDS_REL2ABS(cs_addrs->nl->cur_lru_cache_rec_off);
                                hdr = cs_addrs->acc_meth.mm.mmblk_state->mmblk_array + (blk % cs_addrs->hdr->bt_buckets);
                                start_mmblkr = cs_addrs->acc_meth.mm.mmblk_state->mmblk_array + cs_addrs->hdr->bt_buckets;

                                for (lcnt = 0; lcnt <= (MAX_CYCLES * max_ent); )
                                {
                                        cur_mmblkr++;
                                        if (cur_mmblkr == start_mmblkr + max_ent)
                                        {
                                                cur_mmblkr = start_mmblkr;
                                        }

                                        if (TRUE == cur_mmblkr->refer)
                                        {
                                                lcnt++;
                                                cur_mmblkr->refer = FALSE;
                                                continue;
                                        }

                                        if (0 != cur_mmblkr->dirty)
                                        {
                                                wcs_get_space(gv_cur_region, 0, (cache_rec_ptr_t)cur_mmblkr);
                                        }

                                        cur_mmblkr->blk = blk;
                                        q0 = (mmblk_rec_ptr_t)((sm_uc_ptr_t)cur_mmblkr + cur_mmblkr->blkque.fl);
                                        shuffqth((que_ent_ptr_t)q0, (que_ent_ptr_t)hdr);
                                        cs_addrs->nl->cur_lru_cache_rec_off = GDS_ABS2REL(cur_mmblkr);

                                        earlier_dirty = FALSE;
                                        mmblkr = cur_mmblkr;
					/* Here we cannot call LOCK_NEW_BUFF_FOR_UPDATE directly, because in wcs_wtstart
					   csr->dirty is reset before it releases the LOCK in the buffer.
					   To avoid this very small window followings are needed. */
					for (lcnt = 1; ; lcnt++)
					{
						LOCK_BUFF_FOR_UPDATE(mmblkr, n, &cs_addrs->nl->db_latch);
						if (!OWN_BUFF(n))
						{
							if (BUF_OWNER_STUCK < lcnt)
							{
								if (0 == mmblkr->dirty)
								{
									LOCK_NEW_BUFF_FOR_UPDATE(mmblkr);
									break;
								}
								else
								{
									assert(FALSE);
									return cdb_sc_comfail;
								}
							}
							if (WRITER_STILL_OWNS_BUFF(mmblkr, n))
								wcs_sleep(lcnt);
						}
						else
						{
							break;
						}
					}
                                        break;
                                }
                                assert(lcnt <= (MAX_CYCLES * max_ent));
                        }
                        else if ((mmblk_rec_ptr_t)CR_NOTVALID == mmblkr)
                        {
                                /* ------------- yet to write recovery mechanisms if hashtable is corrupt ------*/
                                /* ADD CODE LATER */
                                assert(FALSE);
                        }
                        else
                        {
                                for (lcnt = 1; ; lcnt++)
                                {
                                        LOCK_BUFF_FOR_UPDATE(mmblkr, n, &cs_addrs->nl->db_latch);
                                        if (!OWN_BUFF(n))
                                        {
                                                if (BUF_OWNER_STUCK < lcnt)
                                                {
                                                        if (0 == mmblkr->dirty)
                                                        {
                                                                LOCK_NEW_BUFF_FOR_UPDATE(mmblkr);
                                                                break;
                                                        }
                                                        else
                                                        {
                                                                assert(FALSE);
                                                                return cdb_sc_comfail;
                                                        }
                                                }
                                                if (WRITER_STILL_OWNS_BUFF(mmblkr, n))
                                                        wcs_sleep(lcnt);
                                        }
                                        else
                                        {
                                                break;
                                        }
                                }

                                if (0 != mmblkr->dirty)
                                        earlier_dirty = TRUE;
                                else
                                        earlier_dirty = FALSE;
                        }
                }
        }
#endif
        /* check for online backup -- ATTN: this part of code should be same as in bg_update, except for the db_addr[0] part. */
        if ((cs->blk >= cs_addrs->nl->nbb)
                && (0 == cs_addrs->backup_buffer->failed)
                && (((blk_hdr_ptr_t)(db_addr[0]))->tn < cs_addrs->backup_buffer->backup_tn)
                && (((blk_hdr_ptr_t)(db_addr[0]))->tn >= cs_addrs->backup_buffer->inc_backup_tn))
        {
                backup_block(cs->blk, db_addr[0]);
                if (0 == dollar_tlevel)
                        block_saved = TRUE;
                else
                        si->backup_block_saved = TRUE;
        }
        if (gds_t_writemap == cs->mode)
        {
                assert(0 == (cs->blk & (BLKS_PER_LMAP - 1)));
                if (FALSE == cs->done)
                	gvcst_map_build((uint4 *)cs->upd_addr, db_addr[0], cs, effective_tn);
		else
		{
			/* It has been built; Update tn in the block and copy from private memory to shared space */
			/* It's actually dse_chng_bhead which needs dse_running flag, it's ok for now */
			assert(dse_running || write_after_image);
                        assert(((blk_hdr_ptr_t)cs->new_buff)->tn == effective_tn);
                        memcpy(db_addr[0], cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
		}
                bm_update(cs, db_addr[0], TRUE);
        } else
        {
                /* either it is a non-local-bit-map block or we are in dse_maps() indicated by !run_time */
                assert((0 != (cs->blk & (BLKS_PER_LMAP - 1))) || (!run_time));
                if (FALSE == cs->done)
                {       /* if the current block has not been built (from being referenced in TP) */
                        if (0 != cs->new_buff)
                                cs->first_copy = TRUE;
                        gvcst_blk_build(cs, db_addr[0], effective_tn);
                } else
                {
			/* It has been built; Update tn in the block and copy from private memory to shared space */
			/* It's actually dse_chng_bhead which needs dse_running flag, it's ok for now */
                        assert(dse_running || write_after_image || 0 < dollar_tlevel);
                        assert(dse_running || ctn == effective_tn);
                        if (!dse_running)
				((blk_hdr_ptr_t)db_addr[0])->tn = ((blk_hdr_ptr_t)cs->new_buff)->tn = ctn;
                        memcpy(db_addr[0], cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
                }
                assert(sizeof(blk_hdr) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
                assert((int)(((blk_hdr_ptr_t)db_addr[0])->bsiz) <= cs_data->blk_size);
                if (0 == dollar_tlevel)
                {
                        if (0 != cs->ins_off)
                        {       /* reference to resolve: insert real block numbers in the buffer */
                                assert(0 <= (short)cs->index);
                                assert(&cw_set[cs->index] < cs);
                                assert((sizeof(blk_hdr) + sizeof(rec_hdr)) <= cs->ins_off);
                                assert((cs->ins_off + sizeof(block_id)) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
                                PUT_LONG(db_addr[0] + cs->ins_off, cw_set[cs->index].blk);
                                if (((nxt = cs + 1) < cs_top) && (gds_t_write_root == nxt->mode))
                                {
                                        /* If the next record is a WRITE_ROOT, it contains a second block pointer to resolve
                                         * and it operates on the current block block
                                         */
                                        assert(0 <= (short)nxt->index);
                                        assert(&cw_set[nxt->index] < nxt);
                                        assert((sizeof(blk_hdr) + sizeof(rec_hdr)) <= nxt->ins_off);
                                        assert((nxt->ins_off + sizeof(block_id)) <= ((blk_hdr_ptr_t)db_addr[0])->bsiz);
                                        PUT_LONG(db_addr[0] + nxt->ins_off, cw_set[nxt->index].blk);
                                }
                        }
                } else
                {       /* TP */
                        if (0 != cs->first_off)
                        {       /* TP resolve pointer references to new blocks */
                                for ( chain_ptr = db_addr[0] + cs->first_off; ; chain_ptr += chain.next_off)
                                {
                                        GET_LONGP(&chain, chain_ptr);
                                        assert(1 == chain.flag);
                                        assert((int)(chain_ptr - db_addr[0] + chain.next_off)
                                                        <= (int)(((blk_hdr_ptr_t)db_addr[0])->bsiz));
                                        assert((int)chain.cw_index < sgm_info_ptr->cw_set_depth);
                                        tp_get_cw(si->first_cw_set, chain.cw_index, &cs_ptr);
                                        PUT_LONG(chain_ptr, cs_ptr->blk);
                                        if (0 == chain.next_off)
                                                break;
                                }
                        }
                }       /* TP */
        }       /* not a map */
        if (certify_all_blocks)
        {
                if (FALSE == cert_blk(cs->blk,(blk_hdr_ptr_t)db_addr[0],
				(0 < dollar_tlevel) ? (cs->blk_target ? cs->blk_target->root : 0) : gv_target->root))
                        GTMASSERT;
        }
        if (0 == cs_data->defer_time)
        {
#if defined(UNTARGETED_MSYNC)
                if (cs_addrs->ti->last_mm_sync != cs_addrs->ti->curr_tn)
                {
                        if (-1 == msync((caddr_t)cs_addrs->db_addrs[0],
                                        (size_t)(cs_addrs->db_addrs[1] - cs_addrs->db_addrs[0]), MS_SYNC))
                        {
                                assert(FALSE);
                                return cdb_sc_comfail;
                        }
                        cs_addrs->ti->last_mm_sync = cs_addrs->ti->curr_tn;     /* Save when did last full sync */
                }
#elif defined(TARGETED_MSYNC)
                caddr_t start;

                start = (caddr_t)ROUND_DOWN((sm_off_t)db_addr[0], MSYNC_ADDR_INCS);
                if (-1 == msync(start,
			(size_t)ROUND_UP((sm_off_t)((caddr_t)db_addr[0] - start) + cs_data->blk_size, MSYNC_ADDR_INCS), MS_SYNC))
                {
                        assert(FALSE);
                        return cdb_sc_comfail;
                }
#else
                unix_db_info    *udi;
                int4            save_errno;

                udi = FILE_INFO(gv_cur_region);

                LSEEKWRITE(udi->fd, (db_addr[0] - (sm_uc_ptr_t)cs_data), db_addr[0], cs_data->blk_size, save_errno);
                if (0 != save_errno)
                {
                        gtm_putmsg(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region),
				ERR_TEXT, 2, RTS_ERROR_TEXT("Error during MM Block Write"), save_errno);
                        assert(FALSE);
                        return cdb_sc_comfail;
                }
#endif
        }
#if (!defined(UNTARGETED_MSYNC))
        if (0 < cs_data->defer_time)
        {
                int4    n;

                mmblkr->dirty = cs_addrs->ti->curr_tn;
                mmblkr->refer = TRUE;

                if (FALSE == earlier_dirty)
                {
                        ADD_ENT_TO_ACTIVE_QUE_CNT(&cs_addrs->nl->wcs_active_lvl, &cs_addrs->nl->wc_var_lock);
                        DECR_CNT(&cs_addrs->nl->wc_in_free, &cs_addrs->nl->wc_var_lock);
                        if (INTERLOCK_FAIL == INSQTI((que_ent_ptr_t)&mmblkr->state_que,
                                (que_head_ptr_t)&cs_addrs->acc_meth.mm.mmblk_state->mmblkq_active))
                        {
                                assert(FALSE);
                                return cdb_sc_comfail;
                        }
                }
                RELEASE_BUFF_UPDATE_LOCK(mmblkr, n, &cs_addrs->nl->db_latch);

                if (WRITER_BLOCKED_BY_PROC(n))
                {
                        /* it's off the active queue, so put it back at the head */
                        if (INTERLOCK_FAIL == INSQHI((que_ent_ptr_t)&mmblkr->state_que,
                                (que_head_ptr_t)&cs_addrs->acc_meth.mm.mmblk_state->mmblkq_active))
                        {
                                assert(FALSE);
                                return cdb_sc_comfail;
                        }
                }
        }
#endif

        return cdb_sc_normal;
}



/* update buffered global database */
enum cdb_sc     bg_update(cw_set_element *cs, cw_set_element *cs_top, trans_num ctn, trans_num effective_tn, sgm_info *si)
{
        int4            n;
        uint4           lcnt;
        off_chain       chain;
        sm_uc_ptr_t     blk_ptr, chain_ptr;
        cw_set_element  *cs_ptr, *nxt;
        bt_rec_ptr_t    bt;
        cache_rec_ptr_t cr;
        bool            recycled;

        error_def(ERR_DBCCERR);
        error_def(ERR_ERRCALL);
	error_def(ERR_WCBLOCKED);

        assert(cs_addrs->now_crit);
        assert((0 < dollar_tlevel) || (cs->blk <= cs_addrs->ti->total_blks));
        assert(0 <= cs->blk);
	INCR_DB_CSH_COUNTER(cs_addrs, n_bg_updates, 1);
        bt = bt_put(gv_cur_region, cs->blk);
	if (NULL == bt)		/* for NULL return, we would have already done secshr_db_clnup() in bt_put */
		return cdb_sc_cacheprob;
	if (cs->write_type & GDS_WRITE_KILL)
		bt->killtn = ctn;
        cr = (cache_rec_ptr_t)bt->cache_index;
        recycled = FALSE;
        if ((cache_rec_ptr_t)CR_NOTVALID == cr)
        {       /* no cache record associated with the bt_rec */
                if (NULL == (cr = db_csh_get(cs->blk)))
                {       /* no cache_rec associated with the block */
			assert((gds_t_acquired == cs->mode) || (0 != cs->new_buff));
			INCR_DB_CSH_COUNTER(cs_addrs, n_bg_update_creates, 1);
                        cr = db_csh_getn(cs->blk);
                        if ((cache_rec_ptr_t)CR_NOTVALID == cr)
                        {
				assert(FALSE);
				secshr_db_clnup(NORMAL_TERMINATION);
                                SET_TRACEABLE_VAR(cs_data->wc_blocked, TRUE);
                                BG_TRACE_PRO(wcb_t_end_sysops_nocr_invcr);
                                send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_t_end_sysops_nocr_invcr"),
					process_id, ctn, DB_LEN_STR(gv_cur_region));
                                return cdb_sc_cacheprob;
                        }
                        assert(NULL != cr);
                        assert(0 == cr->dirty);
                        assert(cr->blk == cs->blk);
                        assert(FALSE == cr->in_cw_set);
			cr->data_invalid = TRUE;                /* the buffer has just been identified and is still empty */
                } else if ((cache_rec_ptr_t)CR_NOTVALID == cr)
                {
			assert(FALSE);
			secshr_db_clnup(NORMAL_TERMINATION);
                        SET_TRACEABLE_VAR(cs_data->wc_blocked, TRUE);
                        BG_TRACE_PRO(wcb_t_end_sysops_cr_invcr);
                        send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_t_end_sysops_cr_invcr"),
                                process_id, ctn, DB_LEN_STR(gv_cur_region));
                        return cdb_sc_cacheprob;
                } else if (-1 != cr->read_in_progress)
                {
                        /* wait for another process in t_qread to stop overlaying the buffer */
                        /* this case is believed to require the reuse of a killed block that's still in the cache */
			assert(gds_t_acquired == cs->mode);
                        for (lcnt = 1; -1 != cr->read_in_progress; lcnt++)
                        {       /* very similar code appears elsewhere and perhaps should be common */
                                wcs_sleep(lcnt);
                                if (BUF_OWNER_STUCK < lcnt)
                                {       /* sick of waiting */
                                        if (-1 > cr->read_in_progress)
                                        {       /* outside of design; clear to known state */
                                                INTERLOCK_INIT(cr);
                                                assert(0 == cr->r_epid);
                                                cr->r_epid = 0;
                                        } else if (0 != cr->r_epid)
                                        {
                                                if (FALSE == is_proc_alive(cr->r_epid, cr->image_count))
                                                {       /* process gone; release its lock */
                                                        RELEASE_BUFF_READ_LOCK(cr);
                                                } else
                                                {
                                                        assert(FALSE);
							secshr_db_clnup(NORMAL_TERMINATION);
                                                        SET_TRACEABLE_VAR(cs_data->wc_blocked, TRUE);
                                                        BG_TRACE_PRO(wcb_t_end_sysops_rip_wait);
                                                        send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6,
                                                                LEN_AND_LIT("wcb_t_end_sysops_rip_wait"),
                                                                process_id, ctn, DB_LEN_STR(gv_cur_region));
                                                        return cdb_sc_cacheprob;
                                                }
                                        } else
                                        {       /* process stopped before could set r_epid */
                                                RELEASE_BUFF_READ_LOCK(cr);
                                                if (-1 > cr->read_in_progress)
                                                {       /* process released since if (cr->r_epid); rectify semaphore  */
                                                        LOCK_BUFF_FOR_READ(cr, n);
                                                }
                                        }
                                }
                        }
                }
                cs->first_copy = TRUE;
                cr->in_tend = TRUE;                     /* in_tend sb set before the semaphore (and data_invalid) */
		/* Here we cannot call LOCK_NEW_BUFF_FOR_UPDATE directly, because in wcs_wtstart csr->dirty is reset
		before it releases the LOCK in the buffer. To avoid this very small window followings are needed. */
                for (lcnt = 1; ; lcnt++)
                {       /* unix only logic
                         * the design here is that either this process owns the block, or the writer does.
                         * if the writer does, it must be allowed to finish its write; then it will release the block
                         * and the next LOCK will establish ownership
                         */
                        LOCK_BUFF_FOR_UPDATE(cr, n, &cs_addrs->nl->db_latch);
							/* this destroys evidence of writer ownership, but read on */
                        if (OWN_BUFF(n))                /* this is really a test that there was no prior owner */
                                break;
                        else
                        {
                                if (BUF_OWNER_STUCK < lcnt)
                                {       /* sick of waiting */
                                        if (0 == cr->dirty)
                                        {       /* someone dropped something; assume it was the writer and go on */
                                                LOCK_NEW_BUFF_FOR_UPDATE(cr);
                                                break;
                                        } else
                                        {       /* ??? should the above logic be used regardless of dirty
                                                 * ??? should we be maintaining cr->epid to do a better job of this
                                                 * ??? do restarts work correctly after blocks are already processed
                                                 * ??? should this be a GTMASSERT
                                                 */
                                                DUMP_LOCKHIST();        /* Find history of the locks and relative activity */
                                                assert(FALSE);
                                                cr->in_tend = FALSE;
                                                return cdb_sc_comfail;
                                        }
                                }
                                if (WRITER_STILL_OWNS_BUFF(cr, n))
                                        wcs_sleep(lcnt);
                        }
                }       /* end of for loop to control buffer */
                cr->bt_index = GDS_ABS2REL(bt);
                bt->cache_index = GDS_ABS2REL(cr);
        } else
        {
                cr = (cache_rec_ptr_t)GDS_REL2ABS(cr);
                assert(CR_BLKEMPTY != cr->blk);
                assert(FALSE == cr->in_tend);
                cr->in_tend = TRUE;
                for (lcnt = 1; ; lcnt++)
                {       /* unix only logic
                         * the design here is that either this process owns the block, or the writer does.
                         * if the writer does, it must be allowed to finish its write; then it will release the block
                         * and the next LOCK will establish ownership
                         */
                        LOCK_BUFF_FOR_UPDATE(cr, n, &cs_addrs->nl->db_latch);
							/* this destroys evidence of writer ownership, but read on */
                        if (OWN_BUFF(n))                /* this is really a test that there was no prior owner */
                        {                               /* it will only be true if the writer has cleared it */
                                if (0 != cr->dirty)
                                {       /* own buff - treat as if in active queue */
                                        recycled = TRUE;
                                        break;
                                }
                                /* Buffer is free */
                                if (-1 != cr->read_in_progress)
                                {
                                        /* wait for another process in t_qread to stop overlaying the buffer */
                                        /* this case is believed to require the reuse of a killed block that's still in the cache */
					assert(gds_t_acquired == cs->mode);
                                        for (lcnt = 1; -1 != cr->read_in_progress; lcnt++)
                                        {
                                                wcs_sleep(lcnt);
                                                if (BUF_OWNER_STUCK < lcnt)
                                                {       /* sick of waiting */
                                                        if (-1 > cr->read_in_progress)
                                                        {       /* outside of design; clear to known state */
                                                                INTERLOCK_INIT(cr);
                                                                assert(0 == cr->r_epid);
                                                                cr->r_epid = 0;
                                                        } else if (0 != cr->r_epid)
                                                        {
                                                                if (FALSE == is_proc_alive(cr->r_epid, cr->image_count))
                                                                {       /* process gone; release its lock */
                                                                        RELEASE_BUFF_READ_LOCK(cr);
                                                                } else
                                                                {
                                                                        assert(FALSE);
									secshr_db_clnup(NORMAL_TERMINATION);
                                                                        SET_TRACEABLE_VAR(cs_data->wc_blocked, TRUE);
                                                                        BG_TRACE_PRO(wcb_t_end_sysops_dirtyripwait);
                                                                        send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6,
                                                                                LEN_AND_LIT("wcb_t_end_sysops_dirtyripwait"),
                                                                                process_id, ctn, DB_LEN_STR(gv_cur_region));
                                                                        return cdb_sc_cacheprob;
                                                                }
                                                        } else
                                                        {       /* process stopped before could set r_epid */
                                                                RELEASE_BUFF_READ_LOCK(cr);

                                                                if (-1 > cr->read_in_progress)
                                                                { /* process released since if (cr->r_epid); rectify semaphore  */
                                                                        LOCK_BUFF_FOR_READ(cr, n);
                                                                }
                                                        }
                                                }       /* sick of waiting */
                                        }       /* for until read is finished */
                                }       /* if read is in progress */
                                break;
                        } else
                        {
                                if (BUF_OWNER_STUCK < lcnt)
                                {       /* sick of waiting */
                                        if (0 == cr->dirty)
                                        {       /* someone dropped something; assume it was the writer and go on */
                                                LOCK_NEW_BUFF_FOR_UPDATE(cr);
                                                break;
                                        } else
                                        {       /* ??? should the above logic be used regardless of dirty
                                                 * ??? should we be maintaining cr->epid to do a better job of this
                                                 * ??? do restarts work correctly after blocks are already processed
                                                 * ??? should this be a GTMASSERT
                                                 */
                                                DUMP_LOCKHIST();        /* Find history of the locks and relative activity */
                                                assert(FALSE);
                                                cr->in_tend = FALSE;
                                                return cdb_sc_comfail;
                                        }
                                }
                                if (WRITER_STILL_OWNS_BUFF(cr, n))
                                        wcs_sleep(lcnt);
                        }
                }       /* end of for loop to control buffer */
        }       /* end of if / else on cr NOTVALID */
        if (FALSE == cr->in_cw_set)
        {
                /* in_cw_set should always be set unless we're in DSE (indicated by !run_time),
                 * or this is a newly created block, or we have an in-memory copy.
                 */
		assert((FALSE == run_time) || (gds_t_acquired == cs->mode) || (0 != cs->new_buff));
                if (0 == dollar_tlevel)         /* stuff it in the array before setting in_cw_set */
                {
                        assert((((MAX_BT_DEPTH * 2) - 1) * 2) > cr_array_index);
                        cr_array[cr_array_index++] = cr;
                } else
                {
                        assert(si->cr_array_index < si->cr_array_size);
                        si->cr_array[si->cr_array_index++] = cr;
                }
                cr->in_cw_set = TRUE;
        }
        if (0 != cr->r_epid)
        {       /* must have got it with a db_csh_getn */
                assert(cr->r_epid == process_id);
                cr->r_epid = 0;
                assert(0 == cr->read_in_progress);
                RELEASE_BUFF_READ_LOCK(cr);
        }
        assert(-1 == cr->read_in_progress);
        /* if the code should protect against interlocked queue errors in the middle of updates,
         * the insert logic should be moved here followed by a return,
         * and the code below should be invoked in a second loop after all cache records for the update are queued
         */
        cr->data_invalid = TRUE;                /* data_invalid sb set just before a valid block is modified */
        if (0 != cr->dirty)
        {
                /* Check for anomalous state (should not happen, but...) */
                if (cr->dirty <= cr->flushed_dirty_tn)
                {
                        /* Buffer was flushed and is no longer on the active queue.  Somehow the
                         * dirty flag remained set to an old value.  Fix the value of dirty and
                         * arrange for this buffer to be placed on the active queue.
                         */
#ifdef DEBUG
                        cache_rec cr_save;      /* save it for core which will rewrite it */
                        cr_save = *cr;
                        assert(FALSE);
#endif
                        cr->dirty = ctn;
                        recycled = FALSE;
                }
        } else
                cr->dirty = ctn;                /* block will be dirty.  Note the tn in which this occurred */
        blk_ptr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
        assert((0 <= cs->blk) && (cs->blk <= cs_addrs->ti->total_blks));

        /* check for online backup - ATTN: this part of code should be same as that in mm_update, except for the blk_ptr part. */
#ifdef DEBUG_COMPREHENSIVE_BACKUP
        /* the following util_out_print part is for debug use only */
        if (cs->blk < cs_addrs->nl->nbb)
                util_out_print("no backup because: block !8UL < !8UL", TRUE, cs->blk, cs_addrs->nl->nbb);
        else if (0 != cs_addrs->backup_buffer->failed)
                util_out_print("no backup because: failed field in backup buffer is !12UL", TRUE, cs_addrs->backup_buffer->failed);
        else if (((blk_hdr_ptr_t)(blk_ptr))->tn >= cs_addrs->backup_buffer->backup_tn)
                util_out_print("no backup because: blk tn !8UL >= !8UL, backup_tn", TRUE,
				((blk_hdr_ptr_t)(blk_ptr))->tn, cs_addrs->backup_buffer->backup_tn);
        else if (((blk_hdr_ptr_t)(blk_ptr))->tn < cs_addrs->backup_buffer->inc_backup_tn)
                util_out_print("no backup because: blk tn !8UL < !8UL inc_backup_tn", TRUE,
				((blk_hdr_ptr_t)(blk_ptr))->tn, cs_addrs->backup_buffer->inc_backup_tn);
#endif
        if ((cs->blk >= cs_addrs->nl->nbb)
                && (0 == cs_addrs->backup_buffer->failed)
                && (((blk_hdr_ptr_t)(blk_ptr))->tn < cs_addrs->backup_buffer->backup_tn)
                && (((blk_hdr_ptr_t)(blk_ptr))->tn >= cs_addrs->backup_buffer->inc_backup_tn))
        {
                assert(cr->blk == cs->blk);
                backup_block(cs->blk, blk_ptr);
                if (0 == dollar_tlevel)
                        block_saved = TRUE;
                else
                        si->backup_block_saved = TRUE;
        }

        if (gds_t_writemap == cs->mode)
        {
		assert(!run_time || cs->cr == cr && cs->cycle == cr->cycle);
                assert(0 == (cs->blk & (BLKS_PER_LMAP - 1)));
                if (FALSE == cs->done)
               		gvcst_map_build((uint4 *)cs->upd_addr, blk_ptr, cs, effective_tn);
		else
		{
			/* It has been built; Update tn in the block and copy from private memory to shared space */
			/* It's actually dse_chng_bhead which needs dse_running flag, it's ok for now */
			assert(dse_running || write_after_image);
                        assert(((blk_hdr_ptr_t)cs->new_buff)->tn == effective_tn);
                        memcpy(blk_ptr, cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
		}
                bm_update(cs, (sm_uc_ptr_t)cr->buffaddr, FALSE);
        } else
        {
                /* either it is a non-local bit-map or we are in dse_maps() indicated by !run_time */
                assert((0 != (cs->blk & (BLKS_PER_LMAP - 1))) || (FALSE == run_time));
                if (FALSE == cs->done)
                {       /* if the current block has not been built (from being referenced in TP) */
                        if (0 != cs->new_buff)
                                cs->first_copy = TRUE;
                        gvcst_blk_build(cs, blk_ptr, effective_tn);
                } else
                {
			/* It has been built; Update tn in the block and copy from private memory to shared space */
			/* It's actually dse_chng_bhead which needs dse_running flag, it's ok for now */
                        assert(dse_running || write_after_image || 0 < dollar_tlevel);
                        assert(dse_running || ctn == effective_tn);
                        if (!dse_running)
				((blk_hdr *)blk_ptr)->tn = ((blk_hdr_ptr_t)cs->new_buff)->tn = ctn;
                        memcpy(blk_ptr, cs->new_buff, ((blk_hdr_ptr_t)cs->new_buff)->bsiz);
                }
                assert(sizeof(blk_hdr) <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
                assert((int)((blk_hdr_ptr_t)blk_ptr)->bsiz <= cs_data->blk_size);
                if (0 == dollar_tlevel)
                {
                        if (0 != cs->ins_off)
                        {       /* reference to resolve: insert real block numbers in the buffer */
                                assert(0 <= (short)cs->index);
                                assert(cs - cw_set > cs->index);
                                assert((sizeof(blk_hdr) + sizeof(rec_hdr)) <= cs->ins_off);
                                assert((cs->ins_off + sizeof(block_id)) <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
                                PUT_LONG((blk_ptr + cs->ins_off), cw_set[cs->index].blk);
                                if (((nxt = cs + 1) < cs_top) && (gds_t_write_root == nxt->mode))
                                {
                                        /* If the next record is a WRITE_ROOT, it contains a 2nd block pointer to resolve
                                         * and it operates on the current block block
                                         */
                                        assert(0 <= (short)nxt->index);
                                        assert(nxt - cw_set > nxt->index);
                                        assert(sizeof(blk_hdr) <= nxt->ins_off);
                                        assert(nxt->ins_off <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
                                        PUT_LONG((blk_ptr + nxt->ins_off), cw_set[nxt->index].blk);
                                }
                        }
                } else
                {
                        if (0 != cs->first_off)
                        {       /* TP - resolve pointer references to new blocks */
                                for (chain_ptr = blk_ptr + cs->first_off; ; chain_ptr += chain.next_off)
                                {
                                        GET_LONGP(&chain, chain_ptr);
                                        assert(1 == chain.flag);
                                        assert((chain_ptr - blk_ptr + chain.next_off + sizeof(block_id))
                                                        <= ((blk_hdr_ptr_t)blk_ptr)->bsiz);
                                        assert((int)chain.cw_index < sgm_info_ptr->cw_set_depth);
                                        tp_get_cw(si->first_cw_set, (int)chain.cw_index, &cs_ptr);
                                        PUT_LONG(chain_ptr, cs_ptr->blk);
                                        if (0 == chain.next_off)
                                                break;
                                }
                        }
                }
		cs->cr = cr;		/* for non-bitmap blocks in TP, we may not know the "cr" and "cycle" until commit time */
		cs->cycle = cr->cycle;
        }
        if (certify_all_blocks)
        {
                if (FALSE == cert_blk(cs->blk, (blk_hdr_ptr_t)blk_ptr,
                                (0 < dollar_tlevel) ? (cs->blk_target ? cs->blk_target->root : 0)
                                                        : gv_target->root))
                        GTMASSERT;
        }
	if (FALSE == recycled)
        	cr->jnl_addr = cs->jnl_freeaddr;	/* update jnl_addr only if cache-record is not already in active queue */
        cr->tn = ctn;
        cr->data_invalid = FALSE;
        if (FALSE == recycled)
        {       /* stuff it on the active queue */
                n = INSQTI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cs_addrs->acc_meth.bg.cache_state->cacheq_active);
                if (INTERLOCK_FAIL == n)
                        return cdb_sc_comfail;
                ADD_ENT_TO_ACTIVE_QUE_CNT(&cs_addrs->nl->wcs_active_lvl, &cs_addrs->nl->wc_var_lock);
                DECR_CNT(&cs_addrs->nl->wc_in_free, &cs_addrs->nl->wc_var_lock);
        }
        RELEASE_BUFF_UPDATE_LOCK(cr, n, &cs_addrs->nl->db_latch);
        cr->in_tend = FALSE;
        if (WRITER_BLOCKED_BY_PROC(n))
        {       /* it's off the active que, so put it back at the head to minimize the chances of blocks being "pinned" in memory */
                n = INSQHI((que_ent_ptr_t)&cr->state_que, (que_head_ptr_t)&cs_addrs->acc_meth.bg.cache_state->cacheq_active);
                if (INTERLOCK_FAIL == n)
                {
                        assert(FALSE);
                        return cdb_sc_comfail;
                }
        } else
                assert(FALSE == OWN_BUFF(n));
        VERIFY_QUEUE_LOCK(&cs_addrs->acc_meth.bg.cache_state->cacheq_active, &cs_addrs->nl->db_latch);

#ifdef unused_code
/* the code above is the simple alternative;
 * the code below has the advantage of limiting (but not eliminating) the likelihood of an insqti,
 * which (really shouldn't, but) might fail after some blocks have been committed
 */
        while (WRITER_BLOCKED_BY_PROC(n))
        {       /* this loop should probably be limited for safety */
                if ((0 == cr->jnl_addr) || (cr->jnl_addr <= cs_addrs->jnl->jnl_buff->dskaddr))
                {
                        cr->epid = process_id;
                        if (SS_NORMAL != dsk_write(gv_cur_region, cs->blk, GDS_REL2ABS(cr->buffaddr)))
                        {       /* if it fails, put it back and hope that another time,
                                   another writer will do the work (infinite retry) */
                                cr->epid = 0;
                                n = INSQTI((que_ent_ptr_t)&cr->state_que,
						(que_head_ptr_t)&cs_addrs->acc_meth.bg.cache_state->cacheq_active);
                                if (INTERLOCK_FAIL == n)
                                {
                                        CLEAR_BUFF_UPDATE_LOCK(cr, &cs_addrs->nl->db_latch);      /* not on the active queue */
                                        rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, DB_LEN_STR(reg), ERR_ERRCALL, 3, CALLFROM);
                                }
                                RELEASE_BUFF_UPDATE_LOCK(cr, n, &cs_addrs->nl->db_latch);
                                assert(FALSE);
                        } else
                        {
                                SUB_ENT_FROM_ACTIVE_QUE_CNT(&cs_addrs->nl->wcs_active_lvl, &cs_addrs->nl->wc_var_lock);
                                cr->dirty = 0;
                                cr->epid = 0;
                                CLEAR_BUFF_UPDATE_LOCK(cr, &cs_addrs->nl->db_latch);     /* not on the active queue */
                                break;
                        }
                } else
                {       /* put it back */
                        n = INSQTI((que_ent_ptr_t)&cr->state_que,
					(que_head_ptr_t)&cs_addrs->acc_meth.bg.cache_state->cacheq_active);
                        if (INTERLOCK_FAIL == n)
                        {
                                CLEAR_BUFF_UPDATE_LOCK(cr, &cs_addrs->nl->db_latch);     /* not on the active queue */
                                rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, DB_LEN_STR(reg),
                                                       ERR_ERRCALL, 3, CALLFROM);
                        }
                        RELEASE_BUFF_UPDATE_LOCK(cr, n, &cs_addrs->nl->db_latch);
                }
        }
#endif
        return cdb_sc_normal;
}

/* Used to prevent staleness of buffers. Start timer to call wcs_stale to do periodic flushing */
void    wcs_timer_start(gd_region *reg, boolean_t io_ok)
{
        int4                    reg_parm;
        jnl_private_control     *jpc;
        sgmnt_addrs             *csa;
        sgmnt_data_ptr_t        csd;
        gd_region               *save_region;

        csa = &FILE_INFO(reg)->s_addrs;
        csd = csa->hdr;

        /* This process can only have one flush timer per region. Overall,
           there can only be 2 outstanding timers per region for the
           entire system. Note: wcs_timers starts at -1.. */

        if ((FALSE == csa->timer) && (csa->nl->wcs_timers < 1))
        {
                if ((dba_bg == reg->dyn.addr->acc_meth) ||              /* bg mode or */
                    (dba_mm == reg->dyn.addr->acc_meth && (0 < csd->defer_time)))       /* defer'd mm mode */
                {
                        reg_parm = (int4)reg;
                        csa->timer = TRUE;
                        INCR_CNT(&csa->nl->wcs_timers, &csa->nl->wc_var_lock);
                        start_timer((TID)reg,
                                    csd->flush_time[0] * (dba_bg == reg->dyn.addr->acc_meth ? 1 : csd->defer_time),
                                    &wcs_stale,
                                    sizeof(reg_parm),
                                    (char *)&reg_parm);
                        BG_TRACE_ANY(csa, stale_timer_started);
                }
        }

        /* If we are being called from a timer driven routine, it is not possible to do IO at this time
	 * because the state of the machine (crit check, lseekio, etc.) is not being checked here. */
        if (FALSE == io_ok)
                return;

	/* Note the non-usage of TP_CHANGE_REG_IF_NEEDED macros since this routine can be timer driven. */
	/* Use this opportunity to sync the db if necessary (as a result of writing an epoch record) */
	if (dba_bg == csd->acc_meth && JNL_ENABLED(csd))
	{
		jpc = csa->jnl;
                save_region = gv_cur_region;            /* Save current region and switch gv_cur_region. */
		TP_CHANGE_REG(reg);		/* Since jnl_qio_start has an assert(csa == cs_addrs). */
		if (jpc && jpc->jnl_buff->need_db_fsync && NOJNL != jpc->channel)
			jnl_qio_start(jpc);	/* See jnl_qio_start for how it achieves the db_fsync */
		TP_CHANGE_REG(save_region);
	}
        /* If we are getting too full, do some i/o to clear some out. This should happen only as we are
         * getting near the saturation point in unix. */
        if (csd->flush_trigger <= csa->nl->wcs_active_lvl)
        {
                save_region = gv_cur_region;            /* Save current region for switcheroo */
		TP_CHANGE_REG(reg);
                wcs_wtstart(reg, 0);                    /* Already in need of a good flush */
		TP_CHANGE_REG(save_region);
                csa->stale_defer = FALSE;               /* This took care of any pending work for this region */
        }
        return;
}

/* make sure that the journal file is available if appropriate */
uint4	jnl_ensure_open(void)
{
	uint4	jnl_status = 0;
	DEBUG_ONLY(
		gd_id	save_jnl_gdid;
	)

	assert(cs_addrs->now_crit);
        assert(NULL !=  cs_addrs->jnl);
        if (NOJNL == cs_addrs->jnl->channel)
        {
                cs_addrs->jnl->pini_addr = 0;
                jnl_status = jnl_file_open(gv_cur_region, 0 == cs_data->jnl_file.u.inode, 0);
        } else if (!is_gdid_gdid_identical(&cs_addrs->jnl->fileid, &cs_data->jnl_file.u))
        {
		DEBUG_ONLY(save_jnl_gdid = cs_data->jnl_file.u;)
                /* The journal file has been changed "on the fly"; close the old one and open the new one */
                close(cs_addrs->jnl->channel);
                cs_addrs->jnl->channel = NOJNL;
                cs_addrs->jnl->pini_addr = 0;
		if (GTCM_GNP_SERVER_IMAGE == image_type)
			gtcm_jnl_switched();
                jnl_status = jnl_file_open(gv_cur_region, 0 == cs_data->jnl_file.u.inode, 0);
        }
	return jnl_status;
}

/* go after a specific number of buffers or a particular buffer */
/* not called if UNTARGETED_MSYNC and MM mode */
bool    wcs_get_space(gd_region *reg, int needed, cache_rec_ptr_t cr)
{
        sgmnt_addrs             *csa;
        sgmnt_data_ptr_t        csd;
        cache_que_head_ptr_t    q0, base;
        int4                    n, save_errno = 0, k, i, dummy_errno, max_count, count;
        uint4                   lcnt, size, to_wait, to_msg;

        error_def(ERR_DBFILERR);
        error_def(ERR_WAITDSKSPACE);

        assert((0 != needed) || (NULL != cr));

        csa = &FILE_INFO(reg)->s_addrs;
        csd = csa->hdr;

#if defined(UNTARGETED_MSYNC)
        if (dba_mm == csd->acc_meth)
                assert(FALSE);
#endif

	csd->flush_trigger = MAX(csd->flush_trigger - MAX(csd->flush_trigger / STEP_FACTOR, 1), MIN_FLUSH_TRIGGER(csd->n_bts));
        /* Routine actually serves two purposes:
           1 - Free up required number of buffers or
           2 - Free up a specific buffer
           Do a different kind of loop depending on which is our
           current calling. */
        if (0 != needed)
        {
                BG_TRACE_ANY(csa, bufct_buffer_flush);
                for (lcnt = 1; (csa->nl->wc_in_free < needed) && (BUF_OWNER_STUCK > lcnt); ++lcnt)
                {
			JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, needed, save_errno);
                        if (csa->nl->wc_in_free < needed)
			{
				if ((ENOSPC == save_errno) && (csa->hdr->wait_disk_space > 0))
				{
					/* not enough disk space to flush the buffers to regain them
					 * so wait for it to become available,
					 * and if it takes too long, just
					 * quit. Unfortunately, quitting would
					 * invoke the recovery logic which
					 * should be of no help to this
					 * situation. Then what?
					 */
					lcnt = BUF_OWNER_STUCK;
					to_wait = cs_data->wait_disk_space;
					to_msg = (to_wait / 8) ? (to_wait / 8) : 1; /* output error message around 8 times */
					while ((0 < to_wait) && (ENOSPC == save_errno))
					{
						if ((to_wait == cs_data->wait_disk_space)
							|| (0 == to_wait % to_msg))
						{
							send_msg(VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								process_id, to_wait, DB_LEN_STR(reg), save_errno);
							gtm_putmsg(VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								process_id, to_wait, DB_LEN_STR(reg), save_errno);
						}
						hiber_start(1000);
						to_wait--;
						JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, needed, save_errno);
						if (csa->nl->wc_in_free >= needed)
							break;
					}
				}
				wcs_sleep(lcnt);
			}
			else
				return TRUE;
                        BG_TRACE_ANY(csa, bufct_buffer_flush_loop);
                }
                if (csa->nl->wc_in_free >= needed)
                        return TRUE;
        } else
        {
#ifdef old_code
                BG_TRACE_ANY(csa, spcfc_buffer_flush);
                for (lcnt = 1; (0 != cr->dirty) && (BUF_OWNER_STUCK > lcnt); ++lcnt)
                {
                        for (; 0 != cr->dirty && 0 != csa->acc_meth.bg.cache_state->cacheq_active.fl;)
				JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0);
                        if (0 != cr->dirty)
                                wcs_sleep(lcnt);
                        else
                                return TRUE;
                        BG_TRACE_ANY(csa, spcfc_buffer_flush_loop);
                }
                if (0 == cr->dirty)
                        return TRUE;
#endif

                /* Wait for a specific buffer to be flushed. We attempt to speed this along by
                   shuffling the entry we want to the front of the queue before we call routines to
                   do some writing. */

                assert(csa->now_crit);          /* must be crit to play with queues when not the writer */

                BG_TRACE_PRO_ANY(csa, spcfc_buffer_flush);

                ++fast_lock_count;                      /* Disable wcs_stale for duration */
                if (dba_bg == csd->acc_meth)            /* Determine queue base to use */
                {
                        base = &csa->acc_meth.bg.cache_state->cacheq_active;
                }
                else
                {
                        base = &csa->acc_meth.mm.mmblk_state->mmblkq_active;
                }

                for (k = 0; k < QI_STARVATION; k++)
                {
                        for (i = 0; i < QI_RETRY; i++)
                        {
                                if (GET_SWAPLOCK(&base->latch)) /* Lock queue to prevent interference */
                                {
					/* If it is still in the active queue, then insert it at the head of the queue */

					if (0 != cr->state_que.fl)
					{
						assert(FALSE == csa->wbuf_dqd);
						csa->wbuf_dqd = TRUE;
						q0 = (cache_que_head_ptr_t)((sm_uc_ptr_t)&cr->state_que + cr->state_que.fl);
						shuffqth((que_ent_ptr_t)q0, (que_ent_ptr_t)base);
						csa->wbuf_dqd = FALSE;
						VERIFY_QUEUE(base);
					}

					/* release the queue header lock so that the writers can proceed */
					RELEASE_SWAPLOCK(&base->latch);
					--fast_lock_count;
					assert(0 <= fast_lock_count);

					/* Fire off a writer to write it out. Another writer may grab our cache
					   record so we have to be willing to wait for him to flush it. */

					/* Flush this one buffer the first time through.
					 * If this didn't work, flush normal amount next time in the loop */

					JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 1, save_errno);
					for (lcnt = 1; (0 != cr->dirty) && (BUF_OWNER_STUCK > lcnt); ++lcnt)
					{
						max_count = ROUND_UP(csa->nl->wcs_active_lvl, csd->n_wrt_per_flu);
						/* loop till the active queue is exhausted */
						for (count = 0; 0 != cr->dirty && 0 != csa->nl->wcs_active_lvl &&
							     max_count > count; count++)
						{
							BG_TRACE_PRO_ANY(csa, spcfc_buffer_flush_retries);
							JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, save_errno);
						}
						/* Usually we want to sleep only if we need to wait on someone else
						 * i.e. (i) if we are waiting for another process' fsync to complete
						 *     (ii) if some concurrent writer has taken this cache-record out.
						 *    (iii) if someone else is holding the io_in_prog lock.
						 * Right now we know of only one case where there is no point in waiting
						 *   which is if the cache-record is out of the active queue and is dirty.
						 * But since that is quite rare and we don't lose much in that case by
						 *   sleeping we do an unconditional sleep (only if cr is dirty).
						 */
						if (!cr->dirty)
							return TRUE;
						else
							wcs_backoff(lcnt);
						BG_TRACE_PRO_ANY(csa, spcfc_buffer_flush_loop);
					}
					if (0 == cr->dirty)
						return TRUE;
					assert(FALSE);                  /* We have failed */
					get_space_fail_cr = cr;
					if (gtm_environment_init)
						gtm_fork_n_core();	/* take a snapshot in case running in-house */
					return FALSE;
				} else
				{	/* buffer was locked */
					if (0 == cr->dirty)
					{
						BG_TRACE_ANY(csa, spcfc_buffer_flushed_during_lockwait);
						--fast_lock_count;
						assert(0 <= fast_lock_count);
						return TRUE;
					}
                                }
                        }
                        if (0 != k)
                                wcs_backoff(k);
			performCASLatchCheck(&base->latch, k);
                }
                --fast_lock_count;
                assert(0 <= fast_lock_count);

                if (0 == cr->dirty)
                        return TRUE;
        }
	if (ENOSPC == save_errno)
		rts_error(VARLSTCNT(7) ERR_WAITDSKSPACE, 4, process_id, to_wait, DB_LEN_STR(reg), save_errno);
	else
		assert(FALSE);
	get_space_fail_cr = cr;
	if (gtm_environment_init)
		gtm_fork_n_core();	/* take a snapshot in case running in-house */
        return FALSE;
}

/* A timer has popped. Some buffers are stale -- start writing to the database */
void    wcs_stale(TID tid, int4 hd_len, gd_region **region)
{
        bool            need_new_timer;
	boolean_t	lseekIoInProgress_flag;
        gd_region       *reg, *save_region;
        sgmnt_addrs     *csa, *save_csaddrs;
        sgmnt_data_ptr_t        csd;

	/* Note the non-usage of TP_CHANGE_REG_IF_NEEDED macros since this routine can be timer driven. */
        save_region = gv_cur_region;            /* Certain debugging calls expect gv_cur_region to be correct */
	if (NULL == save_region || FALSE == save_region->open)
        	save_csaddrs = NULL;		/* Save to see if we are in crit anywhere */
	else
        	save_csaddrs = &FILE_INFO(save_region)->s_addrs;	/* Save to see if we are in crit anywhere */
        reg = *region;				/* Region addr needing some synching */
	assert(reg->open);
	TP_CHANGE_REG(reg);
        csa = cs_addrs;
        csd = cs_data; /* csa and csd might be NULL if region has been closed; we expect all timers for a closed region to have
			  been cancelled. But, for safety, we return if csd happens to be NULL */
	assert(NULL != csd);
        if (NULL == csd || (dba_mm == csd->acc_meth && csa->total_blks != csa->ti->total_blks)) /* csd == NULL <=> csa == NULL */
        { /* don't write if region has been closed, or acc meth is MM and file extended */
                if (save_region != gv_cur_region)
                {
			TP_CHANGE_REG(save_region);
                }
                return;
	}
        BG_TRACE_ANY(csa, stale_timer_pop);
        /* Default to need a new timer in case bypass main code because of invalid conditions */
        need_new_timer = TRUE;
        /****************************************************************************************************
           We don't want to do expensive IO flushing if:
           1) We are in the midst of lseek/read/write IO. This could reset an lseek.
           2) We are aquiring crit in any of our regions.
           3) We have crit in any region. Assumption is that if region we were in was not crit, we're clear.
              This is not strictly true in some special TP cases on the final retry if the previous retry did
              not get far enough into the transaction to cause all regions to be locked down but this case is
              statistically infrequent enough that we will go ahead and do the IO in crit "this one time".
           4) We are in a "fast lock".
           **************************************************************************************************/
	GET_LSEEK_FLAG(FILE_INFO(reg)->fd, lseekIoInProgress_flag);
	if ((FALSE == lseekIoInProgress_flag)
		&& (FALSE == crit_in_flux)
		&& (NULL == save_csaddrs || FALSE == save_csaddrs->now_crit)
		&& (0 == fast_lock_count))
        {
                switch (reg->dyn.addr->acc_meth)
                {
                    case dba_bg:
                        /* Flush at least some of our cache */
                        wcs_wtstart(reg, 0);

                        /* If there is no cache left, or already a timer pending, then no need for new timer */
                        if (0 == csa->acc_meth.bg.cache_state->cacheq_active.fl)
                                need_new_timer = FALSE;
                        break;

                    case dba_mm:
#if defined(UNTARGETED_MSYNC)
                        if (csa->ti->last_mm_sync != csa->ti->curr_tn)
                        {
                                bool    was_crit;

                                was_crit = csa->now_crit;
                                if (FALSE == was_crit)
                                        grab_crit(reg);
                                msync((caddr_t)csa->db_addrs[0], (size_t)(csa->db_addrs[1] - csa->db_addrs[0]),
                                      MS_SYNC);
                                csa->ti->last_mm_sync = csa->ti->curr_tn;       /* Save when did last full sync */
                                if (FALSE == was_crit)
                                        rel_crit(reg);
                                need_new_timer = FALSE; /* All sync'd up -- don't need another one */
                        }
#else
                        /* note that wcs_wtstart is called for TARGETED_MSYNC or FILE_IO */
                        wcs_wtstart(reg, 0);
                        if (0 == csa->acc_meth.mm.mmblk_state->mmblkq_active.fl)
                                need_new_timer = FALSE;
#endif
                        break;
                    default:;
                }
        } else
        {
                csa->stale_defer = TRUE;
                unhandled_stale_timer_pop = TRUE;
                BG_TRACE_ANY(csa, stale_process_defer);
        }
        assert(dba_bg == reg->dyn.addr->acc_meth || 0 < csd->defer_time);
        /* If fast_lock_count is non-zero, we must go ahead and set a new timer even if
           we don't need one because we cannot fall through to the DECR_CNT for wcs_timers below
           because we could deadlock. Otherwise, we can go ahead and use the rest of the
           tests to determine if we set a new timer or not. */
        if (0 != fast_lock_count || (need_new_timer && 0 >= csa->nl->wcs_timers))
        {
                start_timer((TID)reg,
                            csd->flush_time[0] * (dba_bg == reg->dyn.addr->acc_meth ? 1 : csd->defer_time),
                            &wcs_stale,
                            sizeof(region),
                            (char *)region);
                BG_TRACE_ANY(csa, stale_timer_started);
		TP_CHANGE_REG(save_region);	/* Restore to former glory */
                return;
        }
        /* We aren't creating a new timer so decrement the count for this one that is now done */
        DECR_CNT(&csa->nl->wcs_timers, &csa->nl->wc_var_lock);
        csa->timer = FALSE;                     /* No timer set for this region this process anymore */
	TP_CHANGE_REG(save_region);	/* Restore to former glory */
        return;
}
