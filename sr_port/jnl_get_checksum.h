/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef __JNL_GET_CHECKSUM_H_
#define __JNL_GET_CHECKSUM_H_

#define INIT_CHECKSUM_SEED 0xFFFFFFFF

#define SLICE_BY	4
#define TABLE_SIZE	256
#define BYTEMASK	0xFF

GBLREF uint4 csum_table[SLICE_BY][TABLE_SIZE];

#ifdef  BIGENDIAN
#define LOBYTE	0
#define ADJUST_CHECKSUM(cursum, num4, newsum)										\
{															\
	uint4 tmpsum = csum_table[LOBYTE + 0][(cursum ^ num4) & BYTEMASK] ^ 						\
			csum_table[LOBYTE + 1][((cursum ^ num4) >> (1 * BITS_PER_UCHAR)) & BYTEMASK] ^			\
			csum_table[LOBYTE + 2][((cursum ^ num4) >> (2 * BITS_PER_UCHAR)) & BYTEMASK] ^ 			\
			csum_table[LOBYTE + 3][((cursum ^ num4) >> (3 * BITS_PER_UCHAR)) & BYTEMASK]; 			\
	newsum = tmpsum ? tmpsum : INIT_CHECKSUM_SEED;									\
}
#else
#define HIBYTE	3
#define ADJUST_CHECKSUM(cursum, num4, newsum)										\
{															\
	uint4 tmpsum = csum_table[HIBYTE - 0][(cursum ^ num4) & BYTEMASK] ^ 						\
			csum_table[HIBYTE - 1][((cursum ^ num4) >> (1 * BITS_PER_UCHAR)) & BYTEMASK] ^			\
			csum_table[HIBYTE - 2][((cursum ^ num4) >> (2 * BITS_PER_UCHAR)) & BYTEMASK] ^ 			\
			csum_table[HIBYTE - 3][((cursum ^ num4) >> (3 * BITS_PER_UCHAR)) & BYTEMASK]; 			\
	newsum = tmpsum ? tmpsum : INIT_CHECKSUM_SEED;									\
}
#endif

#define ADJUST_CHECKSUM_TN(cursum, tn, newsum)										\
{															\
	uint4 tmpsum_tn;												\
	ADJUST_CHECKSUM(cursum, *(uint4 *)tn, tmpsum_tn);								\
	ADJUST_CHECKSUM(tmpsum_tn, *(uint4 *)((char *)tn+SIZEOF(uint4)), newsum);					\
}

#define COMPUTE_COMMON_CHECKSUM(common_cksum, prefix)									\
{															\
	ADJUST_CHECKSUM_TN(INIT_CHECKSUM_SEED, &(prefix.tn), common_cksum);						\
	ADJUST_CHECKSUM(common_cksum, prefix.pini_addr, common_cksum);							\
	ADJUST_CHECKSUM(common_cksum, prefix.time, common_cksum);							\
}

#define COMPUTE_PBLK_CHECKSUM(blk_checksum, pblk_rec, common_cksum, jrec_checksum)					\
{															\
	ADJUST_CHECKSUM(blk_checksum, (pblk_rec)->prefix.jrec_type, jrec_checksum);					\
	ADJUST_CHECKSUM(jrec_checksum, (pblk_rec)->blknum, jrec_checksum);						\
	ADJUST_CHECKSUM(jrec_checksum, (pblk_rec)->bsiz, jrec_checksum);						\
	ADJUST_CHECKSUM(jrec_checksum, (pblk_rec)->ondsk_blkver, jrec_checksum);					\
	ADJUST_CHECKSUM(jrec_checksum, common_cksum, jrec_checksum);							\
}

#define COMPUTE_AIMG_CHECKSUM(blk_checksum, aimg_rec, common_cksum, jrec_checksum)					\
					COMPUTE_PBLK_CHECKSUM(blk_checksum, aimg_rec, common_cksum, jrec_checksum);

