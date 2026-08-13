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

#ifndef GVCST_BLK_SIDX_H_INCLUDED
#define GVCST_BLK_SIDX_H_INCLUDED

/* YDB#1143 : a SEARCH INDEX for an index block, held in shared memory in a bounded cache of slots
 * addressed by block number, never on disk.
 *
 * Records in an index block store their keys compressed against their predecessor, so a key cannot
 * be read without reading everything ahead of it. Compression against the preceding record is what
 * forces the search to be linear, and index-block scanning measures at about a third of runtime on
 * an update-heavy workload.
 *
 * Holding it in shared memory costs no fanout, needs no on-disk change at all, and - the real point
 * - leaves room to store WHOLE keys. Whole keys can be binary searched, so this is O(log n) over the
 * sampled records rather than a jump into a linear walk. Earlier approaches to this issue are in the
 * closed merge requests on it.
 *
 * Layout. A BOUNDED CACHE of slots, addressed by block number:
 *
 *	slot = block number & (slots - 1)		slots is rounded up to a power of two
 *
 * A worked example makes the arithmetic concrete: a 4096 byte index block holding 271 records of
 * ^CUST("0000001"), ^CUST("0000002") and so on, with a 1024 byte search index.
 *
 * As the block is stored, every key but the first is compressed against the record before it, so
 * most records hold only the bytes that differ from their predecessor:
 *
 *	INDEX BLOCK, 4096 bytes allocated, bsiz = 4089 used
 *
 *	off 0        16        42        57                     4077   4089
 *	    +--------+---------+---------+-- ... --+---------+--+------+
 *	    |blk_hdr | rec 0   | rec 1   |         | rec 269 |  | *key |
 *	    | 16 B   | 26 B    | 15 B    |         | 15 B    |  | 12 B |
 *	    +--------+---------+---------+-- ... --+---------+--+------+
 *
 *	one record:   +------+------+------------------+---------------+
 *	              | rsiz | cmpc | key SUFFIX only  | block pointer |
 *	              | 2 B  | 2 B  | rsiz - 4 - 8     | 8 B           |
 *	              +------+------+------------------+---------------+
 *
 *	  rec  off   rsiz cmpc  stored suffix   the key it means (must be REBUILT)
 *	    0    16    26    0  CUST_0000001__  ^CUST("0000001")
 *	    1    42    15   11  2__             ^CUST("0000002")
 *	    2    57    15   11  3__             ^CUST("0000003")
 *	  199  2412    15   11  0__             ^CUST("0000200")
 *	  270  4077    12    0  <none>          star key, rightmost child
 *
 * Record 199 stores three bytes, and its key appears nowhere in the block. Knowing it means walking
 * records 0 through 199 in order, applying each compression count to the key rebuilt so far. That
 * walk is the cost this scheme removes.
 *
 * The search index for that block holds WHOLE keys, so it can be binary searched:
 *
 *	SEARCH INDEX SLOT, 1024 bytes, in shared memory only, never on disk
 *
 *	off 0             40                128                        830      1024
 *	    +--------------+-----------------+--------------------------+--------+
 *	    | blk_sidx_hdr | offset array    | entries: WHOLE keys      | unused |
 *	    |   40 B       | 44 x 2 B = 88 B | 39 x (4 + 14) = 702 B    | 194 B  |
 *	    +--------------+-----------------+--------------------------+--------+
 *
 *	blk_sidx_hdr (40 B)             one entry (4 B + key)
 *	  blk          8 B  at   0        rec_off   2 B   offset of the record in the block
 *	  tn           8 B  at   8        key_len   2 B   length of the whole key
 *	  nent         4 B  at  16        key       n B   the key itself, uncompressed
 *	  bsiz         4 B  at  20
 *	  build_latch  8 B  at  24
 *	  skips        4 B  at  32
 *
 * The offset array is what makes the binary search possible; entries are variable length because
 * keys are stored whole. Each of its elements is a byte offset from the START OF THE SLOT to an
 * entry, so an element ranges over the entries region - 128 to 830 in the picture above - and never
 * over the block.
 *
 * The array is in ASCENDING KEY ORDER, which is the invariant the binary search in
 * "gvcst_blk_sidx_locate" rests on. It holds because the builder walks the block from front to back
 * and appends a sample every "stride" records, and a block's records are themselves in key order, so
 * appending in walk order is appending in key order. Nothing sorts the array afterwards.
 *
 * Sampling is spread over the WHOLE block rather than filling from the front, so the far half of a
 * block is helped as much as the near half. See "gvcst_blk_sidx_build":
 *
 *	max_ent = (1024 - 40) / (2 + 4 + (4089 / 271) + 1)  = 984 / 22 = 44
 *	stride  = (271 + 44 - 1) / 44                       = 7
 *
 * every number there coming from the example above : 1024 is the search index size and 40 is
 * SIZEOF(blk_sidx_hdr), so 984 is what is left for samples; 2 is one element of the offset array and
 * 4 is BLK_SIDX_ENT_HDR_SZ, the rec_off and key_len that head an entry; 4089 is the block's bsiz and
 * 271 its record count, so 4089 / 271 is the average record size, 15 bytes, standing in generously
 * for the key length, since a record also carries that 4 byte header and an 8 byte block pointer; and
 * the trailing 1 is a byte of slack on top of that average, NOT a rounding up of it - it is added to
 * the truncated quotient, so it exceeds DIVIDE_ROUND_UP by one whenever the division comes out exact.
 * 2 + 4 + 15 + 1 is the 22 bytes one sample is assumed to cost.
 *
 * So every 7th record is sampled, giving 39 entries. A search binary searches those 39 whole keys
 * and resumes the ordinary walk at the sampled record, at most 6 records short of its answer
 * instead of up to 270.
 *
 * BG and MM address it identically. BG began as one slot per global buffer, which measurement
 * showed to be wrong on two counts. On the threeen1gE benchmark with 10000 global buffers the whole
 * index-block WORKING SET was 813 blocks, but those 813 blocks passed through 9041 different global
 * buffers over the run: a per-buffer index is thrown away and rebuilt every time a block moves to
 * another buffer, and 10000 slots were being held to describe 813 blocks. Keying on the block
 * number instead makes the index survive buffer reuse and sizes the memory to the working set
 * rather than to the buffer pool. 4096 slots covered that working set with 99.8 PCT of searches
 * finding their slot uncontended.
 *
 * Coherency. A search index is only a hint about a block image that can change under a reader at any
 * time. It is stamped with the block number and the block's transaction number, and both must match
 * before it is used:
 *
 *	blk	the slot describes some other block, which is a collision
 *	tn	the block was updated, which always advances its tn
 *
 * That makes a search index SELF-INVALIDATING: nothing has to hunt one down and clear it when a block
 * is updated, and in particular no work is added under crit. A stale search index simply fails the
 * stamp check, and the search that met it walks the block from its first record.
 *
 * A cycle was once part of the stamp, to catch a global buffer being recycled to the same block. Slot
 * addressing removes the need: what is being identified is the IMAGE, and (blk, tn) identifies an
 * image no matter which buffer, or none, happens to hold it.
 *
 * THE INVARIANT THE WHOLE SCHEME RESTS ON: within one slot, an entry's key must be the key of the
 * record at that entry's rec_off. A search resumes at rec_off and seeds its match count from the
 * entry's key, so an entry that pairs one record's key with another record's offset seeds that count
 * wrongly, and a search seeded wrongly does not merely run slowly - it returns the wrong record. Too
 * low a seed makes the walk skip every following record whose compression count exceeds it, so the
 * search sails past its answer to the star key; too high a seed breaks it out early. Either way a
 * wrong record goes on to be updated, which is keys out of order on disk.
 *
 * Three things protect that invariant, and ALL are needed:
 *
 *	1. Only one process at a time may WRITE a slot, enforced by build_latch. Two builders
 *	   interleaving leave a slot holding entries from two different builds, and whichever
 *	   finishes last publishes a perfectly valid stamp over the mixture. No stamp check can
 *	   detect that, because the stamp is not what went wrong.
 *
 *	2. A reader re-checks the stamp AFTER it has taken everything it needs out of the slot. Checking
 *	   only on the way in leaves the reader open to a builder that invalidates the stamp and starts
 *	   overwriting entries in between, which would hand back a sample drawn from two images.
 *
 *	3. A build only publishes an index for a block that NOBODY WROTE while it was being walked.
 *	   An updater writes a block in place, so a builder can read some records before the write and
 *	   some after, and splice two images together.
 *
 *	   BG and MM need DIFFERENT tests for this, because they write a block at different points of
 *	   the commit, and each one's test is useless on the other.
 *
 *	   BG writes the buffer in phase 2 at step CMT18, outside crit, after the commit already
 *	   finished at CMT13 - so the tn that copy lays down is OLDER than any curr_tn the build could
 *	   have read, and no comparison against curr_tn can see a phase 2 copy at all - such a test
 *	   on BG is not merely useless but harmful, since all it can do is reject builds that were
 *	   perfectly good. What brackets the copy is cr->in_tend, set in phase 1 and cleared after the
 *	   copy behind SHM_WRITE_MEMORY_BARRIER, so a process that sees it clear sees the whole copy.
 *
 *	   The build READS in_tend twice, once before the walk and once after, and the two catch
 *	   different copies. The first catches one that was ALREADY RUNNING when the header was read.
 *	   The second catches one that STARTED DURING the walk and is still running. A copy that begins
 *	   and ends entirely between the two reads is invisible to both, and is caught instead by the
 *	   tn comparison below, since it writes a tn different from the one read before the walk.
 *
 *	   The POSITION of the first read is what that division rests on: it is taken immediately AFTER
 *	   the block header has been read, never before. Its job is one specific copy - one that has
 *	   ALREADY WRITTEN the header just read and has NOT YET finished the block. That copy is the only
 *	   one the tn comparison cannot see, because memcpy copies forward: the header carries the new tn
 *	   at both ends of the walk, so tn equals end_tn over a body spliced from two images. in_tend is
 *	   set in phase 1 before the copy writes anything and cleared only after it has written
 *	   everything, so such a copy has in_tend non-zero for as long as it runs, and reading in_tend
 *	   after the header is what puts the read inside that interval. A read barrier keeps the in_tend
 *	   load from floating above the header load.
 *
 *	   It is not asked to catch anything else, and does not. A copy that starts after the header read
 *	   and finishes before this one leaves tn holding the OLD header and end_tn the new one, which the
 *	   tn comparison catches. A copy that finished before the header read leaves header and body both
 *	   from the new image, which is consistent and wants no catching at all.
 *
 *	   Read it ahead of the header instead, and the case it exists for escapes, over a window a
 *	   context switch can widen without limit: the read is clear because the commit has not started
 *	   yet; the commit then runs phase 1, reaches CMT13, releases crit and starts its phase 2 copy;
 *	   the header read returns the NEW tn over a partly copied body; the copy finishes inside the
 *	   walk; in_tend reads clear at both ends, and end_tn equals the tn already read. Every post-walk
 *	   test passes, and the index is published stamped with the block's REAL CURRENT tn - poisoning
 *	   that never expires, because no later reader has grounds to doubt a stamp naming the block's
 *	   own current tn.
 *
 *	   BG's post-walk test looks at two things in_tend cannot speak for. cr->cycle with cr->blk
 *	   catches "db_csh_getn" recycling the cache record to a DIFFERENT block mid-walk, whose content
 *	   has nothing to do with the block being indexed. cr->data_invalid needs no test of its own,
 *	   being set and reset strictly inside in_tend.
 *
 *	   MM writes the block in place under crit, between CMT02 and CMT13, and early_tn tracks exactly
 *	   that: raised at CMT02 before anything is written, settling when curr_tn catches up at CMT13.
 *	   So (curr_tn == early_tn) asks whether a commit is in flight AT THAT INSTANT, and the build
 *	   asks it ONCE, below the header read. Everything after that rests on the block's own tn.
 *
 *	   MM needs only the one sample where BG needs two in_tend reads, and the asymmetry is the point.
 *	   BG cannot trust its own block tn mid-copy, phase 2 memcpying the block forward so that tn and
 *	   end_tn both read the new number over a spliced body. MM's writer lays the tn down BEFORE the
 *	   records and behind a write barrier, so a walk that read any byte of a concurrent write also
 *	   sees the new tn at "tn != end_tn". The sample covers the one case that test cannot see - a
 *	   commit that had written this block's header before the build read it and was still writing the
 *	   body - and nothing is left over for a second sample after the walk. One there would also
 *	   refuse a build whenever any commit anywhere was in flight at that instant, which is a large
 *	   share of them at 16 jobs.
 *
 *	   Asking it at an instant rather than across the whole walk is what makes MM usable under
 *	   concurrency. The earlier form noted curr_tn before the walk and required early_tn to equal it
 *	   afterwards, which refused the build whenever ANY commit touched ANY block in the region while
 *	   the walk ran. At 16 jobs that is nearly every walk, so a builder paid for a full walk and
 *	   published nothing: 3.4 PCT slower than the feature switched off, where one job was 14.9 PCT
 *	   faster. A commit that begins and ends inside the walk is now kept or refused on its merits -
 *	   refused by "tn != end_tn" if it touched THIS block, kept if it touched some other one.
 *
 *	   The first sample has to sit BELOW the header read, for the same reason BG's first in_tend read
 *	   does, and against the same hazard. "gvcst_blk_build" writes the block header, tn included,
 *	   BEFORE the records and behind a write barrier, so a commit already past that point leaves the
 *	   new tn visible at both ends of the walk over a body still being rewritten, where no comparison
 *	   of the block's own tn can see it. Sampled above the header read instead, a commit starting in
 *	   the gap would read as quiescent, write the header, and rewrite the body throughout the walk.
 *
 *	   Each sample reads curr_tn, barriers, then reads early_tn. The barrier is load bearing: with
 *	   the two loads reordered, a commit in flight at the first and finished by the second reads back
 *	   as quiescent. Reading curr_tn first also makes the pair err towards refusing a good build
 *	   rather than accepting a torn one.
 *
 *	   That test is only sound because "t_end" and "tp_tend" carry an MM write barrier immediately
 *	   after early_tn is set at CMT02. Without it a weakly ordered platform can show a builder the
 *	   block change without the early_tn change, and there is then no ordered marker to fall back
 *	   on, since the block's own header is what lags. The barrier costs about 14 PCT on MM at 16
 *	   jobs on aarch64, where MM_WRITE_MEMORY_BARRIER is "dmb ish", and nothing at all on x86_64 or
 *	   on BG, which never executes it. It is carried deliberately: MM on a weakly ordered platform
 *	   is exactly where a coherency bug would sit unexercised.
 *
 *	   Two more comparisons, against the same post-walk header snapshot, apply to both:
 *
 *	     - the block's tn as it was BEFORE the walk. On BG a phase 2 copy that begins AND ends
 *	       inside the walk leaves in_tend clear at both reads, and only this sees it. It fired on
 *	       the first BG run when it was briefly an assert.
 *	     - the block's bsiz as it was before the walk. No case is known that reaches this one; it is
 *	       kept because twice one of these two tests was judged implied by the others and demoted to
 *	       an assert, and twice testing proved that wrong - tn on the first BG run, bsiz repeatedly
 *	       on MM after that. It costs one comparison and can only refuse a build, never accept a bad
 *	       one.
 *
 * Every way a concurrent writer can disturb this, and what catches each one. Two tables, because
 * the hazards a BUILD faces while it walks a block are not the ones a SEARCH faces when it comes
 * to use a slot. Reading them as tables shows that no check covers more than its own row, and
 * that several rows rest on one check with nothing behind it - marked ONLY:
 *
 *	while a BUILD walks the block                             what catches it
 *	--------------------------------------------------------  ----------------------------------
 *	another builder writes the same slot at once              build_latch, item 1
 *	the builder dies holding the latch                        salvage in gvcst_blk_sidx_build
 *	BG: copy in flight at the first in_tend read              first in_tend read, ONLY this if it
 *	                                                              had already written the header
 *	BG: copy in flight at the second in_tend read             second in_tend read
 *	BG: copy in flight at neither, but ran between them       tn != end_tn, ONLY this
 *	BG: copy finished before the builder's walk began         nothing needed, image consistent
 *	BG: cache record recycled to another block mid-walk       cr->cycle and cr->blk, ONLY these
 *	MM: commit already writing this block when the walk       the quiescence sample, ONLY this - it
 *	    began, its header down before the build read it           had put the new tn down already
 *	MM: commit writes this block after the build read the     tn != end_tn, ONLY this - the writer
 *	    header, at any point                                      lays the tn down before the records
 *	MM: commit writes some OTHER block during the walk        nothing needed, this block untouched
 *	the block changes size under the builder's walk           bsiz != end_bsiz, no case known
 *	a record reads as damaged, as a torn block often does     the walk's own bounds checks, which
 *	                                                              abandon the build unstamped
 *
 *	when a SEARCH comes to use a slot                         what catches it
 *	--------------------------------------------------------  ----------------------------------
 *	a build is part way through writing that slot             stamp cleared before writing, so
 *	                                                              BLK_SIDX_DESCRIBES fails
 *	a builder rewrites the slot mid-read                      the reader's stamp re-check, item 2
 *	the block was updated after the index was built           tn in the stamp, self-invalidating
 *	another block claims the same slot                        blk in the stamp
 *	a TP private image reaches a search or a build            CYCLE_PVT_COPY gate in the caller
 *
 * One hazard is deliberately absent from both tables : the block being rewritten under the SEARCH
 * itself. That invalidates an ordinary walk from the first record exactly as much as one resumed from
 * a sample, so it is not something this feature introduces or has to answer for - "t_qread" and the
 * tn validation at commit are what cover it.
 *
 * Back to the three protections listed well above - the build_latch, the reader's second stamp check,
 * and the rule that a build publishes nothing for a block somebody wrote while it was being walked.
 * NONE ALONE IS SUFFICIENT, which is the reason all three are carried. The latch does not stop a
 * reader consuming a build in progress. The reader's re-check cannot help against interleaved
 * builders, whose damage is complete and stamped before that reader ever looks. And neither of them
 * notices a build that read a block mid-update, which is what the whole of item 3 above exists for.
 *
 * This is not reasoning from first principles after the fact. YDB#1143 shipped with none of the three
 * and reproduced as INTEG "keys out of order", and as a slot holding a key for one global at its
 * first record and keys for another after it, which no sorted block can contain.
 *
 * One thing deliberately has no index and always walks the block from its first record: a PRIVATE
 * BLOCK IMAGE. In TP a search can run against a cw-set element's own copy of a block rather than
 * against the shared global buffer. Its content is the transaction's own and does not match the
 * committed image the slot describes, while its tn has not yet moved to say so, so it is left to the
 * ordinary search rather than allowed to read or to populate a shared slot.
 */

