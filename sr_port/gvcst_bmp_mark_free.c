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

/* gvcst_bmp_mark_free.c
	This marks all the blocks in kill set list to be marked free.
	Note ks must be already sorted
*/
#include "mdef.h"
#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "hashtab.h"     	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "t_write_map.h"
#include "mm_read.h"
#include "gvcst_bmp_mark_free.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF cw_set_element	cw_set[];
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF unsigned char	rdfail_detail;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF inctn_opcode_t   inctn_opcode;
GBLREF boolean_t        mu_reorg_process;

trans_num gvcst_bmp_mark_free(kill_set *ks)
{
	block_id	bit_map, next_bm;
	blk_ident	*blk, *blk_top, *next_blk;
	trans_num	ctn;
	unsigned int	len;
	int4		cycle;
	cache_rec_ptr_t	cr;
	srch_hist	alt_hist;
	inctn_opcode_t  save_inctn_opcode;
	sm_uc_ptr_t	bmp;
	trans_num	ret_tn = 0;

	error_def(ERR_GVKILLFAIL);
	if (JNL_ENABLED(cs_data))
        {
                /* This function is called for TP/non-TP/reorg. They have set inctn_opcode and
                 * do not expect to change. So save it before using the global.   */
                save_inctn_opcode = inctn_opcode;
                if (mu_reorg_process)
                        inctn_opcode = inctn_bmp_mark_free_mu_reorg;
                else
                        inctn_opcode = inctn_bmp_mark_free_gtm;
        }
	alt_hist.h[0].blk_num = 0;	/* need for calls to T_END for bitmaps */
	for (blk = &ks->blk[0], blk_top = &ks->blk[ks->used]; blk < blk_top;  blk = next_blk)
	{
		if (0 != blk->flag)
		{
		    next_blk = blk + 1;
		    continue;
		}
		assert(0 < blk->block);
		assert((int4)blk->block <= cs_addrs->ti->total_blks);
		bit_map = ROUND_DOWN2((int)blk->block, BLKS_PER_LMAP);
		next_bm = bit_map + BLKS_PER_LMAP;
		/* Scan for the next local bitmap */
		for (next_blk = blk; (0 == next_blk->flag) && (next_blk < blk_top)
		     && ((block_id)next_blk->block < next_bm);  ++next_blk)
			;
		update_array_ptr = update_array;
		len = (char *)next_blk - (char *)blk;
		memcpy(update_array_ptr, blk, len);
		update_array_ptr += len;
		/* the following assumes sizeof(blk_ident) == sizeof(int) */
		*(int *)update_array_ptr = 0;
		t_begin(ERR_GVKILLFAIL, TRUE);
		for (;;)
		{
			ctn = cs_addrs->ti->curr_tn;
			if (dba_mm == cs_addrs->hdr->acc_meth)
			{
				if (NULL == (bmp = mm_read(bit_map)))
				{
					t_retry(rdfail_detail);
					continue;
				}
			} else
			{
				if (NULL == (bmp = t_qread(bit_map, (sm_int_ptr_t)&cycle, &cr)))
				{
					t_retry(rdfail_detail);
					continue;
				}
				cw_set[0].cr = cr;
				cw_set[0].cycle = cycle;
			}
			t_write_map(bit_map, bmp, (uchar_ptr_t)update_array, ctn);
			if (0 == (ret_tn = t_end(&alt_hist, NULL)))
				continue;
			break;
		}
	}       /* for (all blocks in the kill_set */
	if (JNL_ENABLED(cs_data))
                inctn_opcode = save_inctn_opcode;

	return ret_tn;
}
