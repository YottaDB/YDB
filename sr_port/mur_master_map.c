/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "gdsbml.h"

/* Include prototypes */
#include "bit_set.h"
#include "bit_clear.h"
#include "dbfilop.h"

GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF 	int		mur_regno;

void	mur_master_map()
{
	uchar_ptr_t	bml_buffer;
	uint4		bplmap, blk_index, bml_size;
	bool		dummy;
	file_control	*db_ctl;

	assert(cs_addrs == mur_ctl[mur_regno].csa);
	db_ctl = mur_ctl[mur_regno].db_ctl;
	bplmap = cs_addrs->hdr->bplmap;
	bml_size = ROUND_UP(BML_BITS_PER_BLK * bplmap + sizeof(blk_hdr), 8);
	bml_buffer = (uchar_ptr_t)malloc(bml_size);
	for (blk_index = 0;  blk_index < cs_addrs->ti->total_blks;  blk_index += bplmap)
	{
		/* Read local bit map into buffer */
		db_ctl->op = FC_READ;
		db_ctl->op_buff = bml_buffer;
		db_ctl->op_len = bml_size;
		db_ctl->op_pos = cs_addrs->hdr->start_vbn + cs_addrs->hdr->blk_size / DISK_BLOCK_SIZE * blk_index;
		dbfilop(db_ctl);
		if (bml_find_free(0, bml_buffer + sizeof(blk_hdr), bplmap, &dummy) == NO_FREE_SPACE)
			bit_clear(blk_index / bplmap, cs_addrs->bmm);
		else
			bit_set(blk_index / bplmap, cs_addrs->bmm);
	}

	/* Last local map may be smaller than bplmap so redo with correct bit count */
	if (bml_find_free(0, bml_buffer + sizeof(blk_hdr), cs_addrs->ti->total_blks - cs_addrs->ti->total_blks / bplmap * bplmap,
			  &dummy)
	    == NO_FREE_SPACE)
		bit_clear(blk_index / bplmap - 1, cs_addrs->bmm);
	else
		bit_set(blk_index / bplmap - 1, cs_addrs->bmm);

	free(bml_buffer);
	cs_addrs->nl->highest_lbm_blk_changed = cs_addrs->ti->total_blks;
}
