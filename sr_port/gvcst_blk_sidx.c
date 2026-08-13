/****************************************************************
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "copy.h"
#include "min_max.h"
#include "memcoherency.h"
#include "lockconst.h"
#include "compswap.h"
#include "interlock.h"		/* for GET_SWAPLOCK and RELEASE_SWAPLOCK */
#include "is_proc_alive.h"
#include "send_msg.h"
#include "gvcst_blk_sidx.h"

GBLREF	uint4		process_id;
GBLREF	sgmnt_addrs	*cs_addrs;

DEBUG_ONLY(error_def(ERR_TEXT);)

/* See gvcst_blk_sidx.h for the layout of a search index and for why it is held in shared memory rather
 * than in the block.
 */

/* How many records this block holds. A hop down the record headers, without touching keys, so it is
 * far cheaper than the walk in the build below and lets that walk pick a stride that spreads the
 * samples over the WHOLE block. Sampling until the space runs out instead would index the front of a
 * block and leave the back of it unhelped, which is the half the linear walk is slowest to reach and
 * so the half an index is worth the most on.
 */
static int	sidx_count_recs(sm_uc_ptr_t buff, int bsiz)
{
	sm_uc_ptr_t	rec, top;
	unsigned short	rsiz;
	int		nrec;

	top = buff + bsiz;
	nrec = 0;
	for (rec = buff + SIZEOF(blk_hdr); rec < top; rec += rsiz)
	{
		GET_USHORT(rsiz, &((rec_hdr_ptr_t)rec)->rsiz);
		if ((0 == rsiz) || ((rec + rsiz) > top))
			return 0;			/* damaged : no search index for this block */
		nrec++;
	}
	return nrec;
}

/* The build itself, run with this slot claimed. Split out from "gvcst_blk_sidx_build" for one reason:
 * so that the claim is taken and released around a single call, and every path out of the build is
 * therefore a released claim. This walks a block that can change under it and gives up in a dozen
 * places - a damaged record, a key too long to sample, too few records to be worth indexing, the block
 * moving underneath - and if the claim were taken and dropped inside here instead, each of those
 * returns would need its own release.
 *
 * Leaking one would WEDGE THE SLOT, which is worse than losing an index:
 *
 *	- no other process could build that slot again. "COMPSWAP_LOCK" fails against a live pid, and
 *	  the salvage path in the caller only takes the latch away from a holder that is DEAD.
 *	- the slot would be stuck in its UNUSABLE state, because the first thing this function does is
 *	  set hdr->tn to BLK_SIDX_TN_NONE. So not a stale index that a later build could replace: no
 *	  index at all, ever.
 *	- it would last for the life of the shared memory segment, nothing else resetting that latch.
 *
 * Every index block whose block number picks that slot would then search linearly forever, silently:
 * right answers, no error, the feature quietly dead for those blocks. A process DYING mid-build leaks
 * the claim the same way, which the caller handles separately by salvaging from a dead holder.
 */
