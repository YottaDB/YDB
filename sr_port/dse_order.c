/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "copy.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "mmemory.h"

GBLREF short int	patch_path_count;
GBLREF block_id		patch_left_sib,patch_right_sib,patch_find_blk;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF char		patch_comp_key[MAX_KEY_SZ + 1];
GBLREF unsigned char	patch_comp_count;
GBLREF bool		patch_find_root_search;

int dse_order(block_id srch,
	      block_id_ptr_t pp,
	      int4 *op,
	      char *targ_key,
	      short int targ_len,
	      bool dir_data_blk)
{
	sm_uc_ptr_t	bp, b_top, rp, r_top, key_top, c1, ptr;
	unsigned char	cc;
	block_id	last;
	short int	size, rsize;
	int4		dummy_int;
	cache_rec_ptr_t	dummy_cr;
	error_def(ERR_DSEBLKRDFAIL);

	last = 0;
	patch_path_count++;
	if(!(bp = t_qread(srch, &dummy_int, &dummy_cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = bp + cs_addrs->hdr->blk_size;
	else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
		b_top = bp + SIZEOF(blk_hdr);
	else
		b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
	patch_comp_count = 0;
	patch_comp_key[0] = patch_comp_key[1] = 0;
	for (rp = bp + SIZEOF(blk_hdr); rp < b_top ;rp = r_top, last = *pp)
	{
		GET_SHORT(rsize,&((rec_hdr_ptr_t)rp)->rsiz);
		if (rsize < SIZEOF(rec_hdr))
			r_top = rp + SIZEOF(rec_hdr);
		else
			r_top = rp + rsize;
		if (r_top > b_top || (r_top == b_top && ((blk_hdr*)bp)->levl))
		{
			if (b_top - rp != SIZEOF(rec_hdr) + SIZEOF(block_id) || ((rec_hdr *) rp)->cmpc)
				return FALSE;
			if (dir_data_blk && !((blk_hdr_ptr_t)bp)->levl)
			{
				for (ptr = rp + SIZEOF(rec_hdr); ;)
					if (*ptr++ == 0 && *ptr++ == 0)
						break;
				GET_LONGP(pp,ptr);
			} else
				GET_LONGP(pp,b_top - SIZEOF(block_id));
			break;
		} else
		{
			if (r_top - rp < SIZEOF(block_id) + SIZEOF(rec_hdr))
				break;
			if (dir_data_blk && !((blk_hdr_ptr_t)bp)->levl)
			{
				for (ptr = rp + SIZEOF(rec_hdr); ;)
					if (*ptr++ == 0 && *ptr++ == 0)
						break;
				key_top = ptr;
			} else
				key_top = r_top - SIZEOF(block_id);
			if (((rec_hdr_ptr_t) rp)->cmpc > patch_comp_count)
				cc = patch_comp_count;
			else
				cc = ((rec_hdr_ptr_t) rp)->cmpc;
			size = key_top - rp - SIZEOF(rec_hdr);
			if (size > SIZEOF(patch_comp_key) - 2 - cc)
				size = SIZEOF(patch_comp_key) - 2 - cc;
			if (size < 0)
				size = 0;
			memcpy(&patch_comp_key[cc], rp + SIZEOF(rec_hdr), size);
			patch_comp_count = cc + size;
			GET_LONGP(pp,key_top);
			if (memvcmp(targ_key,targ_len,&patch_comp_key[0],patch_comp_count) <= 0)
				break;
		}
	}
	*op = (int4)(rp - bp);
	if (*pp == patch_find_blk && !dir_data_blk)
	{
		patch_left_sib = last;
		if (r_top < b_top)
		{
			rp = r_top;
			GET_SHORT(rsize,&((rec_hdr_ptr_t)r_top)->rsiz);
			r_top = rp + rsize;
			if (r_top > b_top)
				r_top = b_top;
			if (r_top - rp >= SIZEOF(block_id))
				GET_LONG(patch_right_sib,r_top - SIZEOF(block_id));
		}
		return TRUE;
	}
	if ( *pp > 0 && *pp < cs_addrs->ti->total_blks && (*pp % cs_addrs->hdr->bplmap))
	{
		if (((blk_hdr_ptr_t) bp)->levl > 1 && dse_order(*pp,pp + 1,op + 1,targ_key,targ_len, 0))
			return TRUE;
		else if (((blk_hdr_ptr_t)bp)->levl == 1 && patch_find_root_search)
			return dse_order( *pp,pp + 1,op + 1, targ_key, targ_len, 1);
		else if (((blk_hdr_ptr_t)bp)->levl == 0 && patch_find_root_search)
			return TRUE;
	}
	patch_path_count--;
	return FALSE;
}
