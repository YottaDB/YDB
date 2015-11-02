/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSCC_H__
#define __GDSCC_H__

/* this requires gdsroot.h gtm_facilit.h fileinfo.h gdsbt.h gdsfhead.h gdir.h gdskey.h  */

#include <sys/types.h>

/* BIG_UA is the maximum size of a single update array specified as an unsigned quantity (usages rely on this). It is 16MB. */
#define BIG_UA  (uint4)16777216

#define CDB_R_SET_SIZE		32
#define CDB_CW_SET_SIZE 	(MAX_BT_DEPTH * 3 + 1 + 2)
#define CDB_W_SET_SIZE		16

/* CDB_CW_SET_SIZE = 24
 *	3 for all the levels (including updated block, newly created sibling and possible bitmap update)
 *	1 extra for the root level (to take care of gds_t_write_root case)
 *	2 in the case of creation of a new global variable (1 index block with a * key and 1 data block
 *	  containing the key)
 */

#define CDB_T_CREATE 0
#define CDB_T_WRITE 1
#define CDB_T_WRITE_ROOT 2

/* The following defines the write_type for a block that is going to be updated.
 * GDS_WRITE_PLAIN is the default type for most updates.
 * GDS_WRITE_BLOCK_SPLIT is set in case of a block update due to a block split. It is currently not used anywhere in the code.
 * GDS_WRITE_KILLTN requires a little more explanation.
 *
 * The TP commit logic ("tp_tend") makes use of an optimization referred to as the "indexmod" optimization.
 * This optimization tries to avoid a restart in the case where a TP transaction does a SET to a data block and later finds
 * at TCOMMIT time that the index block which was part of the SET had been updated by a concurrent SET (or a REORG split
 * operation) to a different data block (that also had the same index block as an ancestor) which resulted in a block split
 * causing the index block to be updated. In this case there is no reason to restart. The index block could have been
 * modified by other operations as well (e.g. M-kill, REORG coalesce or swap operations or any DSE command or a block split
 * operation that caused the height of the global variable tree to increase [C9B11-001813]). In these cases, we dont want
 * this optimization to take effect as we cant be sure everything that was relied upon for the TP transaction was still valid.
 * These disallowed operations are generically referred to as "kill" type of operations. This optimization is implemented by
 * having a field "killtn" (name derived from "kill" type of operations) in the bt (block-table) structure for each block.
 * This field is assigned the same value as the "tn" whenever an index block gets updated due to one of the disallowed operations.
 * Otherwise it stays untouched (i.e. killtn <= tn at all times). It is the "killtn" (and not "tn") that is used in the
 * cdb_sc_blkmod validation check in tp_tend if we are validating a index block. Since KILLs, MUPIP REORG and/or DSE operations
 * are usually rare compared to SET activity, most of the cases we expect the indexmod optimization to be in effect and
 * therefore help reduce the # of TP restarts due to index block changes. Note that each SET/GET/KILL operation in TP goes
 * through an intermediate validation routine tp_hist which does the cdb_sc_blkmod validation using "tn" (not "killtn").
 * Only if that passed, do we relax the commit time validation for index blocks.
 *
 * The operations that are not allowed to use this optimization (M-kill, REORG or DSE) are supposed to make sure they
 * set the write_type of the cw-set-element to GDS_WRITE_KILLTN. Failing to do so cause "killtn" in the bt to NOT be uptodate
 * which in turn can cause false validation passes (in the cdb_sc_blkmod check) causing GT.M processes to incorrectly commit
 * when they should not. This can lead to GT.M/application level data integrity errors.
 */
#define	GDS_WRITE_PLAIN		0
#define	GDS_WRITE_KILLTN	1
#define	GDS_WRITE_BLOCK_SPLIT	2
/* blk_prior_state's last bit indicates whether the block was free before update
 * BLOCK FREE:          0b*******1, BLOCK NOT FREE:     0b*******0
 */
