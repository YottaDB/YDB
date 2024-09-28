/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -----------------------------------
 * lke_showtree	: displays a lock tree
 * used in	: lke_show.c
 * -----------------------------------
 */

#include "mdef.h"

#include <stddef.h>
#include "gtm_signal.h"
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "mlkdef.h"
#include "cmidef.h"
#include "gtmio.h"
#include "lke.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "mlk_ops.h"

#define KDIM	64		/* max number of subscripts */

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	uint4		process_id;
GBLREF	VSIG_ATOMIC_T	util_interrupt;

error_def(ERR_CTRLC);

mlk_shrblk_ptr_t mlk_shrblk_sort(mlk_shrblk_ptr_t head, intrpt_state_t *prev_intrpt_state);

void lke_show_memory(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t bhead, char *prefix, intrpt_state_t *prev_intrpt_state)
{
	boolean_t		was_crit = FALSE;
	char			new_prefix[KDIM + 2], temp[MAX_ZWR_KEY_SZ + 1];
	mlk_prcblk_ptr_t	pending;
	mlk_shrblk_ptr_t	bckt, bnext, children, parent;
	mlk_shrsub_ptr_t	dsub;
	mlk_subhash_state_t	hs;
	mlk_subhash_res_t	hashres;
	mlk_subhash_val_t	hash;
	uint4			total_len;

	SNPRINTF(new_prefix, KDIM + 2, "	%s", prefix);
	for (bckt = bhead, bnext = 0; bnext != bhead; bckt = bnext)
	{
		dsub = (mlk_shrsub_ptr_t)R2A(bckt->value);
		memcpy(temp, dsub->data, dsub->length);
		temp[dsub->length] = '\0';
		parent = bckt->parent ? (mlk_shrblk_ptr_t)R2A(bckt->parent) : NULL;
		children = bckt->children ? (mlk_shrblk_ptr_t)R2A(bckt->children) : NULL;
		pending = bckt->pending ? (mlk_prcblk_ptr_t)R2A(bckt->pending) : NULL;
		PRINTF("%s%s : [shrblk] %p : [shrsub] %p (len=%d) : [shrhash] %x : [parent] %p : [children] %p : [pending] %p : "
				"[owner] %u : [auxowner] %" PRIuPTR "\n",
			prefix, temp, bckt, dsub, dsub->length, bckt->hash, parent, children, pending, bckt->owner, bckt->auxowner);
		MLK_SUBHASH_INIT_PVTCTL(pctl, hs);
		total_len = 0;
		mlk_shrhash_val_build(bckt, &total_len, &hs);
		MLK_SUBHASH_FINALIZE(hs, total_len, hashres);
		hash = MLK_SUBHASH_RES_VAL(hashres);
		if (hash != bckt->hash)		/* Should never happen; only here in case things get mangled. */
			PRINTF("\t\t: [computed shrhash] %x\n", hash);
		FFLUSH(stdout);
		if (bckt->children)
			lke_show_memory(pctl, (mlk_shrblk_ptr_t)R2A(bckt->children), new_prefix, prev_intrpt_state);
		if (util_interrupt)
		{
			if (LOCK_CRIT_HELD(pctl->csa))
				REL_LOCK_CRIT(pctl, was_crit);
			if (NULL != prev_intrpt_state)
				ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, *prev_intrpt_state);
			rts_error_csa(CSA_ARG(pctl->csa) VARLSTCNT(1) ERR_CTRLC);
		}
		bnext = (mlk_shrblk_ptr_t)R2A(bckt->rsib);
	}
}

