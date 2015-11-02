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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "dsefind.h"
#include "copy.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"

GBLREF global_root_list	*global_roots_tail;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF block_id		patch_path[MAX_BT_DEPTH + 1];
GBLREF int4		patch_offset[MAX_BT_DEPTH + 1];
GBLREF short int	patch_path_count;

void dse_find_roots(block_id index)
{
	int		count;
	int4		dummy_int;
	cache_rec_ptr_t dummy_cr;
	short		temp_short;
	sm_uc_ptr_t	bp, b_top, rp, r_top, key_top;
	global_dir_path	*d_ptr;
	error_def(ERR_DSEBLKRDFAIL);

	if(!(bp = t_qread(index,&dummy_int,&dummy_cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = bp + cs_addrs->hdr->blk_size;
	else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
		b_top = bp + SIZEOF(blk_hdr);
	else
		b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
	for (rp = bp + SIZEOF(blk_hdr); rp < b_top ;rp = r_top)
	{	GET_SHORT(temp_short,&((rec_hdr_ptr_t)rp)->rsiz);
		r_top = rp + temp_short;
		if (r_top > b_top)
			r_top = b_top;
		if (r_top - rp < SIZEOF(block_id))
			break;
		global_roots_tail->link = (global_root_list *)malloc(SIZEOF(global_root_list));
		global_roots_tail = global_roots_tail->link;
		global_roots_tail->link = 0;
		for (key_top = rp + SIZEOF(rec_hdr); key_top < r_top; )
			if(!*key_top++ && !*key_top++)
				break;
		GET_LONG(global_roots_tail->root,key_top);
		global_roots_tail->dir_path = (global_dir_path *)malloc(SIZEOF(global_dir_path));
		d_ptr = global_roots_tail->dir_path;
		for (count = 0; ; count++)
		{	d_ptr->block = patch_path[count];
			d_ptr->offset = patch_offset[count];
			if (count < patch_path_count - 1)
				d_ptr->next = (global_dir_path *)malloc(SIZEOF(global_dir_path));
			else
			{	d_ptr->next = 0;
				d_ptr->offset = (int4)(rp - bp);
				break;
			}
			d_ptr = d_ptr->next;
		}
	}
	return;
}