#define COMPUTE_LOGICAL_REC_CHECKSUM(jfb_checksum, jrec, common_cksum, jrec_checksum)					\
{															\
	ADJUST_CHECKSUM(jfb_checksum, (jrec)->prefix.jrec_type, jrec_checksum);						\
	ADJUST_CHECKSUM(jrec_checksum, (jrec)->update_num, jrec_checksum);						\
	ADJUST_CHECKSUM(jrec_checksum, (jrec)->token_seq.token, jrec_checksum);						\
	ADJUST_CHECKSUM(jrec_checksum, (jrec)->strm_seqno, jrec_checksum);						\
	ADJUST_CHECKSUM(jrec_checksum, (jrec)->num_participants, jrec_checksum);					\
	ADJUST_CHECKSUM(jrec_checksum, common_cksum, jrec_checksum);							\
}

/* This macro is to be used whenever we are computing the checksum of a block that has been acquired. */
#define	JNL_GET_CHECKSUM_ACQUIRED_BLK(cse, csd, csa, old_blk, bsize)							\
{															\
	cache_rec_ptr_t	cr;												\
	boolean_t	cr_is_null;											\
															\
	GBLREF	uint4	dollar_tlevel;											\
															\
	/* Record current database tn before computing checksum of acquired block. This is used				\
	 * later by the commit logic to determine if the block contents have changed (and hence				\
	 * if recomputation of checksum is necessary). For BG, we have two-phase commit where				\
	 * phase2 is done outside of crit. So it is possible that we note down the current database			\
	 * tn and then compute checksums outside of crit and then get crit and yet in the validation			\
	 * logic find the block header tn is LESSER than the noted dbtn (even though the block				\
	 * contents changed after the noted dbtn). This will cause us to falsely validate this block			\
	 * as not needing checksum recomputation. To ensure the checksum is recomputed inside crit,			\
	 * we note down a tn of 0 in case the block is locked for update (cr->in_tend is non-zero).			\
	 */														\
	assert((gds_t_acquired == cse->mode) || (gds_t_create == cse->mode)						\
		|| (gds_t_recycled2free == cse->mode));									\
	assert(cse->old_block == (sm_uc_ptr_t)(old_blk));								\
	assert((bsize) <= csd->blk_size);										\
	/* Since this macro is invoked only in case of before-image journaling and since MM does not			\
	 * support before-image journaling, we can safely assert that BG is the only access method.			\
	 */														\
	assert(dba_bg == csd->acc_meth);										\
	/* In rare cases cse->cr can be NULL even though this block is an acquired block. This is 			\
	 * possible if we are in TP and this block was part of the tree in the initial phase of the			\
	 * transaction but was marked free (by another process concurrently) in the later phase of			\
	 * the same TP transaction. But this case is a sureshot restart situation so be safe and 			\
	 * ensure recomputation happens inside of crit just in case we dont restart. Also add asserts			\
	 * (using donot_commit variable) to ensure we do restart this transaction.					\
	 */														\
	cr = cse->cr;													\
	cr_is_null = (NULL == cr);											\
	assert(!cr_is_null || dollar_tlevel);										\
	DEBUG_ONLY(if (cr_is_null) TREF(donot_commit) |= DONOTCOMMIT_JNLGETCHECKSUM_NULL_CR;)				\
	cse->tn = ((cr_is_null || cr->in_tend) ? 0 : csd->trans_hist.curr_tn);						\
	/* If cr is NULL, it is a restartable situation. So dont waste time computing checksums. Also			\
	 * if the db is encrypted, we cannot get at the encryption global buffer (jnl_get_checksum			\
	 * requires this) since we dont even have a regular global buffer corresponding to this block			\
	 * so there is no way jnl_get_checksum can proceed in that case. So it is actually necessary			\
	 * to avoid computing checksums if cr is NULL.									\
	 */														\
	cse->blk_checksum = !cr_is_null ? jnl_get_checksum((uint4 *)(old_blk), csa, (bsize)) : 0;			\
}
uint4 jnl_get_checksum(uint4 *buff, sgmnt_addrs *csa, int bufflen);
uint4 compute_checksum(uint4 init_sum, uint4 *buff, int bufflen);

#endif