/* Sizing is two file header fields, both set by MUPIP SET -SEARCH_INDEX_SIZE=bytes[,slots] and both
 * 0 by default, which allocates nothing at all:
 *
 *	search_idx_size		bytes per slot
 *	search_idx_slots	how many slots
 *
 * They have to live in the file header rather than in the environment because they decide how large
 * the shared segment is; see their own comments in gdsfhead.h. gdsbt.h carries the bounds MUPIP SET
 * validates against.
 */
#define	BLK_SIDX_MIN_ENT	4	/* fewer entries than this is not worth a binary search */

/* A search index describing a block whose tn is this cannot exist : 0 is never a live block's tn, so a
 * zeroed (freshly created) shared memory segment reads as "no search index here" without needing a pass
 * to initialize one.
 */
#define	BLK_SIDX_TN_NONE	0

/* How many times a slot may turn away a different block before that block takes it anyway. The slots
 * are a shared cache, so two blocks can want the same one. Unchecked, two colliding blocks evict each
 * other on every search and both rebuild constantly - measured at 2.3x SLOWER than having no index at
 * all. Refusing to evict fixes that but would let a rarely searched block keep a slot a busier one
 * needs, so the refusal is counted and gives way after this many attempts.
 */
#define	BLK_SIDX_SKIP_LIMIT	4

