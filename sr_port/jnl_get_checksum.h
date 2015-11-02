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

#define INIT_CHECKSUM_SEED 1
#define CHKSUM_SEGLEN4 8

#define ADJUST_CHECKSUM(sum, num4)  (((sum) >> 4) + ((sum) << 4) + (num4))

#ifdef UNIX
/* Include the sequence number field of the journal record as part of the checksum computation. ADJUST_CHECKSUM
 * macro currently relies on 4 byte quanitites as inputs. But, the journal sequence number is an 8 byte quantity.
 * To avoid multiple calls to ADJUST_CHECKSUM to include the complete 8 byte sequence number, each of which might
 * take around 4-5 instructions, consider only the lower order order 4 bytes of the sequence number for the checksum
 * compuation. Given that the lower order bytes are the ones that will keep changing for every transaction, this
 * should suffice.
 */
#define ADJUST_CHECKSUM_WITH_SEQNO(IS_REPLICATED, CHECKSUM, SEQNO)						\
{														\
	seq_num				rec_token_seq;								\
														\
	if (IS_REPLICATED)											\
	{													\
		rec_token_seq = SEQNO;										\
		CHECKSUM = ADJUST_CHECKSUM(CHECKSUM, (rec_token_seq & 0x0000FFFF));				\
	}													\
}
#else
#define ADJUST_CHECKSUM_WITH_SEQNO(IS_REPLICATED, CHECKSUM, SEQNO)
#endif

/* This macro is to be used whenever we are computing the checksum of a block that has been acquired. */
#define	JNL_GET_CHECKSUM_ACQUIRED_BLK(cse, csd, csa, old_blk, bsize)					\
{													\
	cache_rec_ptr_t	cr;										\
	boolean_t	cr_is_null;									\
													\
	GBLREF	uint4	dollar_tlevel;									\
													\
	/* Record current database tn before computing checksum of acquired block. This is used		\
	 * later by the commit logic to determine if the block contents have changed (and hence		\
	 * if recomputation of checksum is necessary). For BG, we have two-phase commit where		\
	 * phase2 is done outside of crit. So it is possible that we note down the current database	\
	 * tn and then compute checksums outside of crit and then get crit and yet in the validation	\
	 * logic find the block header tn is LESSER than the noted dbtn (even though the block		\
	 * contents changed after the noted dbtn). This will cause us to falsely validate this block	\
	 * as not needing checksum recomputation. To ensure the checksum is recomputed inside crit,	\
	 * we note down a tn of 0 in case the block is locked for update (cr->in_tend is non-zero).	\
	 */												\
	assert((gds_t_acquired == cse->mode) || (gds_t_create == cse->mode)				\
		|| (gds_t_recycled2free == cse->mode));							\
	assert(cse->old_block == (sm_uc_ptr_t)(old_blk));						\
	assert((bsize) <= csd->blk_size);								\
	/* Since this macro is invoked only in case of before-image journaling and since MM does not	\
	 * support before-image journaling, we can safely assert that BG is the only access method.	\
	 */												\
	assert(dba_bg == csd->acc_meth);								\
	/* In rare cases cse->cr can be NULL even though this block is an acquired block. This is 	\
	 * possible if we are in TP and this block was part of the tree in the initial phase of the	\
	 * transaction but was marked free (by another process concurrently) in the later phase of	\
	 * the same TP transaction. But this case is a sureshot restart situation so be safe and 	\
	 * ensure recomputation happens inside of crit just in case we dont restart. Also add asserts	\
	 * (using donot_commit variable) to ensure we do restart this transaction.			\
	 */												\
	cr = cse->cr;											\
	cr_is_null = (NULL == cr);									\
	assert(!cr_is_null || dollar_tlevel);								\
	DEBUG_ONLY(if (cr_is_null) TREF(donot_commit) |= DONOTCOMMIT_JNLGETCHECKSUM_NULL_CR;)		\
	cse->tn = ((cr_is_null || cr->in_tend) ? 0 : csd->trans_hist.curr_tn);				\
	/* If cr is NULL, it is a restartable situation. So dont waste time computing checksums. Also	\
	 * if the db is encrypted, we cannot get at the encryption global buffer (jnl_get_checksum	\
	 * requires this) since we dont even have a regular global buffer corresponding to this block	\
	 * so there is no way jnl_get_checksum can proceed in that case. So it is actually necessary	\
	 * to avoid computing checksums if cr is NULL.							\
	 */												\
	cse->blk_checksum = !cr_is_null ? jnl_get_checksum((uint4 *)(old_blk), csa, (bsize)) : 0;	\
}

uint4 jnl_get_checksum(uint4 *buff, sgmnt_addrs *csa, int bufflen);
uint4 jnl_get_checksum_entire(uint4 *buff, int bufflen);

#endif
