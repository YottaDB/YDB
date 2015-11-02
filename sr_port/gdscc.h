/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

#define	GDS_WRITE_PLAIN		0
#define	GDS_WRITE_KILL		1
#define	GDS_WRITE_BLOCK_SPLIT	2

/* macro to traverse to the end of an horizontal cw_set_element list */

#define TRAVERSE_TO_LATEST_CSE(x)					\
{									\
	assert(dollar_tlevel);						\
	if (x)								\
                for ( ; (x)->high_tlevel; x = (x)->high_tlevel)		\
                        ;						\
}

typedef uint4	block_offset;
typedef int4   	block_index;

enum gds_t_mode
{
	gds_t_noop = 0,		/* there is code that initializes stuff to 0 relying on it being equal to gds_t_noop */
	gds_t_create,
	gds_t_write,
	gds_t_write_root,
	gds_t_acquired,
	gds_t_committed,
	gds_t_writemap,
	n_gds_t_op,		/* tp_tend() depends on this order of placement of n_gds_t_op */
	kill_t_create,
	kill_t_write
};

typedef struct key_value_struct
{
	gv_key			key;			/* note that the following array holds the actual key contents */
	char			key_contents[ROUND_UP(MAX_KEY_SZ + MAX_NUM_SUBSC_LEN, 4)];
	mstr			value;
	struct key_value_struct	*next;
} key_cum_value;

/* Create/write set element. This is used to describe modification of a database block */

typedef struct cw_set_element_struct
{
	que_ent		free_que;			/* should be the first member in the structure (for buddy_list) */
							/* needed to use free_element() of the buddy list */
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
	uint4		write_type;			/* can be either 0 or GDS_WRITE_KILL or GDS_WRITE_BLOCK_SPLIT
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
	int4		t_level;			/* transaction level associated with cw element, for incremental rollback */
	enum db_ver	ondsk_blkver;			/* Actual block version from block header as it exists on disk.
							 * If "cse->mode" is gds_t_write_root, this is uninitialized.
							 * If "cse->mode" is gds_t_create/gds_t_acquired, this is GDSVCURR.
							 * Otherwise, this is set to cr->ondsk_blkver (cr is got from the history).
							 * Whenever "cse->old_block" is reset, this needs to be reset too (except
							 *	in the case of gds_t_create/gds_t_acquired).
							 */
	enum gds_t_mode	old_mode;			/* saved copy of "cse->mode" before being reset to gds_t_committed */

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
} cw_set_element;

block_id	bm_getfree(block_id orig_hint, bool *blk_used, unsigned int cw_work, cw_set_element *cs, int *cw_depth_ptr);

#endif