static void	sidx_build_locked(sm_uc_ptr_t sidx, int sidx_size, sm_uc_ptr_t buff, int blk_size, block_id blk,
					cache_rec_ptr_t cr)
{
	blk_sidx_hdr	*hdr;
	trans_num	end_tn, lcl_curr_tn;
	/* The zero looks like a dead store and is not. "start_cycle" is set and used under the same test
	 * on "cr", but gcc does not correlate the two and reports -Wmaybe-uninitialized at the use site.
	 * That is an error here : CMakeLists.txt compiles with -Wall -Werror, and the
	 * -Wno-maybe-uninitialized it carries is in a branch that applies only to gcc 4.8.5 and older.
	 * Checked with gcc 15.2, which fails the build without it; clang proves the correlation and is
	 * quiet either way.
	 */
	int4		start_cycle = 0;
	uint4		end_bsiz;
	unsigned char	key[MAX_KEY_SZ + 1];
	sm_uc_ptr_t	rec, top, out, out_top;
	unsigned short	*ent_off;
	int		bsiz, nrec, stride, key_len, cmpc, sfx, nent, max_ent, ent_len, next_at, n;
	unsigned short	rsiz;
	boolean_t	long_blk_id;
	trans_num	tn;

	hdr = (blk_sidx_hdr *)sidx;
	hdr->tn = BLK_SIDX_TN_NONE;		/* nothing here is usable until the stamp is published below */
	/* A NULL cr is taken below to mean MM, and everything on that path depends on it. It is true only
	 * because "gvcst_blk_search" gates this call on (CYCLE_PVT_COPY != pStat->cycle): a privately built
	 * block also comes back from "t_qread" with no cache record, under BG as much as MM. Were that gate
	 * ever relaxed, such an image would take the MM path here and be PUBLISHED, and its tn names the
	 * committed block rather than the private content, so the stamp would look valid to every reader.
	 * Assert the assumption so the gate cannot quietly stop holding.
	 *
	 * The converse is asserted too. A non-NULL cr means BG, since MM has no cache records at all, and
	 * the code below sends every non-NULL cr down the BG path - the in_tend, cycle and blk tests, none
	 * of which mean anything on an MM segment. Together the two asserts state that "cr" alone decides
	 * the access method here, which is what lets the rest of this function test "NULL == cr" rather
	 * than reaching for csd->acc_meth each time.
	 */
	assert((NULL != cr) || (dba_mm == cs_addrs->hdr->acc_meth));
	assert((NULL == cr) || (dba_bg == cs_addrs->hdr->acc_meth));
	bsiz = (int)((blk_hdr_ptr_t)buff)->bsiz;
	if ((bsiz <= (int)SIZEOF(blk_hdr)) || (bsiz > blk_size))
		return;
	tn = ((blk_hdr_ptr_t)buff)->tn;
	if (NULL != cr)
	{
		SHM_READ_MEMORY_BARRIER;	/* the in_tend read must not float above the header read above */
		/* BG's bracket, and the FIRST of two in_tend reads. cr->in_tend is set in phase 1 and
		 * cleared after the phase 2 copy behind SHM_WRITE_MEMORY_BARRIER, so finding it clear here
		 * means the whole copy is visible.
		 *
		 * This read must stay BELOW the header read above and must never be moved ahead of it. What it
		 * is here to catch is one specific copy : one that has ALREADY WRITTEN the header just read
		 * and has NOT YET finished the block. That copy is the only one the tn comparison cannot see,
		 * memcpy copying forward so that tn and end_tn are both the new number over a spliced body,
		 * and it necessarily has in_tend non-zero while it runs. Moved above the header read, this
		 * would run before such a copy had started, read clear, and let it through - publishing an
		 * index stamped with the block's real current tn, which no later reader has grounds to doubt.
		 * gvcst_blk_sidx.h traces that step by step, and sets out how this read, the one after the
		 * walk and the tn comparison divide the three ways a phase 2 copy can overlap the walk.
		 */
		if (cr->in_tend)
			return;			/* a phase 2 copy is in flight : nothing here is stable */
		start_cycle = cr->cycle;	/* to catch this buffer being recycled during the walk */
	} else
	{
		SHM_READ_MEMORY_BARRIER;	/* the two reads below must not float above the header read */
		/* MM's bracket, and the only quiescence sample there is. It sits BELOW the header read for
		 * the same reason BG's first in_tend read does, and the case is the same one: a commit that
		 * has ALREADY WRITTEN the header just read and has NOT YET finished the block. On MM the
		 * block is written in place, and "gvcst_blk_build" writes the header - tn included - BEFORE
		 * the records, behind a write barrier. So such a commit leaves the new tn visible at both
		 * ends of the walk over a body still being rewritten, and no comparison of the block's own
		 * tn can see it. Asking whether a commit is in flight AT THIS INSTANT does see it, because
		 * that commit has raised early_tn and not yet let curr_tn catch up.
		 *
		 * Moved above the header read this would be worthless : a commit starting in the gap would
		 * read as quiescent here, write the header, and be mid-body throughout the walk.
		 *
		 * A commit raises early_tn at CMT02, writes its blocks, and lets curr_tn catch up at CMT13,
		 * so the two are equal exactly when no commit is between those points. Reading curr_tn FIRST,
		 * with a barrier before early_tn, is what keeps the pair honest : with the loads reordered, a
		 * commit in flight at the first and finished by the second reads back as quiescent, which is
		 * the one answer that must never be wrong here. Taking curr_tn first also errs towards
		 * refusing a good build rather than accepting a torn one.
		 */
		lcl_curr_tn = cs_addrs->ti->curr_tn;
		SHM_READ_MEMORY_BARRIER;	/* order the two loads against each other */
		if (lcl_curr_tn != cs_addrs->ti->early_tn)
			return;			/* a commit is in flight : do not even walk the block */
		SHM_READ_MEMORY_BARRIER;	/* and the walk must not float above the two reads */
	}
	nrec = sidx_count_recs(buff, bsiz);
	if (BLK_SIDX_MIN_ENT > nrec)
		return;				/* too few records for a binary search to earn its keep */
	/* The offset array and the entries share one slot, and the array has to be sized before a single
	 * entry is written, so how many samples fit is worked out from an estimate of what one costs.
	 * The divisor is that estimate, term by term:
	 *
	 *	SIZEOF(unsigned short)	its element of the offset array
	 *	BLK_SIDX_ENT_HDR_SZ	the rec_off and key_len that head the entry itself
	 *	bsiz / nrec		the average RECORD size, standing in for the key length. It
	 *				overstates the key deliberately, a record carrying that 4 byte
	 *				header and an 8 byte block pointer besides its share of a key
	 *	+ 1			a byte of slack, which also covers the truncation in the division
	 *				above it : that quotient is a floor, so records averaging say 15.9
	 *				bytes would otherwise be costed at 15
	 *
	 * Every term errs high on purpose. Overshooting costs samples and never safety, since the walk
	 * below stops as soon as the space runs out and keeps whatever fitted, whereas undershooting
	 * would size the offset array for more entries than the slot can hold.
	 */
	max_ent = (sidx_size - (int)SIZEOF(blk_sidx_hdr))
			/ ((int)SIZEOF(unsigned short) + BLK_SIDX_ENT_HDR_SZ + (bsiz / nrec) + 1);
	if (BLK_SIDX_MIN_ENT > max_ent)
		return;				/* keys too long to index this block usefully */
	if (max_ent > nrec)
		max_ent = nrec;
	/* Sample every "stride" records, so the samples are spread evenly across the block rather
	 * than crowded at its front.
	 */
	stride = DIVIDE_ROUND_UP(nrec, max_ent);	/* at least 1 : "max_ent" was clamped to "nrec" just above */
	ent_off = (unsigned short *)(sidx + SIZEOF(blk_sidx_hdr));
	out = sidx + SIZEOF(blk_sidx_hdr) + (max_ent * SIZEOF(unsigned short));
	out_top = sidx + sidx_size;
	top = buff + bsiz;
	long_blk_id = IS_64_BLK_ID(buff);
	key_len = nent = 0;
	next_at = n = 0;
	/* Every record has to be walked even though only some are sampled: a key is stored compressed
	 * against its predecessor, so the key at a sampled record is only knowable by rebuilding every key
	 * ahead of it.
	 */
	for (rec = buff + SIZEOF(blk_hdr); rec < top; rec += rsiz, n++)
	{
		GET_USHORT(rsiz, &((rec_hdr_ptr_t)rec)->rsiz);
		if ((0 == rsiz) || ((rec + rsiz) > top))
			return;			/* block changed under us : leave the search index unstamped */
		EVAL_CMPC2((rec_hdr_ptr_t)rec, cmpc);
		if (cmpc > key_len)
			return;			/* a key cannot compress against more than the one ahead of it */
		sfx = (int)rsiz - (int)SIZEOF(rec_hdr) - SIZEOF_BLK_ID(long_blk_id);
		if ((0 > sfx) || ((cmpc + sfx) > MAX_KEY_SZ))
			return;			/* this bounds the memcpy below, and with it every later "cmpc" : it
						 * is the only assignment to "key_len", which starts at 0, so "key_len"
						 * never exceeds MAX_KEY_SZ and the test above carries that bound to
						 * "cmpc". No separate MAX_KEY_SZ test on "cmpc" is needed.
						 */
		if (sfx)
			memcpy(key + cmpc, rec + SIZEOF(rec_hdr), sfx);
		key_len = cmpc + sfx;
		if ((n < next_at) || (0 == key_len) || (nent >= max_ent))
			continue;		/* not a sample point, the star key, or the table is full */
		ent_len = BLK_SIDX_ENT_HDR_SZ + key_len;
		if ((out + ent_len) > out_top)
			break;			/* space ran out sooner than the estimate : keep what we have */
		PUT_USHORT(out, (unsigned short)(rec - buff));
		PUT_USHORT(out + 2, (unsigned short)key_len);
		memcpy(out + BLK_SIDX_ENT_HDR_SZ, key, key_len);
		ent_off[nent] = (unsigned short)(out - sidx);
		out += ent_len;
		nent++;
		next_at = n + stride;
	}
	if (BLK_SIDX_MIN_ENT > nent)
		return;
	/* The block may have changed while it was being walked - a reader holds nothing that would stop
	 * it. Reading part of one image and part of another produces entries spliced from two blocks, and
	 * the stamp cannot show it, because the stamp is honest about an image that was never whole.
	 * Observed as one slot holding a key for ^x at the first record and keys for ^n after it, which
	 * no sorted block can contain.
	 *
	 * Take the header ONCE here and judge on that snapshot. Re-reading it further down would be
	 * asking about a moving target: nothing stops another process writing this block the instant
	 * after a test passes, and that is harmless, because its tn then moves and the stamp published
	 * below stops matching, so no reader uses this slot. What matters is only that nothing wrote the
	 * block WHILE it was being walked.
	 */
	SHM_READ_MEMORY_BARRIER;	/* finish reading the block before judging whether it moved */
	end_tn = ((blk_hdr_ptr_t)buff)->tn;
	end_bsiz = (uint4)((blk_hdr_ptr_t)buff)->bsiz;
	if (NULL != cr)
	{	/* BG. The SECOND in_tend read: this one catches a copy that started during the walk, where
		 * the one before the walk catches a copy already running. A copy that begins and ends between
		 * the two is caught by the tn comparison further down instead.
		 *
		 * The cycle and the block number cover what in_tend cannot, "db_csh_getn" recycling this cache
		 * record to a DIFFERENT block mid-walk. cr->data_invalid needs no test of its own, being set
		 * and reset strictly inside in_tend, and no curr_tn comparison belongs on this path at all.
		 * See gvcst_blk_sidx.h for each of those.
		 */
		if (cr->in_tend || (start_cycle != cr->cycle) || (cr->blk != blk))
			return;
	}
	/* MM has NO counterpart to that second in_tend read, which looks like an omission and is not.
	 * BG needs two because its own block tn cannot be trusted mid-copy: phase 2 memcpies the block
	 * forward, so tn and end_tn both read the new number over a spliced body, and only in_tend sees
	 * it. MM is the opposite. "gvcst_blk_build" lays the tn down BEFORE the records and behind a
	 * write barrier, so any walk that read a byte of a concurrent write also sees the new tn at the
	 * "tn != end_tn" test below. The single quiescence sample, taken below the header read, covers
	 * the one case that test cannot see - a commit that had already written this block's header
	 * before the build read it, and was still writing the body. Nothing is left for a second sample,
	 * and one there would refuse a build whenever any commit anywhere happened to be in flight at
	 * that instant, which at 16 jobs is a large share of the builds this change exists to keep.
	 */
	/* The block's tn as it was BEFORE the walk, against the same header snapshot, is an INDEPENDENT
	 * test and not a consequence of the ones above. On BG a phase 2 copy that begins AND ends inside
	 * the walk leaves in_tend clear at both of the reads that bracket it, with the cycle and the
	 * block number unchanged throughout, so nothing above notices, and the buffer has still been
	 * rewritten underneath the walk. Only comparing the tn at the two ends sees it, and the first BG
	 * run showed as much : this test was briefly an assert, and that assert failed there.
	 */
	if (tn != end_tn)
		return;			/* the block was rewritten while we walked it */
	/* Size is a SEPARATE test, and unlike every other test here NO case is known that reaches it: MM
	 * is covered by early_tn and the barrier behind it, and on BG the write barrier before in_tend is
	 * cleared means a clear in_tend implies the whole copy is visible, header included.
	 *
	 * It is kept anyway, and the reason is this project's own record rather than an argument. Twice
	 * one of these last two tests was judged to be implied by the ones above it and demoted to an
	 * assert, and twice testing proved that judgement wrong : the tn test just above, and then this
	 * size test, which failed repeatedly on MM. Both times the reasoning read exactly
	 * as sound as the paragraph above. The test costs one comparison of a value already loaded, and
	 * it can only ever refuse a build, never accept a bad one, so it stays as the cheapest possible
	 * insurance against that reasoning being wrong a third time.
	 */
	if (bsiz != (int)end_bsiz)
		return;			/* the block changed size while we walked it */
	hdr->blk = blk;
	hdr->nent = nent;
	hdr->bsiz = bsiz;
	SHM_WRITE_MEMORY_BARRIER;	/* publish the contents before the stamp that vouches for them */
	hdr->tn = tn;
}

