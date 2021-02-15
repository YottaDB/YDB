/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*******************************************************************************
*
*	MODULE NAME:		PATCH_M_REST
*
*	CALLING SEQUENCE:	void dse_m_rest (blk, bml_list, bml_size)
*				block_id	blk;
*				unsigned char	*bml_list;
*				int4		bml_size;
*
*	DESCRIPTION:	This is a recursive routine kicked off by PATCH_MAPS
*			in the RESTORE_ALL function.  It reconstructs a
*			a local copy of all the local bit maps.
*
*	HISTORY:
*
*******************************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsdbver.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "copy.h"
#include "util.h"
#include "gdsbml.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"

GBLREF sgmnt_addrs	*cs_addrs;
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEINVALBLKID);

void dse_m_rest	(block_id		blk,		/* block number */
		 unsigned char		*bml_list,	/* start of local list of local bit maps */
		 int4			bml_size,	/* size of each entry in *bml_list */
		 v_block_id_ptr_t	blks_ptr,	/* total free blocks */
		 bool			in_dir_tree)
{
	block_id	next, bml_index;
	boolean_t	long_blk_id; /* does the current block use 64-bit block ids */
	cache_rec_ptr_t	dummy_cr;
	int4		dummy_int, bplmap, util_len;
	long		blk_id_size;
	short		level, rsize;
	sm_uc_ptr_t	bp, b_top, rp, r_top, bml_ptr, np, ptr;
	unsigned char	util_buff[MAX_UTIL_LEN];

	if (!(bp = t_qread (blk, &dummy_int, &dummy_cr)))
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = bp + cs_addrs->hdr->blk_size;
	else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
		b_top = bp + SIZEOF(blk_hdr);
	else
		b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
	if (((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER) /* Check blk version to see if using 32 or 64 bit block_id */
	{
#		ifdef BLK_NUM_64BIT
		long_blk_id = TRUE;
		blk_id_size = SIZEOF(block_id_64);
#		else
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#		endif
	} else
	{
		long_blk_id = FALSE;
		blk_id_size = SIZEOF(block_id_32);
	}
	level = ((blk_hdr_ptr_t)bp)->levl;
	bplmap = cs_addrs->hdr->bplmap;

	for (rp = bp + SIZEOF(blk_hdr); rp < b_top ;rp = r_top)
	{
		if (in_dir_tree || level > 1)	/* reread block because it may have been flushed from read */
		{
			if (!(np = t_qread(blk,&dummy_int,&dummy_cr))) /* cache due to LRU buffer scheme and reads in recursive */
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);	/* calls to dse_m_rest. */
			if (np != bp)
			{
				b_top = np + (b_top - bp);
				rp = np + (rp - bp);
				r_top = np + (r_top - bp);
				bp = np;
				if (((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER) /* Check if using 32 or 64 bit block_id */
				{
#					ifdef BLK_NUM_64BIT
					long_blk_id = TRUE;
					blk_id_size = SIZEOF(block_id_64);
#					else
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				} else
				{
					long_blk_id = FALSE;
					blk_id_size = SIZEOF(block_id_32);
				}
			}
		}
		GET_SHORT(rsize,&((rec_hdr_ptr_t)rp)->rsiz);
		r_top = rp + rsize;
		if (r_top > b_top)
			r_top = b_top;
		if ((r_top - rp) < (SIZEOF(rec_hdr) + blk_id_size))
			break;
		if (in_dir_tree && level == 0)
		{
			for (ptr = rp + SIZEOF(rec_hdr); ; )
			{
				if (*ptr++ == 0 && *ptr++ == 0)
					break;
			}
			if (long_blk_id == TRUE)
#				ifdef BLK_NUM_64BIT
				GET_BLK_ID_64(next,ptr);
#				else
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#				endif
			else
				GET_BLK_ID_32(next,ptr);
		}
		else
		{
			if (long_blk_id == TRUE)
#				ifdef BLK_NUM_64BIT
				GET_BLK_ID_64(next, r_top - SIZEOF(block_id_64));
#				else
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#				endif
			else
				GET_BLK_ID_32(next, r_top - SIZEOF(block_id_32));
		}
		if ((next < 0) || (next >= cs_addrs->ti->total_blks) || ((next / bplmap) * bplmap == next))
		{
			memcpy(util_buff,"Invalid pointer in block ",25);
			util_len = 25;
			util_len += i2hexl_nofill(blk, &util_buff[util_len], MAX_HEX_INT8);
			memcpy(&util_buff[util_len], " record offset ",15);
			util_len += 15;
			assert((rp - bp) == (int)(rp - bp)); /* (rp - bp) is a record offset in a block and should fit in an int */
			/* Using MAX_HEX_SHORT for int value because to save line space
			 * since the value should always fit in 2-bytes
			 */
			util_len += i2hex_nofill((int)(rp - bp), &util_buff[util_len], MAX_HEX_SHORT);
			util_buff[util_len] = 0;
			util_out_print((char*)util_buff,TRUE);
			continue;
		}
		bml_index = next / bplmap;
		bml_ptr = bml_list + (bml_index * bml_size);
		if (bml_busy(next - ((next / bplmap) * bplmap), bml_ptr + SIZEOF(blk_hdr)))
		{
			*blks_ptr = *blks_ptr - 1;
			if (((blk_hdr_ptr_t) bp)->levl > 1)
				dse_m_rest (next, bml_list, bml_size, blks_ptr, in_dir_tree);
			else if (in_dir_tree)
			{
				assert(((blk_hdr_ptr_t) bp)->levl == 0 || ((blk_hdr_ptr_t) bp)->levl == 1);
				dse_m_rest (next, bml_list, bml_size, blks_ptr, ((blk_hdr_ptr_t)bp)->levl);
			}
		}
	}
	return;
}