typedef struct blk_sidx_hdr_struct
{
	block_id	blk;		/* block number described; a mismatch is a slot collision */
	trans_num	tn;		/* blk_hdr.tn of the image described; BLK_SIDX_TN_NONE if unbuilt */
	uint4		nent;		/* how many entries follow */
	uint4		bsiz;		/* bsiz of the image described */
	global_latch_t	build_latch;	/* held by the one process building this slot; see gvcst_blk_sidx_build */
	uint4		skips;		/* how many times another block has been turned away; see the reader */
} blk_sidx_hdr;

/* One sampled record. "key" is the WHOLE key, uncompressed - that is the point of holding this
 * outside the block. A search that resumes at a record has to seed its match count from the full key
 * of that record; seeding it short is not a smaller optimization, it is wrong, because the walk then
 * skips every following record whose compression count exceeds the seeded value.
 */
/* An entry is rec_off and key_len, each an "unsigned short", followed by key_len bytes of key. It is
 * read and written field by field with GET_USHORT and PUT_USHORT rather than through a struct, being
 * variable length and landing at an arbitrary offset inside the slot. The picture above has the
 * layout.
 */
#define	BLK_SIDX_ENT_HDR_SZ	(2 * SIZEOF(unsigned short))

/* GDS_ANY_SEARCHIDX and SEARCHIDX_AVAILABLE, which find a block's slot, are in gdsfhead.h. */