/* Build a search index for this block image, but only if no other process is already writing this slot.
 *
 * The claim has to be atomic. Otherwise two processes writing one slot interleave their entries, and
 * whichever finishes last stamps the mixture as valid - an entry then pairs one record's key with
 * another record's offset, which is precisely the thing gvcst_blk_sidx.h says must never happen. An
 * earlier version of this used a plain store here, reasoning that the claim gates only whether an
 * index is BUILT and never whether one is USED, so losing the race could cost nothing worse than a
 * second build. That reasoning was wrong: the two builds are not independent, they are writing the
 * same bytes. It reproduced as INTEG "keys out of order".
 *
 * Failing to claim is not an error and is not retried. A search index is a cache, so the cost of not
 * building one is a linear search - the very thing this database did before the search index feature
 * existed.
 */
void	gvcst_blk_sidx_build(sm_uc_ptr_t sidx, int sidx_size, sm_uc_ptr_t buff, int blk_size, block_id blk,
				cache_rec_ptr_t cr)
{
	blk_sidx_hdr	*hdr;
	int4		holder;

	hdr = (blk_sidx_hdr *)sidx;
	if (!GET_SWAPLOCK(&hdr->build_latch))
	{	/* Somebody else has it. Almost always they are mid-build and this build is redundant, so
		 * leave them to it. But a process that dies holding the claim would otherwise wedge this slot
		 * for the life of the shared segment, and wedge it in its UNUSABLE state, since a build clears
		 * the stamp before it writes anything. So take the claim away from a holder that is gone.
		 *
		 * Salvaging is safe even though the dead holder may have left the slot half written, because
		 * nothing here trusts what a slot contains. Its entries are meaningless until a stamp vouches
		 * for them, the dead holder cleared that stamp on its way in and never published a new one, and
		 * this build overwrites the entries from the beginning before publishing its own. A reader
		 * arriving meanwhile sees BLK_SIDX_TN_NONE and searches the block the ordinary way.
		 *
		 * The three tests below are ordered so that each one only runs when the previous leaves the
		 * question open, and the last is what makes the salvage race-free:
		 *
		 *	LOCK_AVAILABLE == holder	the holder released between the failed claim above and this
		 *					read, so there is nothing to salvage; let the next search
		 *					build it rather than race for it here, and skips the
		 *					"is_proc_alive" syscall in a case that cannot be a dead holder.
		 *	is_proc_alive(holder, 0)	a live holder is mid-build, which is the common case; leave
		 *					it alone.
		 *	COMPSWAP_LOCK(..., holder, ...)	take the latch only if it still holds exactly that dead
		 *					pid. Two processes can reach this point having read the same
		 *					dead holder, and this makes one of them win: the loser's
		 *					compare fails because the pid is no longer what it read, and
		 *					it returns rather than joining the winner in the slot.
		 *
		 * That last one cannot be dropped in favour of a plain store on the strength of the two above
		 * it. The failed claim at the top of this function establishes only that the latch was not
		 * AVAILABLE at that instant - it conveys no ownership - and "is_proc_alive" reports on a pid
		 * read earlier, which grants nothing either. Only an atomic take of exactly the pid observed
		 * separates two would-be salvagers, and two of them building into one slot is the corruption
		 * described above. It is also why this one is a bare COMPSWAP_LOCK rather than GET_SWAPLOCK:
		 * the latch is taken from the dead pid, not from LOCK_AVAILABLE.
		 */
		holder = hdr->build_latch.u.parts.latch_pid;
		if ((LOCK_AVAILABLE == holder) || is_proc_alive(holder, 0)
				|| !COMPSWAP_LOCK(&hdr->build_latch, holder, process_id))
			return;
	}
	sidx_build_locked(sidx, sidx_size, buff, blk_size, blk, cr);
	RELEASE_SWAPLOCK(&hdr->build_latch);
}

