/****************************************************************
 *								*
 * Copyright (c) 2005-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "gdsblk.h"

#ifndef GDS_BLK_UPGRADE_INCLUDED
#define GDS_BLK_UPGRADE_INCLUDED

#ifdef DEBUG_DB_UPGRADE	/* define to get output for debugging */
#define DBGUPGRADE(X)
#else
#define DBGUPGRADE(X)
#endif
#define UPGRADE_IF_NEEDED	0	/* default */
#define UPGRADE_NEVER		1
#define UPGRADE_ALWAYS		2

int4 gds_blk_upgrade(sm_uc_ptr_t gds_blk_src, sm_uc_ptr_t gds_blk_trg, int4 bsiz, enum db_ver *ondsk_blkver);

GBLREF	uint4		ydb_blkupgrade_flag;	/* control whether dynamic upgrade is attempted or not */
GBLREF	boolean_t	dse_running;

static inline void blk_ptr_adjust(sm_uc_ptr_t buff, block_id offset)
{	/* buff contains a prior index block needing its offset adjusted */
	block_id	blk_num, *blk_ptr;
	sm_uc_ptr_t	blkEnd, recBase;
	unsigned short	temp_ushort;

	blkEnd = buff + ((blk_hdr_ptr_t)buff)->bsiz;
	assert(IS_64_BLK_ID(buff) == FALSE);
	DBG_VERIFY_ACCESS(blkEnd - 1);
	for (recBase = buff + SIZEOF(blk_hdr) ; recBase < blkEnd; recBase += ((rec_hdr_ptr_t)recBase)->rsiz)
	{	/* iterate through block updating pointers with the offset */
		if (blkEnd <= (recBase + SIZEOF(rec_hdr)))
		{
			assert(FALSE);
			buff = NULL;
		}
		GET_USHORT(temp_ushort, &(((rec_hdr_ptr_t)recBase)->rsiz));
		/* collation info in directory tree which was already converted, so following is safe */
		blk_ptr = (block_id *)(recBase + (int4)temp_ushort - SIZEOF_BLK_ID(FALSE));
		GET_BLK_ID_32(blk_num, blk_ptr);
		PUT_BLK_ID_32(blk_ptr, blk_num - offset);
	}
}

/* The following macro was gutted in V7+ as dsk_read uses the inline fuction above to adjst block_ids, but does not actually
 * upgrade block_id fileds from 4 to 8 bytes, as managing that as part of the read was deemed to complex and not necessary
 * The macro remains in mur_blocks_free.c as a marker for possible future conversions and the text below retained for guidance
 *
 * See if block needs to be converted to current version. Assume buffer is at least short aligned.
 * Note: csd->fully_upgraded is not derived within the macro but instead passed in as a parameter to ensure whichever
 * function (dsk_read currently) references this does that once and copies the value into a local variable that is used
 * in all further usages. This way multiple usages are guaranteed to see the same value. Using csd->fully_upgraded in
 * each of those cases could cause different values to be seen (since csd can be concurrently updated).
 */
#define GDS_BLK_UPGRADE_IF_NEEDED(blknum, srcbuffptr, trgbuffptr, curcsd, ondskblkver, upgrdstatus, fully_upgraded)		\
{																\
	if (NULL != (void *)(ondskblkver))											\
		*(ondskblkver) = ((blk_hdr_ptr_t)(srcbuffptr))->bver;								\
	upgrdstatus = SS_NORMAL;												\
}

#endif