/* Three questions get asked of a stamp, and they are NOT the same question. They are defined together
 * so the differences can be read off rather than reconstructed from three call sites:
 *
 *	BLK_SIDX_DESCRIBES	does this slot describe the image in BUFF right now? All four fields.
 *				A reader asks this before using a slot, a builder before rebuilding one.
 *	BLK_SIDX_STILL_DESCRIBES is it still the image a reader took its sample from? The same tn, blk
 *				and bsiz, but bsiz against the value the CALLER captured rather than a
 *				fresh read of the block, which is the stricter question : a slot rebuilt
 *				for a block that changed size under the reader satisfies a fresh
 *				comparison, both sides having moved together, and fails this one. The
 *				BLK_SIDX_TN_NONE test is left out because it is subsumed - a live block's
 *				tn is never 0, so a cleared stamp already differs from it.
 *	BLK_SIDX_OWNED_BY_OTHER	does this slot belong to a DIFFERENT block? Only tn and blk. It must
 *				not test tn-equality or bsiz : an entry of this block's own gone stale has
 *				to fall through to a rebuild rather than be turned away by the damper.
 *
 * Both parts of a stamp go stale independently - the slot can be taken by another block, or this
 * block can be updated, which always moves its tn.
 */
#define	BLK_SIDX_DESCRIBES(SIDX, BUFF, BLK)							\
	((BLK_SIDX_TN_NONE != ((blk_sidx_hdr *)(SIDX))->tn)					\
		&& (((blk_sidx_hdr *)(SIDX))->tn == ((blk_hdr_ptr_t)(BUFF))->tn)		\
		&& (((blk_sidx_hdr *)(SIDX))->blk == (BLK))					\
		&& (((blk_sidx_hdr *)(SIDX))->bsiz == (uint4)((blk_hdr_ptr_t)(BUFF))->bsiz))

