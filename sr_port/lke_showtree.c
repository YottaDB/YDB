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

/*
 * -----------------------------------
 * lke_showtree	: displays a lock tree
 * used in	: lke_show.c
 * -----------------------------------
 */

#include "mdef.h"

#include <signal.h>
#include <stddef.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "mlkdef.h"
#include "cmidef.h"
#include "lke.h"

#define KDIM	64		/* max number of subscripts */

GBLREF VSIG_ATOMIC_T	util_interrupt;
error_def(ERR_CTRLC);

void lke_show_memory(mlk_shrblk_ptr_t bhead, char *prefix)
{
	mlk_shrblk_ptr_t	b, bnext;
	mlk_shrsub_ptr_t	dsub;
	char			temp[MAX_ZWR_KEY_SZ + 1];
	char			new_prefix[KDIM+2];

	SPRINTF(new_prefix, "	%s", prefix);
	for (b = bhead, bnext = 0; bnext != bhead; b = bnext)
	{
		dsub = (mlk_shrsub_ptr_t)R2A(b->value);
		memcpy(temp, dsub->data, dsub->length);
		temp[dsub->length] = '\0';
		PRINTF("%s%s : [shrblk] %lx : [shrsub] %lx\n", prefix, temp, (long unsigned int) b, (long unsigned int) dsub);
		if (b->children)
			lke_show_memory((mlk_shrblk_ptr_t)R2A(b->children), new_prefix);
		bnext = (mlk_shrblk_ptr_t)R2A(b->rsib);
	}
}

/* Note:*shr_sub_size keeps track of total subscript area in lock space. Initialize *shr_sub_size to 0 before calling this.
 * lke_showtree() will keep adding on previous value of shr_sub_size. If such info is not needed simply pass NULL to shr_sub_size
 */
bool	lke_showtree(struct CLB 	*lnk,
		     mlk_shrblk_ptr_t	tree,
		     bool 		all,
		     bool 		wait,
		     pid_t 		pid,
		     mstr 		one_lock,
		     bool 		memory,
	             int		*shr_sub_size)
{
	mlk_shrblk_ptr_t	node, start[KDIM];
	unsigned char	subscript_offset[KDIM];
	static char	name_buffer[MAX_ZWR_KEY_SZ + 1];
	static MSTR_DEF(name, 0, name_buffer);
	int		depth = 0;
	bool		locks = FALSE;
	int		string_size = 0;

	if (memory)
	{
		lke_show_memory(tree, "	");
		if (shr_sub_size)
			(*shr_sub_size) = string_size;
		return TRUE;
	}
	node = start[0]
	     = tree;
	subscript_offset[0] = 0;

	for (;;)
	{
		name.len = subscript_offset[depth];
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
			     = (mlk_shrblk_ptr_t)R2A(node->children);
			subscript_offset[depth] = name.len;
		}
		if (util_interrupt)
			rts_error(VARLSTCNT(1) ERR_CTRLC);
	}
}
