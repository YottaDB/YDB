/****************************************************************
 *
 *	Copyright 2005, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef __JNL_GET_CHECKSUM_H_
#define __JNL_GET_CHECKSUM_H_

#define INIT_CHECKSUM_SEED 1
#define CHKSUM_SEGLEN4 8

#define ADJUST_CHECKSUM(sum, num4)  (((sum) >> 4) + ((sum) << 4) + (num4))

/* This macro is to be used whenever we are computing the checksum of a block that has been acquired. */
#define	JNL_GET_CHECKSUM_ACQUIRED_BLK(cse, csd, old_blk, bsize)					\
{	/* Record current database tn before computing checksum of acquired block.		\
	 * This is used later by the commit logic to determine if the block contents		\
	 * have changed (and hence if recomputation of checksum is necessary).			\
	 */											\
	assert((gds_t_acquired == cse->mode) || (gds_t_create == cse->mode));			\
	assert(cse->old_block == (sm_uc_ptr_t)(old_blk));					\
	assert((bsize) <= csd->blk_size);							\
	cse->tn = csd->trans_hist.curr_tn;							\
	cse->blk_checksum = jnl_get_checksum((uint4 *)(old_blk), (bsize));			\
}

uint4 jnl_get_checksum(uint4 *buff, int bufflen);
uint4 jnl_get_checksum_entire(uint4 *buff, int bufflen);

#endif
