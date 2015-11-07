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
#include "skan_offset.h"
#include "skan_rnum.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"
#include "util.h"
#include "t_abort.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF uint4		update_array_size;
GBLREF srch_hist	dummy_hist;
GBLREF block_id		patch_curr_blk;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF gd_addr		*gd_header;
GBLREF char 		patch_comp_key[MAX_KEY_SZ + 1];
GBLREF unsigned short 	patch_comp_count;
GBLREF gd_region        *gv_cur_region;
GBLREF cw_set_element   cw_set[];

void dse_adrec(void)
{
	char		data[MAX_LINE], key[MAX_KEY_SZ + 1];
	unsigned short	cc;
	int		tmp_cmpc;
	sm_uc_ptr_t	new_bp, lbp, b_top, rp, r_top, key_top;
	short int	size, new_len, rsize;
	int		data_len, key_len;
	int4		blk_seg_cnt, blk_size;
	block_id	blk;
	blk_segment	*bs1, *bs_ptr;
	srch_blk_status	blkhist;

	error_def(ERR_CPBEYALLOC);
	error_def(ERR_DBRDONLY);
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DSEFAIL);
	error_def(ERR_GVIS);
	error_def(ERR_REC2BIG);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if(!cli_get_hex("BLOCK", (uint4 *)&blk))
			return;
		patch_curr_blk = blk;
	}
	if (patch_curr_blk < 0 || patch_curr_blk >= cs_addrs->ti->total_blks || !(patch_curr_blk % cs_addrs->hdr->bplmap))
	{
		util_out_print("Error: invalid block number.", TRUE);
		return;
	}
	if (cli_present("KEY") != CLI_PRESENT)
	{
		util_out_print("Error: key must be specified.", TRUE);
		return;
	}
	if (!dse_getki(&key[0], &key_len, LIT_AND_LEN("KEY")))
		return;
	t_begin_crit(ERR_DSEFAIL);
	blk_size = cs_addrs->hdr->blk_size;
	blkhist.blk_num = patch_curr_blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);

	lbp = (uchar_ptr_t)malloc(blk_size);
	memcpy(lbp, blkhist.buffaddr, blk_size);

	if (((blk_hdr_ptr_t)lbp)->bsiz > blk_size)
		((blk_hdr_ptr_t)lbp)->bsiz = blk_size;
	else if (((blk_hdr_ptr_t)lbp)->bsiz < SIZEOF(blk_hdr))
		((blk_hdr_ptr_t)lbp)->bsiz = SIZEOF(blk_hdr);

	b_top = lbp + ((blk_hdr_ptr_t)lbp)->bsiz;
	if (((blk_hdr_ptr_t)lbp)->levl)
	{
		if (cli_present("POINTER") != CLI_PRESENT)
		{
			util_out_print("Error: block pointer must be specified for this index block record.", TRUE);
			free(lbp);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		if (!cli_get_hex("POINTER", (uint4 *)&blk))
		{
			t_abort(gv_cur_region, cs_addrs);
			free(lbp);
			return;
		}
		if (blk < 0 || blk >= cs_addrs->ti->total_blks || !(blk % cs_addrs->hdr->bplmap))
		{
			util_out_print("Error: pointer is an invalid block number.", TRUE);
			free(lbp);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		MEMCP(&data[0], (char *)&blk, 0, SIZEOF(block_id), SIZEOF(block_id));
		data_len = SIZEOF(block_id);
	} else
	{
		if (cli_present("DATA") != CLI_PRESENT)
		{
			util_out_print("Error:  data must be specified for this data block record.", TRUE);
			free(lbp);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		if (FALSE == dse_data(&data[0], &data_len))
		{
			free(lbp);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
		if (key_len + data_len >  cs_addrs->hdr->max_rec_size)
		{
 			rts_error(VARLSTCNT(10) ERR_REC2BIG, 4, key_len + data_len, (int4)cs_addrs->hdr->max_rec_size,
								REG_LEN_STR(gv_cur_region), ERR_GVIS, 2, LEN_AND_STR(key));
		}

	}
	if (cli_present("RECORD") == CLI_PRESENT)
	{
		if (!(rp = skan_rnum(lbp, TRUE)))
		{
			free(lbp);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
	} else if (cli_present("OFFSET") == CLI_PRESENT)
	{
		if (!(rp = skan_offset(lbp, TRUE)))
		{
			free(lbp);
			t_abort(gv_cur_region, cs_addrs);
			return;
		}
	} else
	{
		util_out_print("Error:  must specify a record number or offset for the record to be added.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}

	new_bp = (uchar_ptr_t)malloc(blk_size);
	size = (key_len < patch_comp_count) ? key_len : patch_comp_count;
	for (cc = 0; cc < size && patch_comp_key[cc] == key[cc]; cc++)
		;
	SET_CMPC((rec_hdr_ptr_t)new_bp, cc);
	new_len = key_len - cc + data_len + SIZEOF(rec_hdr);
	PUT_SHORT(&((rec_hdr_ptr_t)new_bp)->rsiz, new_len);
	MEMCP(new_bp, &key[cc], SIZEOF(rec_hdr), key_len - cc, blk_size);
	MEMCP(new_bp, &data[0], SIZEOF(rec_hdr) + key_len - cc, data_len, blk_size);
	if (rp < b_top)
	{
		GET_SHORT(rsize, &((rec_hdr_ptr_t)rp)->rsiz);
		if (rsize < SIZEOF(rec_hdr))
			r_top = rp + SIZEOF(rec_hdr);
		else
			r_top = rp + rsize;
		if (r_top >= b_top)
			r_top = b_top;
		if (((blk_hdr_ptr_t)lbp)->levl)
			key_top = r_top - SIZEOF(block_id);
		else
		{
			for (key_top = rp + SIZEOF(rec_hdr); key_top < r_top; )
				if (!*key_top++ && !*key_top++)
					break;
		}
		if (EVAL_CMPC((rec_hdr_ptr_t)rp) > patch_comp_count)
			cc = patch_comp_count;
		else
			cc = EVAL_CMPC((rec_hdr_ptr_t)rp);
		size = key_top - rp - SIZEOF(rec_hdr);
		if (size > SIZEOF(patch_comp_key) - 2 - cc)
			size = SIZEOF(patch_comp_key) - 2 - cc;
		if (size < 0)
			size = 0;
		memcpy(&patch_comp_key[cc], rp + SIZEOF(rec_hdr), size);
		patch_comp_count = cc + size;
		size = (key_len < patch_comp_count) ? key_len : patch_comp_count;
		for (cc = 0; cc < size && patch_comp_key[cc] ==  key[cc]; cc++)
			;
		SET_CMPC((rec_hdr_ptr_t)(new_bp + new_len), cc);
		rsize = patch_comp_count - cc + r_top - key_top + SIZEOF(rec_hdr);
		PUT_SHORT(&((rec_hdr_ptr_t)(new_bp + new_len))->rsiz, rsize);
		MEMCP(new_bp, &patch_comp_key[cc], new_len + SIZEOF(rec_hdr), patch_comp_count - cc, blk_size);
		MEMCP(new_bp, key_top, new_len + SIZEOF(rec_hdr) + patch_comp_count - cc, b_top - key_top, blk_size);
		new_len += patch_comp_count - cc + SIZEOF(rec_hdr) + b_top - key_top;
	}
	if (rp - lbp + new_len > blk_size)
	{
		util_out_print("Error: record too large for remaining space in block.", TRUE);
		free(lbp);
		free(new_bp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	memcpy(rp, new_bp, new_len);
	free(new_bp);
	((blk_hdr_ptr_t)lbp)->bsiz += new_len + (unsigned int)(rp - b_top);

	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + SIZEOF(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
	BUILD_AIMG_IF_JNL_ENABLED(cs_data, cs_addrs->ti->curr_tn);
	t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);

	free(lbp);
	return;
}