/* Find the sampled record that sorts last among those strictly before the target, and copy it into
 * "samp" for the caller to resume from. FALSE means the caller gets no resume point and walks the
 * block from its first record.
 *
 * CALLER CONTRACT : the caller must have just passed BLK_SIDX_DESCRIBES on this same slot and block
 * image. That is not merely a convenience to save repeating the test - that read of the stamp is the
 * acquire half of the builder's publish protocol, and the SHM_READ_MEMORY_BARRIER below is the other
 * half, so a caller that reaches here without having read the stamp first would let the entry reads
 * below overtake the stamp that vouches for them.
 */
boolean_t	gvcst_blk_sidx_locate(sm_uc_ptr_t sidx, int sidx_size, sm_uc_ptr_t buff, block_id blk,
					unsigned char *target, int target_len, blk_sidx_samp *samp)
{
	blk_sidx_hdr	*hdr;
	unsigned short	*ent_off;
	sm_uc_ptr_t	ent;
	unsigned short	rec_off, key_len, ent_at, found_at = 0;
	int		bsiz, lo, hi, mid, cmp, nent, found, match, mtop;

	hdr = (blk_sidx_hdr *)sidx;
	bsiz = (int)((blk_hdr_ptr_t)buff)->bsiz;
	SHM_READ_MEMORY_BARRIER;	/* A builder writes the entries, barriers, then publishes the stamp, so
					 * a reader has to read the stamp before the entries it vouches for. The
					 * stamp read this pairs with is the caller's BLK_SIDX_DESCRIBES, not one
					 * here - see the CALLER CONTRACT above. Repeating the test here would
					 * cost four comparisons on the hottest path and could not fail, the
					 * closing check below being what actually makes the sample trustworthy.
					 */
	nent = (int)hdr->nent;
	/* Bound "nent" by DIVIDING the room available rather than by multiplying "nent" out. "hdr->nent"
	 * is a uint4 read from shared memory and nothing here trusts it : at a value near 2^30 the product
	 * "nent * SIZEOF(unsigned short)" overflows a signed int and goes NEGATIVE, so a multiplied form of
	 * this test passes, and the binary search below then indexes "ent_off" a gigabyte past the slot.
	 * The room reserved is the offset array plus one entry header, so a slot claiming more entries than
	 * it could hold even a single entry for is refused here rather than on the first pass round the
	 * loop.
	 */
	if ((BLK_SIDX_MIN_ENT > nent)
			|| (nent > ((sidx_size - (int)SIZEOF(blk_sidx_hdr) - BLK_SIDX_ENT_HDR_SZ)
					/ (int)SIZEOF(unsigned short))))
		return FALSE;
	ent_off = (unsigned short *)(sidx + SIZEOF(blk_sidx_hdr));
	/* Binary search for the last entry that sorts strictly before the target. */
	lo = 0;
	hi = nent - 1;
	found = -1;
	while (lo <= hi)
	{
		mid = lo + ((hi - lo) / 2);
		/* Read the offset ONCE. A builder may be rewriting this slot underneath us, so checking
		 * "ent_off[mid]" and then using "ent_off[mid]" would be checking one value and using
		 * another, and the bound just established would not be the bound that applies. The same
		 * reason "nent", "rec_off" and "key_len" are pulled into locals before being tested.
		 *
		 * Only the UPPER bound is checked, and it is what keeps the two entry header reads below
		 * inside the slot : "ent_at" is an unsigned short, so without it "ent" could land up to 64KB
		 * past the slot base and off the end of the array. What it reserves is BLK_SIDX_ENT_HDR_SZ,
		 * exactly the four bytes those two reads consume - the key that follows them cannot be bounded
		 * yet, because its length is one of the four bytes not yet read, which is why the variable
		 * part gets its own test once "key_len" is in hand. A lower bound saying the entry lies past
		 * the offset array would catch only cases that are already harmless - "ent" stays inside the
		 * slot and "key_len" is range checked below - and could only arise from a slot being rewritten
		 * underneath us, which the closing stamp check catches anyway : a builder clears the stamp
		 * before it writes a byte, so a slot in that state fails that check and no sample is returned.
		 */
		ent_at = ent_off[mid];
		if ((int)ent_at > (sidx_size - BLK_SIDX_ENT_HDR_SZ))
			return FALSE;
		ent = sidx + ent_at;
		GET_USHORT(rec_off, ent);
		GET_USHORT(key_len, ent + 2);
		if ((0 == key_len) || (key_len > MAX_KEY_SZ)
				|| (((int)ent_at + BLK_SIDX_ENT_HDR_SZ + (int)key_len) > sidx_size))
			return FALSE;
		cmp = memcmp(ent + BLK_SIDX_ENT_HDR_SZ, target, MIN((int)key_len, target_len));
		if (0 <= cmp)
		{	/* at or after the target, so not a resume point */
			hi = mid - 1;
		} else
		{
			found = mid;
			found_at = ent_at;		/* the key itself is copied out after the loop */
			samp->rec_off = (int)rec_off;
			samp->key_len = (int)key_len;
			lo = mid + 1;
		}
	}
	if (0 > found)
		return FALSE;
	/* Work out here how much of the sampled key the target shares, on the entry that survived, rather
	 * than handing the key back for the caller to walk. The caller needs only this number : it seeds
	 * its match count with it and never looks at the key again. Doing it here means a pro build copies
	 * the key nowhere at all - one pass over the shared bytes replaces a copy of the whole key plus a
	 * walk of the shared prefix in the caller.
	 *
	 * Every move to the right in the search above made the entry it landed on the current best, so
	 * doing this inside the loop would repeat it for entries the next move discards. The bounds for
	 * these reads were checked when the entry was probed, and "found_at" is the value that was checked.
	 *
	 * It has to happen BEFORE the stamp check below, which is what makes the sample trustworthy : that
	 * check can only vouch for what has already been taken out of a builder's reach, and this number
	 * is as much a part of the sample as rec_off is.
	 */
	ent = sidx + found_at + BLK_SIDX_ENT_HDR_SZ;
	mtop = MIN(samp->key_len, target_len);
	for (match = 0; (match < mtop) && (ent[match] == target[match]); match++)
		;
	samp->match = match;
	/* The key itself is wanted only by the shadow check's diagnostics, so it is copied in DEBUG alone.
	 * This changes what is RECORDED, never what is done : both builds take the same path and reach the
	 * same answer.
	 */
	DEBUG_ONLY(memcpy(samp->key, ent, samp->key_len);)
	/* Range check the resume point ONCE, on the entry that survived. The search above never uses
	 * "rec_off" - only the key is compared - so of all the entries probed only the one that became the
	 * sample matters, and testing each of them would be a pair of compares per pass of the loop for an
	 * answer that is thrown away every time but the last.
	 *
	 * It has to be past the block header as well as inside the block, because this is where the caller
	 * RESUMES ITS WALK, and nothing downstream re-examines it : a resume short of the first record
	 * would put the walk off the record chain with no test between here and there to notice.
	 */
	if ((SIZEOF(blk_hdr) > samp->rec_off) || (samp->rec_off >= bsiz))
		return FALSE;
	/* Check the stamp, now that everything the caller will use is out of a builder's reach - rec_off
	 * and match, both in the caller's own "samp". The caller's BLK_SIDX_DESCRIBES on the way in does
	 * not cover this by itself : a builder can clear the stamp and start overwriting entries after
	 * that test and before the reads just above, in which case what was taken is a mixture of two
	 * images, and the key those reads measured need not belong to the rec_off beside it.
	 *
	 * What that costs if it goes unchecked is not a slow search but a WRONG ONE. The caller resumes at
	 * samp.rec_off with its match count seeded from samp.match, so a count measured against some other
	 * record's key seeds it wrongly: seeded too low the walk skips every following record whose
	 * compression count exceeds the seed and sails past the answer to the star key, seeded too high it
	 * breaks out early. See the invariant at the top of gvcst_blk_sidx.h.
	 *
	 * Returning FALSE costs nothing worth weighing against that. The caller simply walks the block as
	 * it did before this feature existed, and the slot is left alone for the builder to finish.
	 *
	 * Reading the stamp on both sides of those reads - the caller's BLK_SIDX_DESCRIBES before,
	 * this one after - is sound even though a whole rebuild could in principle fit between the
	 * two and leave the stamp reading exactly as it did before, so that comparing the two sees
	 * nothing at all. A rebuild landing on the same blk, tn and bsiz is rebuilding the same
	 * block IMAGE, and a build is a deterministic function of that image: same records, same
	 * stride, same samples. The entries it wrote are byte for byte the ones just read, so
	 * "unchanged stamp" really does mean "unchanged entries" here, without a sequence number to
	 * tell the two apart.
	 */
	SHM_READ_MEMORY_BARRIER;	/* finish every read of the slot before reading the stamp that vouches
					 * for them; the builder publishes the stamp last, so a stamp read early
					 * could vouch for entries that were still being written
					 */
	/* Note the bsiz handed in : the LOCAL one captured on entry, which every entry bound above was
	 * validated against, not a fresh read of the block. gvcst_blk_sidx.h has why that is the stricter
	 * question, alongside the two sibling tests.
	 */
	if (!BLK_SIDX_STILL_DESCRIBES(sidx, buff, blk, bsiz))
		return FALSE;
	return TRUE;
}

