/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <sys/shm.h>

#include "mdef.h"
#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmidef.h"	/* for cmmdef.h */
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"	/* for gtcmtr_protos.h */
#include "util.h"
#include "lke.h"
#include "lke_getcli.h"
#include "mlk_shrclean.h"
#include "mlk_shrsub_garbage_collect.h"
#include "interlock.h"
#include "mlk_garbage_collect.h"
#include "sleep.h"
#include "min_max.h"
#include "gtmmsg.h"
#include "do_shmat.h"

#include "mlk_ops.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mlk_shrhash_find_bucket.h"

GBLREF	gd_addr		*gd_header;
GBLREF	VSIG_ATOMIC_T	util_interrupt;
GBLREF	uint4		process_id;

error_def(ERR_MLKCLEANED);
error_def(ERR_MLKHASHTABERR);
error_def(ERR_NOREGION);
error_def(ERR_MLKHASHWRONG);
error_def(ERR_CTRLC);

void lke_clean(void)
{
	/* Arguments for lke_getcli */
	bool			locks, all = TRUE, wait = TRUE, interactive = FALSE, match = FALSE, memory = TRUE, nocrit = TRUE;
	boolean_t		exact = TRUE, integ = FALSE;
	int4			pid, repeat, bc_before, bc_after;
	gtm_int8		sleep_time;
	mstr			regname, node, one_lock;
	char			regbuf[MAX_RN_LEN], nodebuf[32], one_lockbuf[MAX_KEY_SZ];
	int			fi, mi;
	uint4			ti, num_buckets, bucket_offset, loop_cnt, loop_cnt2, total_len;
	mlk_shrhash_map_t	usedmap, usedmap2;
	hash128_state_t		hs;
	gtm_uint16		hashres;
	mlk_shrhash_ptr_t	shrhash, search_bucket, check_bucket, free_bucket;
	mlk_shrblk_ptr_t	shrblk, shr;
	mlk_shrsub_ptr_t	sub, sub2;
	gd_region		*reg;
	int			n;
	boolean_t		was_crit;
	mlk_pvtblk		pvtblk;
	DEBUG_ONLY(mlk_shrblk_ptr_t		blk);
	struct timespec		start_clock, end_clock;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	regname.addr = regbuf;
	regname.len = SIZEOF(regbuf);
	node.addr = nodebuf;
	node.len = SIZEOF(nodebuf);
	one_lock.addr = one_lockbuf;
	one_lock.len = SIZEOF(one_lockbuf);
	memset(&pvtblk, 0, SIZEOF(mlk_pvtblk));
	if (lke_getcli(&all, &wait, &interactive, &pid, &regname, &node, &one_lock, &memory, &nocrit, &exact,
				&repeat, &integ) == 0)
		return;
	do {
		clock_gettime(CLOCK_MONOTONIC, &start_clock);
		for (reg = gd_header->regions, n = 0; n != gd_header->n_regions; ++reg, ++n)
		{
			/* If region matches and is open */
			if (((0 == regname.len)
					|| ((reg->rname_len == regname.len) && !memcmp(reg->rname, regname.addr, regname.len)))
				&& reg->open)
			{
				match = TRUE;
				assert(IS_REG_BG_OR_MM(reg));
				if (IS_STATSDB_REGNAME(reg))
					continue;
				/* Construct a dummy pvtblk to pass in */
				MLK_PVTCTL_INIT(pvtblk.pvtctl, reg);
				prepare_for_gc(&pvtblk.pvtctl);
				assert(pvtblk.pvtctl.ctl->lock_gc_in_progress.u.parts.latch_pid == process_id);
				GRAB_LOCK_CRIT_AND_SYNC(pvtblk.pvtctl, was_crit);
				WBTEST_ONLY(WBTEST_MLOCK_HANG, SLEEP_USEC(10000ULL * MILLISECS_IN_SEC, 0););
				bc_before = pvtblk.pvtctl.ctl->blkcnt;
				mlk_garbage_collect(&pvtblk, 0, TRUE);
				assert(pvtblk.pvtctl.ctl->lock_gc_in_progress.u.parts.latch_pid == process_id);
				RELEASE_SWAPLOCK(&pvtblk.pvtctl.ctl->lock_gc_in_progress);
				bc_after = pvtblk.pvtctl.ctl->blkcnt;
				if (bc_after > bc_before)
					gtm_putmsg_csa(CSA_ARG(pvtblk.pvtctl.csa) VARLSTCNT(5) ERR_MLKCLEANED, 3,
							(bc_after - bc_before), REG_LEN_STR(reg));
				if (!integ)
				{
					REL_LOCK_CRIT(pvtblk.pvtctl, was_crit);
					continue;
				}
				shrhash = pvtblk.pvtctl.shrhash;
				num_buckets = pvtblk.pvtctl.shrhash_size;

				/* NOTE; this test will overwrite values currently locked, do NOT use in production
				 *  fills the hash table with junk and puts something outside of the neighborhood
				 *  to verify it is detected and reported correctly */
				WBTEST_ONLY(WBTEST_TRASH_HASH_NO_RECOVER,
						mlk_shrblk_ptr_t	newfreehead;

						HASH128_STATE_INIT(hs, 0);
						sub = malloc(SIZEOF(mlk_shrsub) + 23);
						sub->length = 24;
						memcpy(sub->data, "A12345670123456776543210", 24);
						blk = (mlk_shrblk_ptr_t)R2A(pvtblk.pvtctl.ctl->blkfree);
						pvtblk.pvtctl.ctl->blkcnt--;
						assert(0 != blk->rsib);
						newfreehead = (mlk_shrblk_ptr_t)R2A(blk->rsib);
						newfreehead->lsib = 0;
						A2R(pvtblk.pvtctl.ctl->blkfree, newfreehead);
						CHECK_SHRBLKPTR(pvtblk.pvtctl.ctl->blkfree, pvtblk.pvtctl);
						CHECK_SHRBLKPTR(newfreehead->rsib, pvtblk.pvtctl);
						memset(blk, 0, SIZEOF(mlk_shrblk));
						blk->owner = 42;
						blk->sequence = 1;
						A2R(blk->value, sub);
						A2R(sub->backpointer, blk);
						total_len = 0;
						mlk_shrhash_val_build(blk,
								&total_len, &hs);
						ydb_mmrhash_128_result(&hs, total_len, &hashres);
						ti = ((uint4)hashres.one) % num_buckets;
						for(int foo=0; foo < MLK_SHRHASH_NEIGHBORS + 2; foo++)
						{
							check_bucket = &shrhash[(ti + foo) % num_buckets];
							check_bucket->hash = (uint4)hashres.one;
							check_bucket->shrblk_idx = MLK_SHRBLK_IDX(pvtblk.pvtctl, blk);
						}
						check_bucket = &shrhash[ti];
						check_bucket->usedmap = ~(0);
				);
				/* NOTE; this test will overwrite values currently locked, do NOT use in production
				 *  puts space in the first 10 slots of the hash table starting at some position, then fills
				 *  outside of the neighborhood to verify that the cleanup will move the misplaced value
				 *  if it can */
				WBTEST_ONLY(WBTEST_TRASH_HASH_RECOVER,
						mlk_shrblk_ptr_t	newfreehead;

						HASH128_STATE_INIT(hs, 0);
						sub = malloc(SIZEOF(mlk_shrsub) + 32);
						sub->length = 33;
						memcpy(sub->data, "A12345678901234567890123456789012", 33);
						blk = (mlk_shrblk_ptr_t)R2A(pvtblk.pvtctl.ctl->blkfree);
						pvtblk.pvtctl.ctl->blkcnt--;
						assert(0 != blk->rsib);
						newfreehead = (mlk_shrblk_ptr_t)R2A(blk->rsib);
						newfreehead->lsib = 0;
						A2R(pvtblk.pvtctl.ctl->blkfree, newfreehead);
						CHECK_SHRBLKPTR(pvtblk.pvtctl.ctl->blkfree, pvtblk.pvtctl);
						CHECK_SHRBLKPTR(newfreehead->rsib, pvtblk.pvtctl);
						memset(blk, 0, SIZEOF(mlk_shrblk));
						blk->owner = 42;
						blk->sequence = 1;
						A2R(blk->lsib, blk);
						A2R(blk->rsib, blk);
						A2R(blk->value, sub);
						A2R(sub->backpointer, blk);
						total_len = 0;
						mlk_shrhash_val_build(blk,
								&total_len, &hs);
						ydb_mmrhash_128_result(&hs, total_len, &hashres);
						ti = ((uint4)hashres.one) % num_buckets;
						free_bucket = &shrhash[ti];
						free_bucket->usedmap = 0;
						for(int foo=0; foo < MLK_SHRHASH_NEIGHBORS + 2; foo++)
						{
							check_bucket = &shrhash[(ti + foo) % num_buckets];
							if (foo < 10)
							{
								check_bucket->hash = 0;
								check_bucket->shrblk_idx = 0;
								check_bucket->usedmap = 0;
							} else
							{
								check_bucket->hash = (uint4)hashres.one;
								check_bucket->shrblk_idx = MLK_SHRBLK_IDX(pvtblk.pvtctl, blk);
								/* When the shift reaches the number of bits in the underlying int,
								 * the following wraps on x86-family processors and goes to zero on
								 * Power processors. Rather than force the wrap behavior on Power,
								 * allow the natural behavior and adjust the expectations of the
								 * test.
								 */
								SET_NEIGHBOR(free_bucket->usedmap, foo);
							}
						}
				);
				/* Search through the hash table and verify that each entry marked by a bucket hashes to that
				 * bucket and that the entry in each bucket has been marked by the bucket it should be in
				 */
				for (fi = 0; fi < num_buckets; fi++)
				{
					search_bucket = &shrhash[fi];
					usedmap = search_bucket->usedmap;
					for (loop_cnt = 0; usedmap != 0 && loop_cnt < MLK_SHRHASH_NEIGHBORS;
							loop_cnt++, usedmap >>= 1)
					{
						if (1 == (usedmap & 1U))
						{
							check_bucket = &shrhash[(fi + loop_cnt) % num_buckets];
							if (0 == check_bucket->shrblk_idx)
							{
								gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4)
										MAKE_MSG_WARNING(ERR_MLKHASHWRONG),
										2, LEN_AND_LIT("Correcting."));
								/* This is low-risk for having concurrency issues if we get
								 * interrupted; it should be atomic */
								CLEAR_NEIGHBOR(search_bucket->usedmap, loop_cnt);
								continue;
							}
							shrblk = MLK_SHRHASH_SHRBLK(pvtblk.pvtctl, check_bucket);
							assert(0 != shrblk->value);
							HASH128_STATE_INIT(hs, 0);
							total_len = 0;
							mlk_shrhash_val_build(shrblk, &total_len, &hs);
							ydb_mmrhash_128_result(&hs, total_len, &hashres);
							/* If this triggers, it means a bucket was marked as used for a
							 *  particular hash value, but is in fact used by a different hash
							 *  value; we can not easily recover from this, and the safest
							 *  course of action is for the user to bring things down ASAP
							 */
							if ((((uint4)hashres.one % num_buckets) != fi) ||
									((uint4)hashres.one != check_bucket->hash))
							{
								REL_LOCK_CRIT(pvtblk.pvtctl, was_crit);
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MLKHASHWRONG,
										2, LEN_AND_LIT("Can't correct, exiting."));
							}
						}
					}
					if (0 != search_bucket->shrblk_idx)
					{	/* Verify that the value in this bucket should be in this bucket */
						HASH128_STATE_INIT(hs, 0);
						total_len = 0;
						mlk_shrhash_val_build(MLK_SHRHASH_SHRBLK(pvtblk.pvtctl, search_bucket),
									&total_len, &hs);
						ydb_mmrhash_128_result(&hs, total_len, &hashres);
						ti = ((uint4)hashres.one) % num_buckets;
						check_bucket = &shrhash[ti];
						bucket_offset = (num_buckets + fi - ti) % num_buckets;
						if (MLK_SHRHASH_NEIGHBORS <= bucket_offset)
						{	/* If this triggers, it means the hash was more than 32 away from correct
								  bucket; not in the right neighborhood -- try moving it */
							mi = mlk_shrhash_find_bucket(&pvtblk.pvtctl, (uint4)hashres.one);
							shr = MLK_SHRHASH_SHRBLK(pvtblk.pvtctl, search_bucket);
							if (mi == MLK_SHRHASH_FOUND_NO_BUCKET)
							{	/* If this triggers, it mean the hash table is full and things are
								 * out of position; very bad
								 * Emit a critical warning and carry on
								 */
								char name_buffer[MAX_ZWR_KEY_SZ + 1];
								MSTR_DEF(name, 0, name_buffer);

								lke_formatlockname(shr, &name);
								gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4)
										MAKE_MSG_WARNING(ERR_MLKHASHTABERR),
										2, RTS_ERROR_MSTR(&name));
								continue;
							}
							mlk_shrhash_insert(&pvtblk.pvtctl, ti, mi,
										MLK_SHRBLK_IDX(pvtblk.pvtctl, shr),
										(uint4)hashres.one);
							/* Clear old bucket */
							search_bucket->shrblk_idx = 0;
							search_bucket->hash = 0;
						} else
						{
							usedmap = check_bucket->usedmap;
							/* If this triggers, it means the bucket we are at has a value that was
							 * not marked as overflow in the correct bucket; this could be a result of
							 * the hashtable being full
							 */
							if (!IS_NEIGHBOR(usedmap, bucket_offset))
							{
								REL_LOCK_CRIT(pvtblk.pvtctl, was_crit);
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MLKHASHWRONG,
											2, LEN_AND_LIT("Can't correct, exiting."));
							}
						}
					}
				}
				WBTEST_ONLY(WBTEST_TRASH_HASH_RECOVER,
						lke_show_hashtable(&pvtblk.pvtctl);
				);
				REL_LOCK_CRIT(pvtblk.pvtctl, was_crit);
			}
		}
		if (!match && (0 != regname.len))
			rts_error_csa(NULL, VARLSTCNT(4) ERR_NOREGION, 2, regname.len, regname.addr);
		if (!repeat)
			break;
		clock_gettime(CLOCK_MONOTONIC, &end_clock);
		/* seconds to milliseconds, then nanoseconds to milliseconds */
		sleep_time = (repeat * MICROSECS_IN_SEC);
		if (end_clock.tv_nsec >= start_clock.tv_nsec)
		{
			sleep_time -= (end_clock.tv_sec - start_clock.tv_sec) * MICROSECS_IN_SEC;
			sleep_time -= (end_clock.tv_nsec - start_clock.tv_nsec) / NANOSECS_IN_USEC;
		} else
		{
			sleep_time -= (end_clock.tv_sec - 1 - start_clock.tv_sec) * MICROSECS_IN_SEC;
			sleep_time -= ((uint8)NANOSECS_IN_SEC + end_clock.tv_nsec - start_clock.tv_nsec) / NANOSECS_IN_USEC;
		}
		if (sleep_time > 0)
			SLEEP_USEC(sleep_time, 0);
		if (util_interrupt)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
	} while (TRUE);
}
