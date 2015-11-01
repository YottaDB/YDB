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
DEBUG_ONLY(GBLREF mur_opt_struct	mur_options;)


jnl_tm_t mur_process_token_table(boolean_t *ztp_broken)
{
	boolean_t 	ztp_brkn;
	jnl_tm_t	min_broken_time;
	int		ht_index;
	multi_struct	*multi;
	ht_entry	*table_base;

	assert(!mur_options.rollback);
	mur_multi_rehash();	/* To release memory and shorten the table */
	table_base = murgbl.token_table.base;
	ztp_brkn = FALSE;
	min_broken_time = MAXUINT4;
	for (ht_index = 0; ht_index < murgbl.token_table.size; ht_index++)
	{
		multi = (multi_struct *)table_base[ht_index].ptr;
		while (NULL != multi)
		{
			if (0 < multi->partner)
			{
				if (min_broken_time > multi->time)
					min_broken_time = multi->time;
				ztp_brkn = ztp_brkn || (ZTPFENCE == multi->fence);
			}
			multi = (multi_struct *)multi->next;
		}
	}
	*ztp_broken = ztp_brkn;
	return min_broken_time;
}