#ifdef DEBUG
/* ==================== SEARCH INDEX SHADOW CHECK, DEBUG ONLY ====================
 *
 * Walk the block again, from the first record, with the search index taking no part, and require the
 * index-assisted search to have reached exactly the same answer. Where the two differ, the index has
 * sent the search to the wrong record, which is how a wrong resume point becomes keys out of order on
 * disk - a corruption INTEG finds hours later, in a block that gives no clue which search wrote it.
 *
 * This exists because the failure mode is close to invisible without it. A wrong search result
 * usually costs nothing observable: the caller re-reads, or commit-time validation restarts the
 * transaction, and only occasionally does it land as a bad update. An early version of this feature
 * carried a coherency bug that survived static review twice and passed 85 runs of the subtest that
 * eventually caught it; this check found it in ten minutes, because it fails at the moment the
 * wrong answer is produced rather than when the damage surfaces.
 *
 * Runs unconditionally in a DEBUG build. It costs a second full walk of every index block searched,
 * which is of no consequence in a build nobody measures, and being unconditional is the point: a
 * check that has to be switched on with an environment variable is a check that stops running.
 */

/* The plain walk. A faithful copy of the loop in "gvcst_search_blk" with the search index resume
 * removed - deliberately a copy and not a reimplementation, so that a disagreement is the index's
 * doing rather than a difference between two pieces of logic.
 */
