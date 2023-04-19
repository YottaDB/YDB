/****************************************************************
 *								*
 * Copyright (c) 2023 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDS_BLK_UPGRADE_INLINE_INCLUDED
#define GDS_BLK_UPGRADE_INLINE_INCLUDED

static inline boolean_t blk_ptr_adjust(sm_uc_ptr_t buff, block_id offset)
{	/* buff contains a prior index block needing its offset adjusted */
	blk_hdr_ptr_t	blkhdr;
	block_id	blk_num, *blk_ptr;
	sm_uc_ptr_t	blkEnd, recBase;
	unsigned short	temp_ushort;
	DEBUG_ONLY(DCL_THREADGBL_ACCESS);

	DEBUG_ONLY(SETUP_THREADGBL_ACCESS);
	blkhdr = (blk_hdr_ptr_t)buff;
	blkEnd = buff + blkhdr->bsiz;
	DBG_VERIFY_ACCESS(blkEnd - 1);
	for (recBase = buff + SIZEOF(blk_hdr) ; recBase < blkEnd; recBase += ((rec_hdr_ptr_t)recBase)->rsiz)
	{	/* iterate through block updating pointers with the offset */
		if ((blkEnd <= (recBase + SIZEOF(rec_hdr))) || (0 == blkhdr->levl) || (IS_64_BLK_ID(buff)))
		{	/* Could be DB corruption, recycled block or concurrency issue, better handled elsewhere */
			DEBUG_ONLY(TREF(donot_commit) = DONOTCOMMIT_BLK_PTR_UPGRADE_INCORRECT);
			return FALSE;
		}
		GET_USHORT(temp_ushort, &(((rec_hdr_ptr_t)recBase)->rsiz));
		/* collation info in directory tree which was already converted, so following is safe */
		blk_ptr = (block_id *)(recBase + (int4)temp_ushort - SIZEOF_BLK_ID(FALSE));
		GET_BLK_ID_32(blk_num, blk_ptr);
		if (offset > blk_num) /* Block num < offset should have been handled in MUPIP UPGRADE */
		{	/* Could be DB corruption, recycled block or concurrency issue, better handled elsewhere */
			DEBUG_ONLY(TREF(donot_commit) = DONOTCOMMIT_BLK_PTR_UPGRADE_INCORRECT);
			return FALSE;
		}
		PUT_BLK_ID_32(blk_ptr, blk_num - offset);
	}
	return TRUE;
}
#endif
