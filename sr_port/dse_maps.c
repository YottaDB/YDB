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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "cli.h"
#include "util.h"
#include "mupipbckup.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "dse_is_blk_free.h"
#include "bit_set.h"
#include "bit_clear.h"
#include "t_write.h"
#include "gvcst_blk_build.h"
#include "cws_insert.h"
#include "jnl_write_aimg_rec.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_timer_start.h"
#ifdef UNIX
#include "process_deferred_stale.h"
#endif

#define MAX_UTIL_LEN 80

GBLREF char             *update_array, *update_array_ptr;
GBLREF int		update_array_size;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF block_id         patch_curr_blk;
GBLREF gd_region        *gv_cur_region;
GBLREF short            crash_count;
GBLREF cw_set_element   cw_set[];
GBLREF unsigned char    cw_set_depth;
GBLREF cache_rec	*cr_array[((MAX_BT_DEPTH * 2) - 1) * 2];	/* Maximum number of blocks that can be in transaction */
GBLREF unsigned int	cr_array_index;
GBLREF boolean_t	block_saved;
GBLREF boolean_t        unhandled_stale_timer_pop;
GBLREF unsigned char    *non_tp_jfb_buff_ptr;

void    cws_reset(void);

void dse_maps(void)
{
        block_id        blk, bml_blk;
        blk_segment     *bs1, *bs_ptr;
	cw_set_element  *cse;
        int4            blk_seg_cnt, blk_size;		/* needed for BLK_INIT, BLK_SEG and BLK_FINI macros */
        sgm_info        *dummysi = NULL;
        sm_uc_ptr_t     bp;
        char            util_buff[MAX_UTIL_LEN];
        int4            bml_size, bml_list_size, blk_index, bml_index;
        int4            total_blks;
        int4            bplmap, dummy_int;
        bool            dummy_bool;
        unsigned char   *bml_list;
        cache_rec_ptr_t cr, dummy_cr;
        bt_rec_ptr_t    btr;
        int             util_len;
        uchar_ptr_t     blk_ptr;
        bool            was_crit;
	uint4		jnl_status;

        error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DBRDONLY);

	if (CLI_PRESENT == cli_present("BUSY") || CLI_PRESENT == cli_present("FREE") ||
		CLI_PRESENT == cli_present("MASTER") || CLI_PRESENT == cli_present("RESTORE_ALL"))
        {
	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
        }
        assert(update_array);
        /* reset new block mechanism */
        update_array_ptr = update_array;
        assert(&FILE_INFO(gv_cur_region)->s_addrs == cs_addrs);
        was_crit = cs_addrs->now_crit;
        if (cs_addrs->critical)
                crash_count = cs_addrs->critical->crashcnt;
        bplmap = cs_addrs->hdr->bplmap;
        if (CLI_PRESENT == cli_present("BLOCK"))
        {
                if (!cli_get_hex("BLOCK", &blk))
                        return;
                if (blk < 0 || blk >= cs_addrs->ti->total_blks)
                {
                        util_out_print("Error: invalid block number.", TRUE);
                        return;
                }
                patch_curr_blk = blk;
        }
        else
                blk = patch_curr_blk;
        if (CLI_PRESENT == cli_present("FREE"))
        {
                if (0 == bplmap)
                {
                        util_out_print("Cannot perform map updates:  bplmap field of file header is zero.", TRUE);
                        return;
                }
                if (blk / bplmap * bplmap == blk)
                {
                        util_out_print("Cannot perform action on a map block.", TRUE);
                        return;
                }
                bml_blk = blk / bplmap * bplmap;
                bm_setmap(bml_blk, blk, FALSE);
                return;
        }
        if (CLI_PRESENT == cli_present("BUSY"))
        {
                if (0 == bplmap)
                {
                        util_out_print("Cannot perform map updates:  bplmap field of file header is zero.", TRUE);
                        return;
                }
                if (blk / bplmap * bplmap == blk)
                {
                        util_out_print("Cannot perform action on a map block.", TRUE);
                        return;
                }
                bml_blk = blk / bplmap * bplmap;
                bm_setmap(bml_blk, blk, TRUE);
                return;
        }
        blk_size = cs_addrs->hdr->blk_size;
        if (CLI_PRESENT == cli_present("MASTER"))
        {
                if (0 == bplmap)
                {
                        util_out_print("Cannot perform maps updates:  bplmap field of file header is zero.", TRUE);
                        return;
                }
                if (!was_crit)
                        grab_crit(gv_cur_region);
                bml_blk = blk / bplmap * bplmap;
                if (dba_mm == cs_addrs->hdr->acc_meth)
                        bp = (sm_uc_ptr_t)cs_addrs->acc_meth.mm.base_addr + (off_t)bml_blk * blk_size;
                else
                {
                        assert(dba_bg == cs_addrs->hdr->acc_meth);
                        if (!(bp = t_qread(bml_blk, &dummy_int, &dummy_cr)))
                                rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
                }
                if ((cs_addrs->ti->total_blks / bplmap) * bplmap == bml_blk)
                        total_blks = (cs_addrs->ti->total_blks - bml_blk);
                else
                        total_blks = bplmap;
                if (-1 == bml_find_free(0, bp + sizeof(blk_hdr), total_blks, &dummy_bool))
                {
                        bit_clear(bml_blk / bplmap, cs_addrs->bmm);
                        if (bml_blk > cs_addrs->nl->highest_lbm_blk_changed)
                                cs_addrs->nl->highest_lbm_blk_changed = bml_blk;
                }
                else
                {
                        bit_set(bml_blk / bplmap, cs_addrs->bmm);
                        if (bml_blk > cs_addrs->nl->highest_lbm_blk_changed)
                                cs_addrs->nl->highest_lbm_blk_changed = bml_blk;
                }
                if (!was_crit)
                        rel_crit(gv_cur_region);
                return;
        }
        if (CLI_PRESENT == cli_present("RESTORE_ALL"))
        {
                if (0 == bplmap)
                {
                        util_out_print("Cannot perform maps updates:  bplmap field of file header is zero.", TRUE);
                        return;
                }
                assert(ROUND_DOWN2(blk_size, 2 * sizeof(int4)) == blk_size);
                bml_size = BM_SIZE(bplmap);
                bml_list_size = (cs_addrs->ti->total_blks + bplmap - 1) / bplmap * bml_size;
                bml_list = (unsigned char *)malloc(bml_list_size);
                for (blk_index = 0, bml_index = 0;  blk_index < cs_addrs->ti->total_blks;
                        blk_index += bplmap, bml_index++)
                        bml_newmap((blk_hdr_ptr_t)(bml_list + bml_index * bml_size), bml_size, cs_addrs->ti->curr_tn);
                if (!was_crit)
                        grab_crit(gv_cur_region);
                blk = get_dir_root();
                assert(blk < bplmap);
                cs_addrs->ti->free_blocks = cs_addrs->ti->total_blks - (cs_addrs->ti->total_blks / bplmap + 1);
                bml_busy(blk, bml_list + sizeof(blk_hdr));
                cs_addrs->ti->free_blocks =  cs_addrs->ti->free_blocks - 1;
                dse_m_rest(blk, bml_list, bml_size, &cs_addrs->ti->free_blocks, TRUE);
                for (blk_index = 0, bml_index = 0;  blk_index < cs_addrs->ti->total_blks;
                                                                blk_index += bplmap, bml_index++)
                {
                        cws_reset();
                        cw_set_depth = 0;
                        update_array_ptr = update_array;
                        assert(cs_addrs->ti->early_tn == cs_addrs->ti->curr_tn);
                        cs_addrs->ti->early_tn++;
                        blk_ptr = bml_list + bml_index * bml_size;
                        bp = t_qread(blk_index, &dummy_int, &dummy_cr);
                        BLK_INIT(bs_ptr, bs1);
                        BLK_SEG(bs_ptr, blk_ptr + sizeof(blk_hdr), bml_size - sizeof(blk_hdr));
                        BLK_FINI(bs_ptr, bs1);
                        t_write(blk_index, (unsigned char *)bs1, 0, 0, bp, LCL_MAP_LEVL, TRUE, FALSE);
			cr_array_index = 0;
			block_saved = FALSE;
			if (JNL_ENABLED(cs_data))
			{
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
				{
					cse = (cw_set_element *)(&cw_set[0]);
					cse->new_buff = non_tp_jfb_buff_ptr;
					gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, cs_addrs->ti->curr_tn);
					cse->done = TRUE;
					jnl_write_aimg_rec(cs_addrs, cse->blk, (blk_hdr_ptr_t)cse->new_buff);
				}
				else
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data),
						DB_LEN_STR(gv_cur_region));
			}
                        if (dba_bg == cs_addrs->hdr->acc_meth)
                                bg_update(cw_set, cw_set + cw_set_depth, cs_addrs->ti->curr_tn, cs_addrs->ti->curr_tn, dummysi);
                        else
                                mm_update(cw_set, cw_set + cw_set_depth, cs_addrs->ti->curr_tn, cs_addrs->ti->curr_tn, dummysi);
                        cs_addrs->ti->curr_tn++;
                        assert(cs_addrs->ti->curr_tn == cs_addrs->ti->early_tn);
			while (cr_array_index)
				cr_array[--cr_array_index]->in_cw_set = FALSE;
			if (block_saved)
				backup_buffer_flush(gv_cur_region);
			wcs_timer_start(gv_cur_region, TRUE);
                }
                /* Fill in master map */
                for (blk_index = 0, bml_index = 0;  blk_index < cs_addrs->ti->total_blks;
                        blk_index += bplmap, bml_index++)
                {
                        if (-1 != bml_find_free(0, (bml_list + bml_index * bml_size) + sizeof(blk_hdr),
                                 bplmap, &dummy_bool))
                        {
                                bit_set(blk_index / bplmap, cs_addrs->bmm);
                                if (blk_index > cs_addrs->nl->highest_lbm_blk_changed)
                                        cs_addrs->nl->highest_lbm_blk_changed = blk_index;
                        } else
                        {
                                bit_clear(blk_index / bplmap, cs_addrs->bmm);
                                if (blk_index > cs_addrs->nl->highest_lbm_blk_changed)
                                        cs_addrs->nl->highest_lbm_blk_changed = blk_index;
                        }
                }
                /* last local map may be smaller than bplmap so redo with correct bit count */
                if (-1 != bml_find_free(0, bml_list + (bml_index - 1) * bml_size + sizeof(blk_hdr),
                        (cs_addrs->ti->total_blks - cs_addrs->ti->total_blks / bplmap * bplmap), &dummy_bool))
                {
                        bit_set(blk_index / bplmap - 1, cs_addrs->bmm);
                        if (blk_index > cs_addrs->nl->highest_lbm_blk_changed)
                                cs_addrs->nl->highest_lbm_blk_changed = blk_index;
                } else
                {
                        bit_clear(blk_index / bplmap - 1, cs_addrs->bmm);
                        if (blk_index > cs_addrs->nl->highest_lbm_blk_changed)
                                cs_addrs->nl->highest_lbm_blk_changed = blk_index;
                }
                if (!was_crit)
                        rel_crit(gv_cur_region);
#ifdef UNIX
		if (unhandled_stale_timer_pop)
			process_deferred_stale();
#endif
                free(bml_list);
		cs_addrs->hdr->kill_in_prog = 0;
                return;
        }
        memcpy(util_buff, "!/Block ", sizeof("!/Block ") - 1 );
        util_len = sizeof("!/Block ") - 1;
        util_len += i2hex_nofill(blk, (uchar_ptr_t)&util_buff[util_len], 8);
        memcpy(&util_buff[util_len], " is marked !AD in its local bit map.!/",
		sizeof(" is marked !AD in its local bit map.!/") - 1);
        util_len += sizeof(" is marked !AD in its local bit map.!/") - 1;
        util_buff[util_len] = 0;
        if (!was_crit)
                grab_crit(gv_cur_region);
        util_out_print(util_buff, TRUE, 4, dse_is_blk_free(blk, &dummy_int, &dummy_cr) ? "free" : "busy");
        if (!was_crit)
                rel_crit(gv_cur_region);
        return;
}
