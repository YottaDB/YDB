/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

GBLREF VSIG_ATOMIC_T	util_interrupt;
error_def(ERR_CTRLC);

mlk_shrblk_ptr_t mlk_shrblk_sort(mlk_shrblk_ptr_t head);

void lke_show_memory(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t bhead, char *prefix)
{
	mlk_shrblk_ptr_t	b, bnext, parent, children;
	mlk_shrsub_ptr_t	dsub;
	mlk_prcblk_ptr_t	pending;
	char			temp[ZWR_EXP_RATIO(MAX_LK_SUB_LEN) + 1];
	char			new_prefix[MAX_LKSUBSCRIPTS + 1];
	mlk_subhash_state_t	hs;
	uint4			total_len;
	mlk_subhash_res_t	hashres;
	mlk_subhash_val_t	hash;

	SNPRINTF(new_prefix, SIZEOF(new_prefix), "\t%s", prefix);
	for (b = bhead, bnext = 0; bnext != bhead; b = bnext)
	{
		dsub = (mlk_shrsub_ptr_t)R2A(b->value);
		memcpy(temp, dsub->data, dsub->length);
		temp[dsub->length] = '\0';
		parent = b->parent ? (mlk_shrblk_ptr_t)R2A(b->parent) : NULL;
		children = b->children ? (mlk_shrblk_ptr_t)R2A(b->children) : NULL;
		pending = b->pending ? (mlk_prcblk_ptr_t)R2A(b->pending) : NULL;
		PRINTF("%s%s : [shrblk] %p : [shrsub] %p (len=%d) : [shrhash] 0x%"PRIUSEDHASH" : [parent] %p : [children] %p : [pending] %p : "
				"[owner] %u : [auxowner] %"PRIuPTR"\n",
			prefix, temp, b, dsub, dsub->length, b->hash, parent, children, pending, b->owner, b->auxowner);
		MLK_SUBHASH_INIT_PVTCTL(pctl, hs);
		total_len = 0;
		mlk_shrhash_val_build(b, &total_len, &hs);
		MLK_SUBHASH_FINALIZE(hs, total_len, hashres);
		hash = MLK_SUBHASH_RES_VAL(hashres);
		if (hash != b->hash)		/* Should never happen; only here in case things get mangled. */
			PRINTF("\t\t: [computed shrhash] 0x%"PRIUSEDHASH"\n", hash);
		FFLUSH(stdout);
		if (b->children)
			lke_show_memory(pctl, (mlk_shrblk_ptr_t)R2A(b->children), new_prefix);
		bnext = (mlk_shrblk_ptr_t)R2A(b->rsib);
	}
}

void lke_show_hashtable(mlk_pvtctl_ptr_t pctl)
{
	uint4			si, num_buckets;
	mlk_subhash_val_t	hash;
	mlk_shrhash_map_t	usedmap;
	mlk_shrhash_ptr_t	shrhash, current_bucket;
	mlk_shrblk_ptr_t	current_shrblk;

	shrhash = pctl->shrhash;
	num_buckets = pctl->shrhash_size;
	for (si = 0 ; si < num_buckets; si++)
	{
		current_bucket = &shrhash[si];
		usedmap = current_bucket->usedmap;
		hash = current_bucket->hash;
		if ((0 == current_bucket->shrblk_idx) && (0 == usedmap))
			continue;
		current_shrblk = MLK_SHRHASH_SHRBLK_CHECK(*pctl, current_bucket);
		PRINTF("%d\t: [shrblk] %p : [hash] 0x%"PRIUSEDHASH" : [usedmap] %"PRIUSEDMAP"\n", si, current_shrblk, hash, usedmap);
		FFLUSH(stdout);
	}
	PRINTF("\t: [num_buckets] %d\n", num_buckets);
	if (0 != pctl->ctl->hash_seed)
		PRINTF("\t: [seed] %"PRIu64"\n", pctl->ctl->hash_seed);
	FFLUSH(stdout);
}

/* Note: *shr_sub_size keeps track of total subscript area in lock space. Initialize *shr_sub_size to 0 before calling this.
 * lke_showtree() will keep adding on previous value of shr_sub_size. If such info is not needed simply pass NULL to shr_sub_size.
 * Also, caller must check if the hash table is external. If so, make a copy and point ctl->blkhash at it.
 */
bool	lke_showtree(struct CLB 	*lnk,
		     mlk_pvtctl_ptr_t	pctl,
		     bool 		all,
		     bool 		wait,
		     pid_t 		pid,
		     mstr 		one_lock,
		     bool 		memory,
	             int		*shr_sub_size)
{
	mlk_shrblk_ptr_t	node, start[MAX_LKSUBSCRIPTS];
	mlk_shrblk_ptr_t	tree;
	mlk_ctldata_ptr_t	ctl;
	int			subscript_offset[MAX_LKSUBSCRIPTS];
	static char		name_buffer[MAX_LKNAME_LEN + 1];	/* + 1 is to store trailing null byte */
	static MSTR_DEF(name, 0, name_buffer);
	int		depth = 0;
	bool		locks = FALSE;
	int		string_size = 0;

	ctl = pctl->ctl;
	assert(ctl && ctl->blkroot);
	tree = (mlk_shrblk_ptr_t)R2A(ctl->blkroot);
	if (memory)
	{
		lke_show_memory(pctl, tree, "\t");
		if (shr_sub_size)
			(*shr_sub_size) = ctl->subfree - ctl->subbase;
		lke_show_hashtable(pctl);
		return TRUE;
	}
	node = start[0]
	     = mlk_shrblk_sort(tree);
	subscript_offset[0] = 0;

	for (;;)
	{
		name.len = subscript_offset[depth];
		assertpro(node->value);
		string_size += MLK_SHRSUB_SIZE((mlk_shrsub_ptr_t)R2A(node->value));
		/* Display the lock node */
		locks = lke_showlock(lnk, node, &name, all, wait, TRUE, pid, one_lock, FALSE)
			|| locks;

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
		}
		else
		{
			/* This node has children, so move down */
			++depth;
			node = start[depth]
			     = mlk_shrblk_sort((mlk_shrblk_ptr_t)R2A(node->children));
			subscript_offset[depth] = name.len;
		}
		if (util_interrupt)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
	}
}
