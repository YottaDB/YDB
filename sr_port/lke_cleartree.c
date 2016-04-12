/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

#include <signal.h>

#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mlk_wake_pending.h"
#include "lke.h"
#include "lke_cleartree.h"
#include "lke_clearlock.h"

#define KDIM	64		/* max number of subscripts */

GBLREF VSIG_ATOMIC_T	util_interrupt;

bool	lke_cleartree(
		      gd_region		*region,
		      struct CLB	*lnk,
		      mlk_ctldata_ptr_t	ctl,
		      mlk_shrblk_ptr_t	tree,
		      bool		all,
		      bool		interactive,
		      int4 		pid,
		      mstr		one_lock,
		      boolean_t		exact)

{
	mlk_shrblk_ptr_t	node, oldnode, start[KDIM];
	unsigned char	subscript_offset[KDIM];
	static char	name_buffer[MAX_ZWR_KEY_SZ + 1];
	static MSTR_DEF(name, 0, name_buffer);
	int		depth = 0;
	bool		locks = FALSE, locked, deleted;

	error_def(ERR_CTRLC);

	node = start[0]
	     = tree;
	subscript_offset[0] = 0;

	for (;;)
	{
		name.len = subscript_offset[depth];

		/* Display the lock node */
		locked = lke_showlock(lnk, node, &name, all, FALSE, interactive, pid, one_lock, exact);
		locks |= locked;

		/* If it was locked, clear it and wake up any processes waiting for it */
		if (locked  &&  lke_clearlock(region, lnk, ctl, node, &name, all, interactive, pid)  &&  node->pending != 0)
			mlk_wake_pending(ctl, node, region);

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
					(void)mlk_shrblk_delete_if_empty(ctl, node);
					return locks;
				}
				--depth;
				node = (mlk_shrblk_ptr_t)R2A(node->parent);
				(void)mlk_shrblk_delete_if_empty(ctl, oldnode);
				oldnode = node;
				node = (mlk_shrblk_ptr_t)R2A(node->rsib);
			}
			deleted = mlk_shrblk_delete_if_empty(ctl, oldnode);
			if (deleted  &&  start[depth] == oldnode)
				start[depth] = node;
		}
		else
		{
			/* This node has children, so move down */
			++depth;
			node = start[depth]
			     = (mlk_shrblk_ptr_t)R2A(node->children);
			subscript_offset[depth] = name.len;
		}
		if (util_interrupt)
			rts_error(VARLSTCNT(1) ERR_CTRLC);
	}
}