static boolean_t	sidx_shadow_walk(gv_key *pKey, sm_uc_ptr_t pBlkBase, sm_uc_ptr_t pTop,
					boolean_t long_blk_id, boolean_t level0,
					int *prev_off, int *prev_match, int *curr_off, int *curr_match)
{
	int		nFlg, nTargLen, nMatchCnt, nTmp;
	sm_uc_ptr_t	pRecBase, pRec, pPrevRec = NULL;
	unsigned char	*pCurrTarg, *pTargKeyBase;
	unsigned short	nRecLen;

	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
	pRecBase = pBlkBase;
	nRecLen = SIZEOF(blk_hdr);
	nMatchCnt = 0;
	nTargLen = (int)pKey->end;
	nTargLen++;
	for (;;)
	{
		pRec = pRecBase + nRecLen;
		if (pRec >= pTop)
		{
			if (pRec > pTop)
				return FALSE;
			nTargLen = 0;
			if (!level0)
			{
				if (pRecBase == pBlkBase)
					return FALSE;
				nMatchCnt = 0;
			} else
			{
				pPrevRec = pRecBase;
				pRecBase = pRec;
			}
			break;
		}
		GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);
		if (0 == nRecLen)
			return FALSE;
		pPrevRec = pRecBase;
		pRecBase = pRec;
		EVAL_CMPC2((rec_hdr_ptr_t)pRec, nTmp)
		if (nTmp > nMatchCnt)
			continue;
		if (nTmp < nMatchCnt)
		{
			if ((bstar_rec_size(long_blk_id) == nRecLen) && !level0)
				nTargLen = 0;
			else
				nTargLen = nTmp;
			break;
		}
		pRec += SIZEOF(rec_hdr);
		do
		{
			if ((nFlg = *pCurrTarg - *pRec++) != 0)
				break;
			pCurrTarg++;
		} while (--nTargLen);
		if (0 < nFlg)
			nMatchCnt = (int)(pCurrTarg - pTargKeyBase);
		else
		{
			if ((bstar_rec_size(long_blk_id) == nRecLen) && !level0)
				nTargLen = 0;
			else
				nTargLen = (int)(pCurrTarg - pTargKeyBase);
			break;
		}
	}
	if (NULL == pPrevRec)
		return FALSE;
	*prev_off = (int)(pPrevRec - pBlkBase);
	*prev_match = nMatchCnt;
	*curr_off = (int)(pRecBase - pBlkBase);
	*curr_match = nTargLen;
	return TRUE;
}