void lke_show_hashtable(mlk_pvtctl_ptr_t pctl, intrpt_state_t *prev_intrpt_state)
{
	boolean_t		was_crit = FALSE;
	mlk_shrblk_ptr_t	current_shrblk;
	mlk_shrhash_map_t	usedmap;
	mlk_shrhash_ptr_t	shrhash, current_bucket;
	uint4			hash, num_buckets, si;

	shrhash = pctl->shrhash;
	num_buckets = pctl->shrhash_size;
	for (si = 0 ; si < num_buckets; si++)
	{
		current_bucket = &shrhash[si];
		usedmap = current_bucket->usedmap;
		if ((0 == current_bucket->shrblk_idx) && (0 == usedmap))
			continue;
		hash = current_bucket->hash;
		current_shrblk = MLK_SHRHASH_SHRBLK_CHECK(*pctl, current_bucket);
		PRINTF("%d\t: [shrblk] %p : [hash] %x : [usedmap] %" PRIUSEDMAP "\n", si, current_shrblk, hash, usedmap);
		FFLUSH(stdout);
		if (util_interrupt)
		{
			if (LOCK_CRIT_HELD(pctl->csa))
				REL_LOCK_CRIT(pctl, was_crit);
			if (NULL != prev_intrpt_state)
				ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, *prev_intrpt_state);
			rts_error_csa(CSA_ARG(pctl->csa) VARLSTCNT(1) ERR_CTRLC);
		}
	}
	PRINTF("\t: [num_buckets] %d\n", num_buckets);
	if (0 != pctl->ctl->hash_seed)
		PRINTF("\t: [seed] %" PRIu64 "\n", pctl->ctl->hash_seed);
	FFLUSH(stdout);
}

/* Note: *shr_sub_size keeps track of total subscript area in lock space. Initialize *shr_sub_size to 0 before calling this.
 * lke_showtree() will keep adding on previous value of shr_sub_size. If such info is not needed simply pass NULL to shr_sub_size.
 * Also, caller must check if the hash table is external. If so, make a copy and point ctl->blkhash at it.
 */
bool	lke_showtree(mlk_pvtctl_ptr_t	pctl,
		struct CLB 	*lnk,
		bool 		all,
		bool 		wait,
		pid_t 		pid,
		mstr 		one_lock,
		bool 		memory,
	        int		*shr_sub_size,
		intrpt_state_t	*prev_intrpt_state)
{
	bool			locks = FALSE;
	boolean_t		was_crit = FALSE;
	int			depth = 0, string_size = 0;
	mlk_ctldata_ptr_t	ctl;
	mlk_shrblk_ptr_t	node, start[KDIM];
	mlk_shrblk_ptr_t	tree;
	static char		name_buffer[MAX_ZWR_KEY_SZ + 1];
	static 			MSTR_DEF(name, 0, name_buffer);
	unsigned char		subscript_offset[KDIM];

	ctl = pctl->ctl;
	assert(ctl && ctl->blkroot);
	tree = (mlk_shrblk_ptr_t)R2A(ctl->blkroot);
	if (memory)
	{
		lke_show_memory(pctl, tree, "	", prev_intrpt_state);
		if (shr_sub_size)
			(*shr_sub_size) = ctl->subfree - ctl->subbase;
		lke_show_hashtable(pctl, prev_intrpt_state);
		return TRUE;
	}
	node = start[0]
	     = mlk_shrblk_sort(tree, prev_intrpt_state);
	subscript_offset[0] = 0;
	for (;;)
	{
		if (util_interrupt)
		{
			if (LOCK_CRIT_HELD(pctl->csa))
				REL_LOCK_CRIT(pctl, was_crit);
			if (NULL != prev_intrpt_state)
				ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, *prev_intrpt_state);
			rts_error_csa(CSA_ARG(pctl->csa) VARLSTCNT(1) ERR_CTRLC);
		}
		name.len = subscript_offset[depth];
		assertpro(node->value);
		string_size += MLK_SHRSUB_SIZE((mlk_shrsub_ptr_t)R2A(node->value));
		/* Display the lock node */
		locks = lke_showlock(pctl, lnk, node, &name, all, wait, TRUE, pid, one_lock, FALSE, prev_intrpt_state) || locks;
	  	/* Move to the next node */
		if (node->children == 0)
		{
			/* This node has no children, so move to the right */
			node = (mlk_shrblk_ptr_t)R2A(node->rsib);
			while (node == start[depth])
			{
				/* There are no more siblings to the right at this depth,
				   so move up and then right */
				if (node->parent == 0)
				{
					/* We're already at the top, so we're done */
					assert(depth == 0);
					if (shr_sub_size)
						(*shr_sub_size) = string_size;
					return locks;
				}
				--depth;
				node = (mlk_shrblk_ptr_t)R2A(((mlk_shrblk_ptr_t)R2A(node->parent))->rsib);
			}
		} else
		{
			/* This node has children, so move down */
			++depth;
			node = start[depth]
			     = mlk_shrblk_sort((mlk_shrblk_ptr_t)R2A(node->children), prev_intrpt_state);
			subscript_offset[depth] = name.len;
		}
	}
}