#define	BLK_SIDX_STILL_DESCRIBES(SIDX, BUFF, BLK, BSIZ)						\
	((((blk_sidx_hdr *)(SIDX))->tn == ((blk_hdr_ptr_t)(BUFF))->tn)				\
		&& (((blk_sidx_hdr *)(SIDX))->blk == (BLK))					\
		&& (((blk_sidx_hdr *)(SIDX))->bsiz == (uint4)(BSIZ)))

#define	BLK_SIDX_OWNED_BY_OTHER(SIDX, BLK)							\
	((BLK_SIDX_TN_NONE != ((blk_sidx_hdr *)(SIDX))->tn) && (((blk_sidx_hdr *)(SIDX))->blk != (BLK)))


/* What a search resumes from. It goes on the stack of gvcst_search_blk, the hottest function in this
 * codebase, so its size is not incidental : an earlier version carried the sampled key here in every
 * build, and at MAX_KEY_SZ that grew the frame from 40 bytes to 1192 and cost 4.5 to 6.5 PCT under
 * contention whether the feature was on or off. Moving the array off the stack fixed that and cost as
 * much again on the other side, a heap buffer not being reliably in cache where a stack one is.
 *
 * A pro build no longer needs the key at all - "gvcst_blk_sidx_locate" measures the shared prefix
 * against the slot and hands back only "match" - so the array is DEBUG only and the pro frame carries
 * three ints.
 */