/* Compare what the index-assisted search concluded against what the plain walk concludes.
 *
 * A reader holds no lock on a BG buffer, and MM maps the file itself, so another process can rewrite
 * the block underneath and make the two walks disagree with nothing wrong at all. USED_TN and
 * USED_BSIZ are the block image the search STARTED from, captured where "pTop" is computed in
 * "gvcst_search_blk"; a disagreement only means something if that same image is still in front of us.
 *
 * The window has to begin at the start of the search, not at the point where it resumed from a
 * sample. Both walks depend on "pTop", which is computed at entry, so a block that changes between
 * entry and the resume leaves them walking different images. Checking only from the resume onwards
 * reported every reorg subtest as corrupt: reorg shuffles records constantly, and all 8 cores
 * examined had the block change size between entry and the resume.
 */
void	gvcst_blk_sidx_shadow_check(gv_key *pKey, srch_blk_status *pStat, sm_uc_ptr_t pBlkBase,
				sm_uc_ptr_t pTop, boolean_t long_blk_id, boolean_t level0,
				trans_num used_tn, uint4 used_bsiz, int4 used_cycle, blk_sidx_samp *samp)
{
	int		s_prev_off, s_prev_match, s_curr_off, s_curr_match, i, n;
	char		buff[SIDX_SHADOW_OUT_LINE], keybuf[128], sampbuf[128], recbuf[128];
	unsigned short	rec_rsiz;
	int		rec_cmpc, rec_sfx;
	sm_uc_ptr_t	rec;
	cache_rec_ptr_t	cr;

	if (!sidx_shadow_walk(pKey, pBlkBase, pTop, long_blk_id, level0,
				&s_prev_off, &s_prev_match, &s_curr_off, &s_curr_match))
		return;			/* the block did not walk cleanly : nothing to compare against */
	if ((s_prev_off == (int)pStat->prev_rec.offset) && (s_prev_match == (int)pStat->prev_rec.match)
			&& (s_curr_off == (int)pStat->curr_rec.offset)
			&& (s_curr_match == (int)pStat->curr_rec.match))
		return;			/* agreed, which is the overwhelmingly common case */
	cr = pStat->cr;
	if ((used_tn != ((blk_hdr_ptr_t)pBlkBase)->tn) || (used_bsiz != (uint4)((blk_hdr_ptr_t)pBlkBase)->bsiz))
		return;			/* the block was rewritten under us : the disagreement says nothing */
	if ((NULL != cr) && (cr->in_tend || (used_cycle != cr->cycle) || (cr->blk != pStat->blk_num)))
		return;			/* BG: tn and bsiz are not enough. "bg_update_phase2" copies the buffer
					 * outside crit, so the records can be mid-rewrite while the header still
					 * reads unchanged, and "db_csh_getn" can recycle the buffer entirely. Seen
					 * as a record claiming rsiz 768 inside a 603 byte block, which no settled
					 * block can hold. Same three tests the build guard uses, for the same
					 * reason; leaving them out here made the check report BG buffer writes as
					 * search index faults. */
	n = (int)pKey->end;
	if (n > (int)((SIZEOF(keybuf) / 2) - 1))
		n = (int)((SIZEOF(keybuf) / 2) - 1);
	keybuf[0] = '\0';
	for (i = 0; i < n; i++)
		SNPRINTF(keybuf + (2 * i), SIZEOF(keybuf) - (2 * i), "%02x", (unsigned int)pKey->base[i]);
	/* The sample the search resumed from, exactly as it used it. Without this the slot can only be read
	 * after the fact, by which time it has been rebuilt and says nothing about what the search saw.
	 */
	n = samp->key_len;
	if (n > (int)((SIZEOF(sampbuf) / 2) - 1))
		n = (int)((SIZEOF(sampbuf) / 2) - 1);
	sampbuf[0] = '\0';
	for (i = 0; i < n; i++)
		SNPRINTF(sampbuf + (2 * i), SIZEOF(sampbuf) - (2 * i), "%02x", (unsigned int)samp->key[i]);
	/* The record the sample points at, read HERE while the block still holds the image the search saw.
	 * By the time a core is examined this block has been rewritten many times over and its bytes say
	 * nothing. Reading the record here separates the two possible faults: if rec_cmpc + rec_sfx equals
	 * the sample's key_len and the suffix matches the tail of the sample's key, the entry is consistent
	 * and the disagreement is in how "locate" compares against the target; if the sum and the suffix do
	 * not match the sample, the entry carries a key that is not the key of its record, and the build
	 * produced it.
	 */
	rec_rsiz = 0;
	rec_cmpc = rec_sfx = -1;
	recbuf[0] = '\0';
	if ((samp->rec_off >= (int)SIZEOF(blk_hdr)) && (samp->rec_off < (int)(pTop - pBlkBase)))
	{
		rec = pBlkBase + samp->rec_off;
		GET_USHORT(rec_rsiz, &((rec_hdr_ptr_t)rec)->rsiz);
		EVAL_CMPC2((rec_hdr_ptr_t)rec, rec_cmpc);
		rec_sfx = (int)rec_rsiz - (int)SIZEOF(rec_hdr) - SIZEOF_BLK_ID(long_blk_id);
		n = rec_sfx;
		if (n > (int)((SIZEOF(recbuf) / 2) - 1))
			n = (int)((SIZEOF(recbuf) / 2) - 1);
		if ((0 < n) && ((rec + SIZEOF(rec_hdr) + n) <= pTop))
		{
			for (i = 0; i < n; i++)
				SNPRINTF(recbuf + (2 * i), SIZEOF(recbuf) - (2 * i), "%02x",
					(unsigned int)*(rec + SIZEOF(rec_hdr) + i));
		}
	}
	SNPRINTF(buff, SIDX_SHADOW_OUT_LINE, "Search index returned a different record than a plain walk of the same "
			"block : blk = 0x%llx : lvl = %d : bsiz = %u : tn = %llu : index(prev = %u/%u curr = %u/%u) : "
			"walk(prev = %d/%d curr = %d/%d) : samp(rec_off = %d key_len = %d key = %s) : "
			"rec(rsiz = %d cmpc = %d sfx = %d suffix = %s) : key = %s",
		(long long unsigned int)pStat->blk_num, (int)((blk_hdr_ptr_t)pBlkBase)->levl,
		(unsigned int)((blk_hdr_ptr_t)pBlkBase)->bsiz, (long long unsigned int)((blk_hdr_ptr_t)pBlkBase)->tn,
		(unsigned int)pStat->prev_rec.offset, (unsigned int)pStat->prev_rec.match,
		(unsigned int)pStat->curr_rec.offset, (unsigned int)pStat->curr_rec.match,
		s_prev_off, s_prev_match, s_curr_off, s_curr_match,
		samp->rec_off, samp->key_len, sampbuf,
		(int)rec_rsiz, rec_cmpc, rec_sfx, recbuf, keybuf);
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(buff));
	assert(FALSE);			/* a wrong search on an unchanged block is a bug, every time */
}
#endif /* DEBUG */
