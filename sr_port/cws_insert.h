/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __CWS_INSERT_H__
#define __CWS_INSERT_H__

GBLREF	hash_table_int4	cw_stagnate;
GBLREF	boolean_t	cw_stagnate_reinitialized;

/* Usually a process does not need the cw_stagnate hash table as it is used only in the final retry.
 * But CWS_RESET (to ensure the hashtable is reinitialized) is done in a lot of places and almost every transaction.
 * To avoid unnecessary overhead in the reset, we start from a small initial value for CWS_INITIAL_SIZE.
 * When necessary hash rounties will expand automatically.
 */
#define CWS_INITIAL_SIZE        4

/* Initialize the cw_stagnate hash-table */
#define	CWS_INIT												\
{														\
	if (0 == cw_stagnate.size)										\
	{													\
		init_hashtab_int4(&cw_stagnate, CWS_INITIAL_SIZE, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);	\
		cw_stagnate_reinitialized = TRUE;               						\
	}													\
}

/* macro CWS_INSERT appends a block_id onto an hashtable of block_id's
 *	cw_stagnate		is the address of the hashtable
 *				it is initially NULL
 *	CWS_INITIAL_SIZE	is the initial size of the hash table
 *				table is enlarged each time it fills up
 * The hashtable is allocated the first time this routine is* called, and the contents of it are reset by calling cws_reset
 * in t_begin, t_begin_crit and tp_hist. The hashtable expands by itself.
 */

GBLREF	boolean_t	mu_reorg_in_swap_blk;
GBLREF	int4		cws_reorg_remove_index;
GBLREF	block_id	cws_reorg_remove_array[];

#define CWS_INSERT(block)									\
{												\
	ht_ent_int4		*dummy;								\
	boolean_t		new_entry;							\
												\
	cw_stagnate_reinitialized = FALSE;							\
	new_entry = add_hashtab_int4(&cw_stagnate, (uint4 *)(&block), HT_VALUE_DUMMY, &dummy);	\
	/* If in mu_swap_blk and a new entry was added to the hashtable, note this block number	\
	 * in the cws_reorg_remove_array for later removal in the next iteration of the for 	\
	 * loop in "mu_swap_blk" (see mu_swap_blk.c for more detail).				\
	 */											\
	if (mu_reorg_in_swap_blk && new_entry)							\
	{											\
		CWS_REORG_INSERT(block);							\
	}											\
}

#define	CWS_REORG_REMOVE_ARRAYSIZE	((4 * MAX_BT_DEPTH) + 2)	/* see mu_swap_blk.c for detail on this calculation */
#define	CWS_REORG_REMOVE_MAXINDEX	(CWS_REORG_REMOVE_ARRAYSIZE - 1)

#define	CWS_REORG_INSERT(blk)						\
{									\
	assert(cws_reorg_remove_index < CWS_REORG_REMOVE_MAXINDEX);	\
	cws_reorg_remove_array[++cws_reorg_remove_index] = (blk);	\
}

/* the use of the variable cw_stagnate_reinitialized to optimize CWS_RESET assumes that all calls to
 * add_hashtab_int4() are done through CWS_INSERT macro.
 */
#define CWS_RESET											\
{	/* if a transaction did not use cw_stagnate hash table, there is no need to reset it */		\
	if (!cw_stagnate_reinitialized)									\
	{												\
		reinitialize_hashtab_int4(&cw_stagnate);						\
		cw_stagnate_reinitialized = TRUE;							\
	}												\
}
#endif
