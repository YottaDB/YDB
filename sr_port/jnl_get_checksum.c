/****************************************************************
 *
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "jnl_get_checksum.h"

/*
 * Input :
 *	checksum : Current checksum value.
 *	buff   	 : Pointer to the input buffer whose checksum needs to be computed and adjusted with the input checksum.
 *	bufflen  : Buffer size in bytes
 *
 * Returns:
 *	Adjusted checksum.
 *
 * Algorithm:
 * 	For every 512 (DISK_BLOCK_SIZE) byte block of the input buffer, the first 16 and last 16 bytes are considered for checksum.
 *	The way this is implemented is as follows.
 *	For the 0th 512-byte block, the first 16 bytes are considered.
 *	For the 1st 512-byte block, the last 16 bytes of the 0th block and the first 16 bytes of the 1st block are considered.
 *		This is taken as a contiguous sequence of 32-bytes (i.e. CHKSUM_SEGLEN4 uint4s)
 *	For the 2nd 512-byte block, the last 16 bytes of the 1st block and the first 16 bytes of the 2nd block are considered.
 *		This is taken as a contiguous sequence of 32-bytes (i.e. CHKSUM_SEGLEN4 uint4s)
 *	And so on until the input buffer length is exceeded.
 * 	If bufflen is not divisible by 4, it is ROUNDED DOWN to the nearest 4-byte (uint4) multiple.
 */
uint4 jnl_get_checksum(uint4 checksum, uint4 *buff, int bufflen)
{
	uint4 	*top, *blk_base, *blk_top, blen;

	assert(checksum);
	for (blen = bufflen / sizeof(*buff), top = buff + blen, blk_top = buff + CHKSUM_SEGLEN4 / 2; buff < top ;)
	{
		if (blk_top > top)
			blk_top = top;
		for ( ; buff < blk_top; buff++)
			checksum = ADJUST_CHECKSUM(checksum, *buff);
		blk_top = (uint4 *)((sm_uc_ptr_t)buff + DISK_BLOCK_SIZE);
		buff = blk_top - CHKSUM_SEGLEN4;
	}
	assert(checksum);
	/* It is theoretically possible that the computed checksum turns out to be 0. In order to differentiate this
	 * with the fact that the checksum was never computed, we returns INIT_CHECKSUM_SEED in the former case.
	 */
	if (!checksum)
		checksum = INIT_CHECKSUM_SEED;
	return checksum;
}