typedef struct blk_sidx_samp_struct
{
	int		rec_off;	/* offset of the sampled record in the BLOCK, where the walk resumes */
	int		key_len;	/* length of the sampled key */
	int		match;		/* how many leading bytes the sampled key shares with the target, which
					 * is what the caller seeds its match count with. Computed by
					 * "gvcst_blk_sidx_locate" while it still has the key in hand, so that a
					 * pro build never has to copy the key anywhere to find it out
					 */
	DEBUG_ONLY(unsigned char key[MAX_KEY_SZ + 1];)	/* the shadow check prints this; no pro build fills it */
} blk_sidx_samp;

/* Build a search index describing the block image in BUFF. Cheap enough to do on demand: it walks the
 * block once, which is the very walk the search was about to do anyway.
 *
 * NOT INLINED, and that matters more than it looks. This has to rebuild whole keys as it walks -
 * every key in an index block is stored compressed against its predecessor - so it needs a MAX_KEY_SZ
 * scratch array. Inlined into "gvcst_search_blk" that array lands in the frame of the hottest
 * function in the codebase, where it is reserved on entry and so paid by EVERY search, including the
 * ones that never build anything and the ones with the feature turned off. Measured: a frame of 1192
 * bytes against master's 40 cost 4.5 to 6.5 PCT at 16 jobs and above, and 120 bytes cost nothing.
 * Whether the compiler inlines it is a heuristic that has already flipped once between builds here,
 * so it is pinned rather than left to chance. Note the scratch array in question is the one in
 * "sidx_build_locked", not anything in blk_sidx_samp, which a pro build no longer carries a key in.
 * The two live in different translation units and this build has no LTO, so the attribute guards
 * against a change of build rather than against today's compiler. Builds happen on about 0.1 PCT of
 * searches, so the out-of-line call costs nothing worth measuring either way.
 */
