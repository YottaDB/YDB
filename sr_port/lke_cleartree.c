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
 * ----------------------------------
 * lke_cleartree : clears a lock tree
 * used in	 : lke_clear.c
 * ----------------------------------
 */

#include "mdef.h"

#include "gtm_signal.h"

#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mlk_wake_pending.h"
#include "lke.h"
#include "lke_cleartree.h"
#include "lke_clearlock.h"
#include "mlk_ops.h"

#define KDIM	64		/* max number of subscripts */

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	uint4		process_id;
GBLREF VSIG_ATOMIC_T	util_interrupt;

error_def(ERR_CTRLC);

mlk_shrblk_ptr_t mlk_shrblk_sort(mlk_shrblk_ptr_t head);

bool	lke_cleartree(
		      mlk_pvtctl_ptr_t	pctl,
		      struct CLB	*lnk,
		      mlk_shrblk_ptr_t	tree,
		      bool		all,
		      bool		interactive,
		      int4 		pid,
		      mstr		one_lock,
		      boolean_t		exact,
		      intrpt_state_t	*prev_intrpt_state)

{
	bool			deleted, locked, locks = FALSE;
	boolean_t		was_crit = FALSE;
	int			depth = 0;
	mlk_shrblk_ptr_t	node, oldnode, start[KDIM];
	static char		name_buffer[MAX_ZWR_KEY_SZ + 1];
	static			MSTR_DEF(name, 0, name_buffer);
	unsigned char		subscript_offset[KDIM];

	node = start[0]
	     = mlk_shrblk_sort(tree);
	subscript_offset[0] = 0;
	for (;;)
	{
		name.len = subscript_offset[depth];
		/* Display the lock node */
		locked = lke_showlock(pctl, lnk, node, &name, all, FALSE, interactive, pid, one_lock, exact, prev_intrpt_state);
		locks |= locked;

		/* If it was locked, clear it and wake up any processes waiting for it */
		if (locked && lke_clearlock(pctl, lnk, node, &name, all, interactive, pid) && (0 != node->pending))
			mlk_wake_pending(pctl, node);

		/* if a specific lock was requested (-EXACT and -LOCK=), then we are done */
		if (exact && (0 != one_lock.len) && locked)
			return locks;
		/* Move to the next node */
		if (node->children == 0)
		{
			/* This node has no children, so move to the right */
			oldnode = node;
			node = (mlk_shrblk_ptr_t)R2A(node->rsib);
			while (node == start[depth])
			{
				/* There are no more siblings to the right at this depth,
				   so move up and then right */
				if (node->parent == 0)
				{
					/* We're already at the top, so we're done */
					assert(depth == 0);
					(void)mlk_shrblk_delete_if_empty(pctl, node);
					return locks;
				}
				--depth;
				node = (mlk_shrblk_ptr_t)R2A(node->parent);
				(void)mlk_shrblk_delete_if_empty(pctl, oldnode);
				oldnode = node;
				node = (mlk_shrblk_ptr_t)R2A(node->rsib);
			}
			deleted = mlk_shrblk_delete_if_empty(pctl, oldnode);
			if (deleted  &&  start[depth] == oldnode)
				start[depth] = node;
		}
		else
		{
			/* This node has children, so move down */
			++depth;
			node = start[depth]
			     = mlk_shrblk_sort((mlk_shrblk_ptr_t)R2A(node->children));
			subscript_offset[depth] = name.len;
		}
		ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, *prev_intrpt_state);
		if (util_interrupt)
		{
			assert(LOCK_CRIT_HELD(pctl->csa));
			REL_LOCK_CRIT(pctl, was_crit);
			rts_error_csa(CSA_ARG(pctl->csa) VARLSTCNT(1) ERR_CTRLC);
		}
		DEFER_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, *prev_intrpt_state);
	}
}