#define BIT_SET_FREE(X)		((X) |= 0x00000001)
#define BIT_CLEAR_FREE(X)		((X) &= 0xfffffffe)
#define WAS_FREE(X)		((X) & 0x00000001)
/* blk_prior_state's last but one bit indicates whether the block was recycled before update
 * BLOCK RECYCLED:  	0b******1*, BLOCK NOT RECYCLED: 0b******0*
 */
#define BIT_SET_RECYCLED_AND_CLEAR_FREE(X)      ((X) = ((X) & 0xfffffffc) + 0x00000002)
#define BIT_CLEAR_RECYCLED_AND_SET_FREE(X)	((X) = ((X) & 0xfffffffc) + 0x00000001)
#define BIT_CLEAR_RECYCLED(X)	((X) &= 0xfffffffd)
#define BIT_SET_RECYCLED(X)	((X) |= 0x00000002)
#define WAS_RECYCLED(X)		(((X) & 0x00000002))
/* blk_prior_state's last but two bit indicates whether the block was in directory tree or global variable tree
 * IN_GV_TREE:         0b*****1**,  IN_DIR_TREE:        0b*****0**
 */
#define IN_GV_TREE 4
#define IN_DIR_TREE 0
#define BIT_SET_DIR_TREE(X)	((X) &= 0xfffffffb)
#define BIT_SET_GV_TREE(X)	((X) |= 0x00000004)
#define KEEP_TREE_STATUS 0x00000004


/* macro to traverse to the end of an horizontal cw_set_element list */

#define TRAVERSE_TO_LATEST_CSE(x)					\
{									\
	GBLREF	uint4		dollar_tlevel;				\
									\
	assert(dollar_tlevel);						\
	if (x)								\
                for ( ; (x)->high_tlevel; x = (x)->high_tlevel)		\
                        ;						\
}

typedef uint4	block_offset;
typedef int4   	block_index;

/* If a new mode is added to the table below, make sure pre-existing mode usages in the current codebase are examined to see
 * if the new mode needs to be added there as well. For example, there is code in tp_incr_commit.c and tp_incr_clean_up.c
 * where gds_t_create and kill_t_create are used explicitly. If the new mode is yet another *create* type, then it might need
 * to be added in those places as well.
 */
enum gds_t_mode
{
	gds_t_noop = 0,		/* there is code that initializes stuff to 0 relying on it being equal to gds_t_noop */
	gds_t_create,
	gds_t_write,
	gds_t_write_recycled,	/* modify a recycled block (currently only done by MUPIP REORG UPGRADE/DOWNGRADE) */
	gds_t_acquired,
	gds_t_writemap,
	gds_t_committed,	/* t_end   relies on this particular placement */
	gds_t_write_root,	/* t_end   relies on this being AFTER gds_t_committed */
	gds_t_busy2free,	/* t_end   relies on this being AFTER gds_t_committed */
	gds_t_recycled2free,	/* t_end   relies on this being AFTER gds_t_committed */
	n_gds_t_op,		/* tp_tend and other routines rely on this being BEFORE kill_t* modes and AFTER all gds_t_* modes */
	kill_t_create,		/* tp_tend relies on this being AFTER n_gds_t_op */
	kill_t_write,		/* tp_tend relies on this being AFTER n_gds_t_op */
};

typedef struct key_value_struct
{
	gv_key			key;			/* note that the following array holds the actual key contents */
	char			key_contents[DBKEYSIZE(MAX_KEY_SZ)];
	mstr			value;
	struct key_value_struct	*next;
} key_cum_value;

/* Create/write set element. This is used to describe modification of a database block */

