/****************************************************************
 *
 *	Copyright 2005, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "jnl_get_checksum.h"

/*
 * Input :
 *	buff   	 : Pointer to the input buffer whose checksum needs to be computed.
 *	bufflen  : Buffer size in bytes
 *
 * Returns:
 *	Computed checksum.
 *
 * Algorithm:
 *	It is different from the function "jnl_get_checksum" in the sense this calculates checksum of entire length passed.
 * 	If bufflen is not divisible by 4, it is ROUNDED DOWN to the nearest 4-byte (uint4) multiple.
 */
uint4 jnl_get_checksum_entire(uint4 *buff, int bufflen)
{
	uint4		*blk_top, blen, checksum;

	checksum = INIT_CHECKSUM_SEED;
	for (blen = bufflen / SIZEOF(*buff), blk_top = buff + blen ; buff < blk_top; buff++)
		checksum = ADJUST_CHECKSUM(checksum, *buff);
	assert(checksum);
	/* It is theoretically possible that the computed checksum turns out to be 0. In order to differentiate this
	 * with the fact that the checksum was never computed, we returns INIT_CHECKSUM_SEED in the former case.
	 */
	if (!checksum)
		checksum = INIT_CHECKSUM_SEED;
	return checksum;
}
