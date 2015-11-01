/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"

GBLREF mur_gbls_t	murgbl;
#define MUR_ENOUGH_COMPLETE_TRANS(MURGBL) ((MURGBL).broken_cnt <= ((MURGBL).token_table.count >> 2)) /* 75% resolved completely */

CONDITION_HANDLER(mur_multi_rehash_ch)
{
	/* If we cannot alloc memory during rehasing, just continue in normal program flow */
	error_def(ERR_MEMORY);
	error_def(ERR_VMSMEMORY);
	error_def(ERR_MEMORYRECURSIVE);
	START_CH;
	/* If we cannot allocate memory or any error while doing rehash, just abort any more rehashing.
	 *  We will continue with old table */
	if (ERR_MEMORY == SIGNAL || ERR_VMSMEMORY == SIGNAL || ERR_MEMORYRECURSIVE == SIGNAL)
	{
		UNWIND(NULL, NULL);
	}
	else
	{
		NEXTCH; /* non memory related error */
	}
}

void mur_multi_rehash(void)
{
	int		ht_index;
	multi_struct	*multi, *next_multi;
	htab_desc	temp_table;
	ht_entry	*htentry, *table_base;
	char		new;
	DEBUG_ONLY(int	brkn_cnt;)

	ESTABLISH(mur_multi_rehash_ch);
	table_base = murgbl.token_table.base;
	/* see if re-hashing of broken entries can reduce the size of the hash-table leading to faster processing times */
	if (MUR_ENOUGH_COMPLETE_TRANS(murgbl))	/* are enough entries fully resolved to justify cost of re-hashing? */
	{ 	/* re-hash broken entries */
		DEBUG_ONLY(brkn_cnt = 0;)
		ht_init(&temp_table, murgbl.broken_cnt);	/* enough size to accommodate broken transactions */
		for (ht_index = 0; ht_index < murgbl.token_table.size; ht_index++)
		{
			multi = (multi_struct *)table_base[ht_index].ptr;
			for (next_multi = multi; NULL != next_multi; multi = next_multi)
			{
				next_multi = (multi_struct *)multi->next;
				if (0 < multi->partner) /* re-hash only broken transactions */
				{
					htentry = ht_put(&temp_table, (mname *)&multi->token, &new);
					assert(new || NULL != htentry->ptr);
					multi->next = (!new ? (multi_struct *)htentry->ptr : NULL);
					htentry->ptr = (char *)multi;
					DEBUG_ONLY(brkn_cnt++;)
				}
			}
		}
		ht_free(&murgbl.token_table);
		murgbl.token_table = temp_table;
		assert(brkn_cnt ==  murgbl.broken_cnt);
	}
	REVERT;
	return;
}
