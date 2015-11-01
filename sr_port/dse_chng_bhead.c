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
#include "mupipbckup.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "t_write_map.h"
#include "gvcst_blk_build.h"
#include "jnl_write_aimg_rec.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_timer_start.h"
#ifdef UNIX
#include "process_deferred_stale.h"
#endif
#include "util.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF int		update_array_size;
GBLREF srch_hist	dummy_hist;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF block_id		patch_curr_blk;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;
GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF cache_rec	*cr_array[((MAX_BT_DEPTH * 2) - 1) * 2];	/* Maximum number of blocks that can be in transaction */
GBLREF unsigned int	cr_array_index;
GBLREF boolean_t	block_saved;
GBLREF boolean_t        unhandled_stale_timer_pop;
GBLREF unsigned char    *non_tp_jfb_buff_ptr;

void dse_chng_bhead(void)
{
	block_id	blk;
	block_id	*blkid_ptr;
	sgm_info	*dummysi = NULL;
	int4		x;
	cache_rec_ptr_t	cr;
	uchar_ptr_t	bp;
	sm_uc_ptr_t	blkBase;
	blk_hdr		new_hdr;
	blk_segment	*bs1, *bs_ptr;
	cw_set_element  *cse;
	int4		blk_seg_cnt, blk_size;	/* needed for BLK_INIT,BLK_SEG and BLK_FINI macros */
	bool		ismap;
	bool		chng_blk;
	uint4		mapsize;
	uint4           jnl_status;

	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DSEFAIL);
	error_def(ERR_DBRDONLY);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	assert(update_array);
	/* reset new block mechanism */
	update_array_ptr = update_array;
	chng_blk = FALSE;
	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK",&blk))
			return;
		if (blk < 0 || blk > cs_addrs->ti->total_blks)
		{	util_out_print("Error: invalid block number.",TRUE);
			return;
		}
		patch_curr_blk = blk;
	}
	blk_size = cs_addrs->hdr->blk_size;
	ismap = (patch_curr_blk / cs_addrs->hdr->bplmap * cs_addrs->hdr->bplmap == patch_curr_blk);
	mapsize = BM_SIZE(cs_addrs->hdr->bplmap);

	t_begin_crit (ERR_DSEFAIL);
	if (!(bp = t_qread (patch_curr_blk,&dummy_hist.h[0].cycle,&dummy_hist.h[0].cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	new_hdr = *(blk_hdr_ptr_t)bp;

	if (cli_present("LEVEL") == CLI_PRESENT)
	{
		if (!cli_get_num("LEVEL",&x))
		{
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
		if (ismap && (unsigned char)x != LCL_MAP_LEVL)
		{
			util_out_print("Error: invalid level for a bit map block.",TRUE);
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
		if (!ismap && (x < 0 || x > MAX_BT_DEPTH + 1))
		{
			util_out_print("Error: invalid level.",TRUE);
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
	 	new_hdr.levl = (unsigned char)x;

		chng_blk = TRUE;
		if (new_hdr.bsiz < sizeof(blk_hdr))
			new_hdr.bsiz = sizeof(blk_hdr);
		if (new_hdr.bsiz  > blk_size)
			new_hdr.bsiz = blk_size;
	}
	if (cli_present("BSIZ") == CLI_PRESENT)
	{
		if (!cli_get_hex("BSIZ",&x))
		{
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
		if (ismap && x != mapsize)
		{
			util_out_print("Error: invalid bsiz.",TRUE);
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
		else if (x < sizeof(blk_hdr) || x > blk_size)
		{
			util_out_print("Error: invalid bsiz.",TRUE);
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
		chng_blk = TRUE;
		new_hdr.bsiz = x;
	}
	if (!chng_blk)
	{
		T_ABORT(gv_cur_region, cs_addrs);	/* note : T_ABORT() is a multi-line macro, hence surrounded by parens */
	} else
	{
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, bp + sizeof(new_hdr), new_hdr.bsiz - sizeof(new_hdr));
		if (!BLK_FINI(bs_ptr, bs1))
		{
			util_out_print("Error: bad block build.",TRUE);
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
		t_write (patch_curr_blk, (unsigned char *)bs1, 0, 0, bp, new_hdr.levl, TRUE, FALSE);
		BUILD_AIMG_IF_JNL_ENABLED(cs_addrs, cs_data, non_tp_jfb_buff_ptr, cse);
		t_end(&dummy_hist, 0);
	}
	if (cli_present("TN") == CLI_PRESENT)
	{
		if (!cli_get_hex("TN",&x))
			return;
		if (x < 0)
		{
			if (x != -1)
			{
				util_out_print("Error: invalid transaction number.",TRUE);
				T_ABORT(gv_cur_region, cs_addrs);
				return;
			}
		}
		t_begin_crit(ERR_DSEFAIL);
		assert(cs_addrs->ti->early_tn == cs_addrs->ti->curr_tn);
		cs_addrs->ti->early_tn++;
		blkBase = t_qread(patch_curr_blk, &dummy_hist.h[0].cycle, &dummy_hist.h[0].cr);
		if (NULL == blkBase)
		{
			rel_crit(gv_cur_region);
			util_out_print("Error: Unable to read buffer.", TRUE);
			T_ABORT(gv_cur_region, cs_addrs);
			return;
		}
		/* Create a null update array for a block */
		if (ismap)
		{
			BLK_ADDR(blkid_ptr, sizeof(block_id), block_id);
			*blkid_ptr = 0;
			t_write_map(patch_curr_blk, blkBase, (unsigned char *)blkid_ptr, cs_addrs->ti->curr_tn);
			cr_array_index = 0;
			block_saved = FALSE;
		} else
		{
			BLK_INIT(bs_ptr, bs1);
			BLK_SEG(bs_ptr, bp + sizeof(new_hdr), new_hdr.bsiz - sizeof(new_hdr));
			BLK_FINI(bs_ptr, bs1);
			t_write(patch_curr_blk, (unsigned char *)bs1, 0, 0, blkBase,
						((blk_hdr_ptr_t)blkBase)->levl, TRUE, FALSE);
			cr_array_index = 0;
			block_saved = FALSE;
			if (JNL_ENABLED(cs_data))
			{
				jnl_status = jnl_ensure_open();
				if (0 == jnl_status)
				{
					cse = (cw_set_element *)(&cw_set[0]);
					cse->new_buff = non_tp_jfb_buff_ptr;
					gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, x);
					cse->done = TRUE;
					if (0 == cs_addrs->jnl->pini_addr)
						jnl_put_jrt_pini(cs_addrs);
					jnl_write_aimg_rec(cs_addrs, cse->blk, (blk_hdr_ptr_t)cse->new_buff);
				} else
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
			}
		}
		/* Pass the desired tn "x" as argument to bg_update or mm_update */
		if (dba_bg == cs_addrs->hdr->acc_meth)
			bg_update(cw_set, cw_set + cw_set_depth, cs_addrs->ti->curr_tn, x, dummysi);
		else
			mm_update(cw_set, cw_set + cw_set_depth, cs_addrs->ti->curr_tn, x, dummysi);
		cs_addrs->ti->curr_tn++;
		assert(cs_addrs->ti->early_tn == cs_addrs->ti->curr_tn);
		/* the following code is analogous to that in t_end and should be maintained in a similar fashion */
		while (cr_array_index)
			cr_array[--cr_array_index]->in_cw_set = FALSE;
		rel_crit(gv_cur_region);
		if (block_saved)
			backup_buffer_flush(gv_cur_region);
		UNIX_ONLY(
			if (unhandled_stale_timer_pop)
				process_deferred_stale();
		)
		wcs_timer_start(gv_cur_region, TRUE);
	}
	return;
}
