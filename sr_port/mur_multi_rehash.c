/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
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
#include "hashtab_int4.h"
#include "hashtab_int8.h"
#include "hashtab_mname.h"
#include "buddy_list.h"
#include "muprec.h"

GBLREF mur_gbls_t	murgbl;
#define MUR_ENOUGH_COMPLETE_TRANS(MURGBL) ((MURGBL).broken_cnt <= ((MURGBL).token_table.count >> 2)) /* 75% resolved completely */

void mur_multi_rehash(void)
{
	multi_struct	*multi, *next_multi;
	hash_table_int8	temp_table;
	ht_ent_int8	*newent, *curent, *topent;
	DEBUG_ONLY(int	brkn_cnt;)

	/* see if re-hashing of broken entries can reduce the size of the hash-table leading to faster processing times */
	if (MUR_ENOUGH_COMPLETE_TRANS(murgbl))	/* are enough entries fully resolved to justify cost of re-hashing? */
	{ 	/* re-hash broken entries */
		DEBUG_ONLY(brkn_cnt = 0;)
		ESTABLISH(hashtab_rehash_ch);
		/* enough size to acommodate broken transactions */
		init_hashtab_int8(&temp_table, murgbl.broken_cnt * 100.0 / HT_LOAD_FACTOR, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
		REVERT;
		for (curent = murgbl.token_table.base, topent = murgbl.token_table.top; curent < topent; curent++)
		{
			if (HTENT_VALID_INT8(curent, multi_struct, multi))
			{
				for (next_multi = multi; NULL != next_multi; multi = next_multi)
				{
					next_multi = (multi_struct *)multi->next;
					if (0 < multi->partner) /* re-hash only broken transactions */
					{
						multi->next = NULL;
						if (!add_hashtab_int8(&temp_table, &multi->token, multi, &newent))
						{
							assert(NULL != newent->value);
							multi->next = (multi_struct *)newent->value;
							newent->value = (char *)multi;
						}
						DEBUG_ONLY(brkn_cnt++;)
					}
				}
			}
		}
		free_hashtab_int8(&murgbl.token_table);
		murgbl.token_table = temp_table;
		assert(brkn_cnt ==  murgbl.broken_cnt);
	}
	return;
}
