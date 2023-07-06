/****************************************************************
 *								*
 * Copyright (c) 2019-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "gtm_string.h"

#include "mdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "mlk_ops.h"
#include "mlk_rehash.h"
#include "mlk_shrhash_add.h"

GBLREF	uint4	process_id;

error_def(ERR_MLKREHASH);

STATICFNDCL int mlk_rehash_tree(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t head, mlk_subhash_state_t parent_state, uint4 parent_len);

STATICFNDEF int mlk_rehash_tree(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t head, mlk_subhash_state_t parent_state, uint4 parent_len)
{
	int			count = 0, child_count;
	mlk_shrblk_ptr_t	node;
	mlk_shrsub_ptr_t	node_sub;
	mlk_subhash_state_t	node_state;
	mlk_subhash_res_t	node_res;
	uint4			node_len;

	node = head;
	do {
		node_state = parent_state;
		node_sub = (mlk_shrsub_ptr_t)R2A(node->value);
		MLK_SUBHASH_INGEST(node_state, &node_sub->length, SIZEOF(node_sub->length));
		node_len = SIZEOF(node_sub->length);
		MLK_SUBHASH_INGEST(node_state, node_sub->data, node_sub->length);
		node_len += node_sub->length;
		MLK_SUBHASH_FINALIZE(node_state, parent_len + node_len, node_res);
		node->hash = MLK_SUBHASH_RES_VAL(node_res);
		if (!mlk_shrhash_add(pctl, node))
			return -1;
		count++;
		if (0 != node->children)
		{
			child_count = mlk_rehash_tree(pctl, (mlk_shrblk_ptr_t)R2A(node->children),
						      node_state, parent_len + node_len);
			if (0 > child_count)
				return -1;
			count += child_count;
		}
		assert(0 != node->rsib);
		node = (mlk_shrblk_ptr_t)R2A(node->rsib);
	} while (node != head);
	return count;
}

void mlk_rehash(mlk_pvtctl_ptr_t pctl)
{
	int			count = -1;
	mlk_shrblk_ptr_t	root;
	mlk_subhash_state_t	hs;

	assert(LOCK_CRIT_HELD(pctl->csa));
	root = (0 == pctl->ctl->blkroot) ? NULL : (mlk_shrblk_ptr_t)R2A(pctl->ctl->blkroot);
	do {
		memset(pctl->shrhash, 0, pctl->shrhash_size * SIZEOF(mlk_shrhash));
		pctl->ctl->hash_seed++;
		if (NULL != root)
		{
			MLK_SUBHASH_INIT_PVTCTL(pctl, hs);
			count = mlk_rehash_tree(pctl, root, hs, 0);
		} else
		{
			count = 0;		/* Initialize if no blocks to rehash */
			break;
		}
	} while (count < 0);
	assert((pctl->ctl->max_blkcnt - pctl->ctl->blkcnt) == count);
	pctl->ctl->gc_needed = FALSE;
	pctl->ctl->resize_needed = FALSE;
	pctl->ctl->rehash_needed = FALSE;
	send_msg_csa(CSA_ARG(pctl->csa) VARLSTCNT(5) ERR_MLKREHASH, 3, REG_LEN_STR(pctl->region), pctl->ctl->hash_seed);
}