typedef struct cw_set_element_struct
{
	trans_num	tn;				/* transaction number for bit maps */
	sm_uc_ptr_t	old_block;			/* Address of 'before-image' of block to be over-written */
	cache_rec_ptr_t	cr;
        struct cw_set_element_struct  *next_cw_set;
        struct cw_set_element_struct  *prev_cw_set;	/* linked list (vertical) of cw_set_elements with one link per block */

        struct cw_set_element_struct  *high_tlevel;
        struct cw_set_element_struct  *low_tlevel;	/* linked list (horizontal) of cw_set elements for a given block with
							 * different transaction levels. Latest cw_set_elements (for a given block)
							 * are inserted at the beginning of the horizontal list	*/
	off_jnl_t	jnl_freeaddr;			/* journal update address */
	uint4		write_type;			/* can be GDS_WRITE_PLAIN or GDS_WRITE_KILLTN or GDS_WRITE_BLOCK_SPLIT
							 * or bit-wise-or of both */
	key_cum_value	*recompute_list_head;		/* pointer to a list of keys (with values) that need to be recomputed */
	key_cum_value	*recompute_list_tail;		/* pointer to a list of keys (with values) that need to be recomputed */
	enum gds_t_mode	mode;				/* Create, write, or write root	*/
	block_id	blk;				/* Block number or a hint block number for creates */
	unsigned char	*upd_addr;			/* Address of the block segment array containing update info
							 * for this block */
        unsigned char   *new_buff;     			/* Address of a buffer created for each global mentioned inside of a
                                       			 * transaction more then once (for tp) */
	gv_namehead	*blk_target;			/* address of the "gv_target" associated with a new_buff
							 * used to invalidate clues that point to malloc'ed copies */
	int4		cycle;

 	/* When a block splits a new block must be created and the parent must be updated to
	 * to have a record pointing to the new block.  The created block number will not be
	 * known until the last possible moment.  Thus it is not possible to completely modify
	 * the parent.  The following 2 fields are used in such a case. "ins_off" tells where
	 * the created block's number should be put in the parent block. "index" tells which
	 * element of the create/write set is being created.
	 */

        block_offset    first_off;
	block_offset	ins_off;			/* Insert block number offset */
        block_offset    next_off;
	block_index	index;				/* Insert block number index */
	int4		reference_cnt;			/* Relevant only for a bitmap block.
							 *    > 0 => # of non-bitmap blocks to be allocated in this bitmap;
							 *    < 0 => # of non-bitmap blocks to be freed up in this bitmap;
							 *   == 0 => change to bitmap block without any non-bitmap block change
							 * Used to update csd->free_blocks when the bitmap block is built
							 */
	int4		level;				/* Block level for newly created blocks	*/
        boolean_t	done;           		/* Has this update been done already? */
	boolean_t	first_copy;			/* If overlaying same buffer, set if first copy needed */
							/* just an optimisation - avoids copying first few bytes, if anyway
							 * we are just overlaying the new_buff in the same transaction */
	boolean_t	forward_process;		/* Need to process update array from front when doing kills */
	uint4		t_level;			/* transaction level associated with cw element, for incremental rollback */
	enum db_ver	ondsk_blkver;			/* Actual block version from block header as it exists on disk.
							 * If "cse->mode" is gds_t_write_root, this is uninitialized.
							 * If "cse->mode" is gds_t_create/gds_t_acquired, this is GDSVCURR.
							 * Otherwise, this is set to cr->ondsk_blkver (cr is got from the history).
							 * Whenever "cse->old_block" is reset, this needs to be reset too (except
							 *	in the case of gds_t_create/gds_t_acquired).
							 */
	int4		old_mode;			/* Saved copy of "cse->mode" before being reset to gds_t_committed.
							 * Is negated at end of bg_update_phase1 to indicate (to secshr_db_clnup)
							 * that phase1 is complete. Is negated back to the postive value at end
							 * of bg_update_phase2. Since this can take on negative values, its type
							 * is int4 (signed) and not enum gds_t_mode (which is unsigned).
							 */
	/* The following two fields aid in rolling back the transactions. 'undo_next_off' holds the
	 * original next_off in the blk buffer that would be if another nested transaction was not
	 * started. 'undo_offset' holds the offset at which 'undo_next_off' should be applied in case
	 * of an undo due to trollback.
	 * A 'kill' might change the next_off field at most in two places in the blk buffer. So, is
	 * an array of size two.
	 */

	block_offset	undo_next_off[2];
	block_offset	undo_offset[2];
	uint4		blk_checksum;
	/*blk_prior_state:the block was in global variable tree/directory tree and was free/busy before update*/
	uint4		blk_prior_state;
} cw_set_element;

#endif
