/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashdef.h"
#include "longset.h"

void ht_init(htab_desc *table, unsigned int req_ht_size)
{

	/* Geometric sequence a, a*r, a*r*r,... with a = 2.5, r = 3.65. After 78721 r = 1.825 */
	unsigned int ht_sizes[] = {1619, 5923, 21569, 78721, 143669, 262187, 478483, 873247, 1593653, 2908421, 0};
	unsigned int 	cur_ht_size;
	int 		index;

	error_def(ERR_HTOFLOW);

	for (index = 0, cur_ht_size = ht_sizes[index]; 0 != cur_ht_size && cur_ht_size < req_ht_size; index++)
 		cur_ht_size = ht_sizes[index];
	if (cur_ht_size)
	{
		table->base = (ht_entry *)malloc(cur_ht_size * sizeof(ht_entry));
		longset((uchar_ptr_t)table->base, cur_ht_size * sizeof(ht_entry), 0);
		table->r = table->base + (table->size = cur_ht_size);
		/* Please add appropriate trigger factor to calculate trigger_size.
		   Now it will trigger hashtable growth for half of current table size
		 */
		table->trigger_size = cur_ht_size >> 1;
		table->count = 0;
	} else if (table->count < ht_sizes[index - 1])
	{
		/*
		Already reached maximum size of a hash table we allow, though it has space.
		So reset trigger_size to be maximum table size.
		Note:
			1. We assume that table will grow according to the prime table in ht_sizes.
			2. We assume that first call to ht_init() will not request size >= maximum.
			   Otherwise hashtable might fail completely.
		*/
		table->trigger_size =  ht_sizes[index - 1];
	} else
	{
		table->r++;
 		rts_error(VARLSTCNT(1) ERR_HTOFLOW);
	}

	return;
}
