/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsdbver.h"
#include "copy.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "mmemory.h"

GBLREF block_id		patch_find_blk, patch_left_sib, patch_right_sib;
GBLREF block_id		patch_path[MAX_BT_DEPTH + 1], patch_path1[MAX_BT_DEPTH + 1];
GBLREF boolean_t	patch_find_root_search;
GBLREF char		patch_comp_key[MAX_KEY_SZ + 1];
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short int	patch_path_count;
GBLREF unsigned short	patch_comp_count;

error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEINVALBLKID);

int dse_order(block_id srch,
	      block_id_ptr_t pp,
	      int4 *op,
	      char *targ_key,
	      short int targ_len,
	      bool dir_data_blk)
{
	block_id	last;
	boolean_t	long_blk_id;
	cache_rec_ptr_t	dummy_cr;
	int4		dummy_int;
	long		blk_id_size;
	short int	rsize, size;
	sm_uc_ptr_t	bp, b_top, key_top, ptr, rp, r_top;
	unsigned short	cc;

	last = 0;
	patch_path_count++;
	if (!(bp = t_qread(srch, &dummy_int, &dummy_cr)))
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER)
	{
#		ifdef BLK_NUM_64BIT
		long_blk_id = TRUE;
		blk_id_size = SIZEOF(block_id_64);
#		else
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEINVALBLKID);
#		endif
	} else
	{
		long_blk_id = FALSE;
		blk_id_size = SIZEOF(block_id_32);
	}
	if (((blk_hdr_ptr_t)bp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = bp + cs_addrs->hdr->blk_size;
	else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)bp)->bsiz)
		b_top = bp + SIZEOF(blk_hdr);
	else
		b_top = bp + ((blk_hdr_ptr_t)bp)->bsiz;
	CLEAR_DSE_COMPRESS_KEY;
	for (rp = bp + SIZEOF(blk_hdr); rp < b_top ;rp = r_top, last = *pp)
	{
		GET_SHORT(rsize, &((rec_hdr_ptr_t)rp)->rsiz);
		if (SIZEOF(rec_hdr) > rsize)
			r_top = rp + SIZEOF(rec_hdr);
		else
			r_top = rp + rsize;
		if ((r_top > b_top) || ((r_top == b_top) && ((blk_hdr*)bp)->levl))
		{
			if (((SIZEOF(rec_hdr) + blk_id_size) != (b_top - rp)) || EVAL_CMPC((rec_hdr *)rp))
				return FALSE;
			if (dir_data_blk && !(((blk_hdr_ptr_t)bp)->levl))
			{
				for (ptr = rp + SIZEOF(rec_hdr); (*ptr++ || *ptr++) && (ptr <= b_top);)
					;
				if (long_blk_id == TRUE)
#					ifdef BLK_NUM_64BIT
					GET_BLK_ID_64(*pp,ptr);
#					else
					rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				else
					GET_BLK_ID_32(*pp,ptr);
			} else
				if (long_blk_id == TRUE)
#					ifdef BLK_NUM_64BIT
					GET_BLK_ID_64(*pp,b_top - SIZEOF(block_id_64));
#					else
					rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				else
					GET_BLK_ID_32(*pp,b_top - SIZEOF(block_id_32));
			break;
		} else
		{
			if ((blk_id_size + SIZEOF(rec_hdr)) > (r_top - rp))
				break;
			if (dir_data_blk && !((blk_hdr_ptr_t)bp)->levl)
			{
				for (ptr = rp + SIZEOF(rec_hdr); (*ptr++ || *ptr++) && (ptr <= r_top);)
					;
				key_top = ptr;
			} else
				key_top = r_top - blk_id_size;
			if (EVAL_CMPC((rec_hdr_ptr_t)rp) > patch_comp_count)
				cc = patch_comp_count;
			else
				cc = EVAL_CMPC((rec_hdr_ptr_t)rp);
			size = key_top - rp - SIZEOF(rec_hdr);
			if ((SIZEOF(patch_comp_key) - 2 - cc) < size)
				size = SIZEOF(patch_comp_key) - 2 - cc;
			if (0 > size)
				size = 0;
			memcpy(&patch_comp_key[cc], rp + SIZEOF(rec_hdr), size);
			patch_comp_count = cc + size;
			if (long_blk_id == TRUE)
#				ifdef BLK_NUM_64BIT
				GET_BLK_ID_64(*pp, key_top);
#				else
				rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEINVALBLKID);
#				endif
			else
				GET_BLK_ID_32(*pp, key_top);
			if (0 >= memvcmp(targ_key, targ_len, patch_comp_key, patch_comp_count))
				break;
		}
	}
	assert((rp - bp) == (int4)(rp - bp));
	*op = (int4)(rp - bp);
	if ((*pp == patch_find_blk) && !dir_data_blk)
	{
		patch_left_sib = last;
		if (r_top < b_top)
		{
			rp = r_top;
			GET_SHORT(rsize, &((rec_hdr_ptr_t)r_top)->rsiz);
			r_top = rp + rsize;
			if (r_top > b_top)
				r_top = b_top;
			if (blk_id_size <= (r_top - rp))
			{
				if (long_blk_id == TRUE)
#					ifdef BLK_NUM_64BIT
					GET_BLK_ID_64(patch_right_sib, r_top - SIZEOF(block_id_64));
#					else
					rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				else
					GET_BLK_ID_32(patch_right_sib, r_top - SIZEOF(block_id_32));
			}
		}
		return TRUE;
	}
	if ((*pp > 0) && (*pp < cs_addrs->ti->total_blks) && (*pp % cs_addrs->hdr->bplmap))
	{
		if ((1 < ((blk_hdr_ptr_t)bp)->levl) && dse_order(*pp, pp + 1, op + 1, targ_key, targ_len, 0))
			return TRUE;
		else if ((1 == ((blk_hdr_ptr_t)bp)->levl) && patch_find_root_search)
			return dse_order(*pp, pp + 1, op + 1, targ_key, targ_len, 1);
		else if ((0 == ((blk_hdr_ptr_t)bp)->levl) && patch_find_root_search)
		{
			patch_find_root_search = FALSE;
			return TRUE;
		}
	}
	patch_path_count--;
	return FALSE;
}
