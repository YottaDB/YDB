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

#include "gtm_string.h"


#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "cli.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "util.h"
#include "skan_offset.h"
#include "skan_rnum.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF int		update_array_size;
GBLREF gd_addr		*gd_header;
GBLREF gd_region        *gv_cur_region;
GBLREF srch_hist	dummy_hist;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF block_id 	patch_curr_blk;
GBLREF char 		patch_comp_key[256];
GBLREF unsigned char 	patch_comp_count;
GBLREF cw_set_element   cw_set[];
GBLREF unsigned char	*non_tp_jfb_buff_ptr;

void dse_rmrec(void)
{
	block_id	blk;
	blk_segment	*bs1, *bs_ptr;
	int4		blk_seg_cnt, blk_size, count;
	sm_uc_ptr_t	bp;
	uchar_ptr_t	lbp, b_top, rp, r_top, key_top, rp_base;
	char		cc, comp_key[256], cc_base;
	short int	size, i, rsize;
	cw_set_element	*cse;
	error_def(ERR_DSEFAIL);
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DBRDONLY);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	assert(update_array);
	/* reset new block mechanism */
	update_array_ptr = update_array;
	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if(!cli_get_hex("BLOCK", &blk))
			return;
		if (blk < 0 || blk >= cs_addrs->ti->total_blks || !(blk % cs_addrs->hdr->bplmap))
		{
			util_out_print("Error: invalid block number.", TRUE);
			return;
		}
		patch_curr_blk = blk;
	}
	if (cli_present("COUNT") == CLI_PRESENT)
	{
		if (!cli_get_hex("COUNT", &count) || count < 1)
			return;
	} else
		count = 1;
	t_begin_crit(ERR_DSEFAIL);
	blk_size = cs_addrs->hdr->blk_size;
	if(!(bp = t_qread(patch_curr_blk, &dummy_hist.h[0].cycle, &dummy_hist.h[0].cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	lbp = (uchar_ptr_t)malloc(blk_size);
	memcpy(lbp, bp, blk_size);

	if (((blk_hdr_ptr_t)lbp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = lbp + cs_addrs->hdr->blk_size;
	else if (((blk_hdr_ptr_t)lbp)->bsiz < sizeof(blk_hdr))
		b_top = lbp + sizeof(blk_hdr);
	else
		b_top = lbp + ((blk_hdr_ptr_t)lbp)->bsiz;
	if (cli_present("RECORD") == CLI_PRESENT)
	{
		if (!(rp = rp_base = skan_rnum(lbp, FALSE)))
		{
			free(lbp);
			t_abort();
			return;
		}
	} else if (!(rp = rp_base = skan_offset(lbp, FALSE)))
	{
		free(lbp);
		t_abort();
		return;
	}
	memcpy(&comp_key[0], &patch_comp_key[0], sizeof(patch_comp_key));
	cc_base = patch_comp_count;
	for ( ; ; )
	{
		GET_SHORT(rsize, &((rec_hdr_ptr_t)rp)->rsiz);
		if (rsize < sizeof(rec_hdr))
			r_top = rp + sizeof(rec_hdr);
		else
			r_top = rp + rsize;
		if (r_top >= b_top)
		{
			if (count)
			{	if (((blk_hdr_ptr_t) lbp)->levl)
					util_out_print("Warning:  removed a star record from the end of this block.", TRUE);
				((blk_hdr_ptr_t)lbp)->bsiz = rp_base - lbp;
				BLK_INIT(bs_ptr, bs1);
				BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + sizeof(blk_hdr),
					(int)((blk_hdr_ptr_t)lbp)->bsiz - sizeof(blk_hdr));
				if (!BLK_FINI(bs_ptr, bs1))
				{
					util_out_print("Error: bad blk build.",TRUE);
					free(lbp);
					t_abort();
					return;
				}
				t_write(patch_curr_blk, (unsigned char *)bs1, 0, 0, bp, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE);
				BUILD_AIMG_IF_JNL_ENABLED(cs_addrs, cs_data, non_tp_jfb_buff_ptr, cse);
				t_end(&dummy_hist, 0);
				free(lbp);
				return;
			}
			r_top = b_top;
		}
		if (((blk_hdr_ptr_t)lbp)->levl)
			key_top = r_top - sizeof(block_id);
		else
		{
			for (key_top = rp + sizeof(rec_hdr); key_top < r_top; )
				if (!*key_top++ && !*key_top++)
					break;
		}
		if (((rec_hdr_ptr_t)rp)->cmpc > patch_comp_count)
			cc = patch_comp_count;
		else
			cc = ((rec_hdr_ptr_t)rp)->cmpc;
		size = key_top - rp - sizeof(rec_hdr);
		if (size < 0)
			size = 0;
		else if (size > sizeof(patch_comp_key) - 2)
			size = sizeof(patch_comp_key) - 2;
		memcpy(&patch_comp_key[cc], rp + sizeof(rec_hdr), size);
		patch_comp_count = cc + size;
		if (--count >= 0)
		{
			rp = r_top;
			continue;
		}
		size = (patch_comp_count < cc_base) ? patch_comp_count : cc_base;
		for (i = 0; i < size && patch_comp_key[i] == comp_key[i]; i++)
			;
		((rec_hdr_ptr_t)rp_base)->cmpc = i;
		rsize = r_top - key_top + sizeof(rec_hdr) + patch_comp_count - i;
		PUT_SHORT(&((rec_hdr_ptr_t)rp_base)->rsiz, rsize);
		memcpy(rp_base + sizeof(rec_hdr), &patch_comp_key[i], patch_comp_count - i);
		memcpy(rp_base + sizeof(rec_hdr) + patch_comp_count - i, key_top, b_top - key_top);
		((blk_hdr_ptr_t)lbp)->bsiz = rp_base + rsize - lbp + b_top - r_top;
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + sizeof(blk_hdr), ((blk_hdr_ptr_t)lbp)->bsiz - sizeof(blk_hdr));
		if (!BLK_FINI(bs_ptr, bs1))
		{
			util_out_print("Error: bad blk build.", TRUE);
			free(lbp);
			t_abort();
			return;
		}
		t_write(patch_curr_blk, (unsigned char *)bs1, 0, 0, bp, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE);
		BUILD_AIMG_IF_JNL_ENABLED(cs_addrs, cs_data, non_tp_jfb_buff_ptr, cse);
		t_end(&dummy_hist, 0);
		free(lbp);
		return;
	}
}