void	gvcst_blk_sidx_build(sm_uc_ptr_t sidx, int sidx_size, sm_uc_ptr_t buff, int blk_size, block_id blk,
				cache_rec_ptr_t cr)
			__attribute__ ((noinline));

/* Binary search the search index for the last sampled record whose key sorts STRICTLY BEFORE the target.
 * Strictly, because the caller resumes at the record AFTER the one returned - a sample equal to the
 * target IS the answer, and resuming past it would step over it. Returns FALSE when the search index does
 * not describe this block image, or when no sample sorts before the target.
 */
boolean_t gvcst_blk_sidx_locate(sm_uc_ptr_t sidx, int sidx_size, sm_uc_ptr_t buff, block_id blk,
					unsigned char *target, int target_len, blk_sidx_samp *samp);

#ifdef DEBUG
/* Walk the block again without the search index and require the same answer, reporting and asserting
 * where the two disagree on a block image that did not change under the reader. DEBUG only, and
 * unconditional there: it costs a second full walk of every index block searched, which does not
 * matter in a build nobody measures, and gating it behind an environment variable would mean it
 * quietly stopped running. This is the check that caught both the coherency bug and the prefix bug
 * described above, each on a run whose subtest otherwise passed; keep it working when changing
 * anything in this file or in the resume in "gvcst_search_blk".
 */
#define	SIDX_SHADOW_OUT_LINE	(1024 + 1)

void	gvcst_blk_sidx_shadow_check(gv_key *pKey, srch_blk_status *pStat, sm_uc_ptr_t pBlkBase,
				sm_uc_ptr_t pTop, boolean_t long_blk_id, boolean_t level0,
				trans_num used_tn, uint4 used_bsiz, int4 used_cycle, blk_sidx_samp *samp);
#endif

#endif /* GVCST_BLK_SIDX_H_INCLUDED */
