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

#include <stdio.h>
#include <unistd.h>


#include "gtmio.h"
#include "iosp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v3_gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "jnl.h"
#include "copy.h"
#include "error.h"
#include "mu_upgrd.h"
#include "mu_upgrd_adjust_blkptr.h"

/*-------------------------------------------------------------------------
    This is a tree traversal. This will start from the root and traverse
    it in-order upto the last level of index blocks. It will cut traversal
    before leaf node (actual data blocks) so that it does not need to go
    to a data block which is always level 0. This reduces I/O several times
    than traversing the entire B-Tree.  Directory tree also has level 0 but
    this contains index. dirtree flag helps to find, whether we are
    in directory tree or global variable tree.
    While traversing, this routine adjusts the block-ids.
    (Assume, BLKS_PER_LMAP=200H)
    Case blk >= 200H and blk <400H
    	blk = blk + (last_full_grp_startblk - 400H)
    Case blk >= 400H and blk < last_full_grp_startblk
    	blk = blk - 200H
    Otherwise
      	no change
  -------------------------------------------------------------------------*/
void mu_upgrd_adjust_blkptr(block_id blk, bool dirtree, sgmnt_data *new_head, int fd, char fn[], int fn_len)
{
	uchar_ptr_t     blk_base, blk_top, rec_base, rec_top, rec_chptr;
	unsigned char 	level;
	int 		status;
	short 		blk_len, rec_len;
	int4 		last_full_grp_startblk;
	block_id 	child;



	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBPREMATEOF);

	last_full_grp_startblk = ROUND_DOWN(new_head->trans_hist.total_blks, BLKS_PER_LMAP);
	blk_base = (uchar_ptr_t) malloc(new_head->blk_size);

	LSEEKREAD(fd, (off_t)(new_head->start_vbn - 1) * DISK_BLOCK_SIZE + (off_t)blk * new_head->blk_size,
		blk_base, new_head->blk_size, status);
	if (0 != status)
		if (-1 == status)
			rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
		else
			rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);

	level = ((blk_hdr_ptr_t) blk_base)->levl;
	blk_len = ((blk_hdr_ptr_t) blk_base)->bsiz;
	blk_top = blk_base + blk_len;
	for(rec_base = blk_base + sizeof(blk_hdr); rec_base<blk_top; rec_base = rec_top)
	{
		GET_SHORT(rec_len, &((rec_hdr *)rec_base)->rsiz);
		assert(rec_len >= 8);
		rec_top = rec_base + rec_len;
		if (dirtree && level == 0)
		{
			for (rec_chptr = rec_base + sizeof(rec_hdr); rec_chptr < rec_top; rec_chptr++)
			{
				if (0 == *rec_chptr && 0 == *(rec_chptr+1) )
				{
					break;
				}
			}
			if (rec_chptr == rec_top)
				GTMASSERT;
			rec_chptr+=2;

		}
		else
			rec_chptr = rec_top - 4;
		GET_LONG(child, rec_chptr);
		if (child >= BLKS_PER_LMAP && child < 2 * BLKS_PER_LMAP)
		{
			child += (last_full_grp_startblk - 2 * BLKS_PER_LMAP);
			PUT_LONG(rec_chptr, child);

		} else if (child >= 2*BLKS_PER_LMAP && child < last_full_grp_startblk)
		{
			child -= BLKS_PER_LMAP;
			PUT_LONG(rec_chptr, child);
		}

		if (level > 1 || (dirtree && level != 0))
			mu_upgrd_adjust_blkptr(child, dirtree, new_head, fd, fn, fn_len);
		else if (dirtree && level == 0)
			mu_upgrd_adjust_blkptr(child, (bool)(dirtree^1), new_head, fd, fn, fn_len);

	}

	LSEEKWRITE(fd, (off_t)(new_head->start_vbn - 1)* DISK_BLOCK_SIZE + (off_t)blk * new_head->blk_size,
		blk_base, new_head->blk_size, status);
	if (0 != status) rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);

	free(blk_base);
}

