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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

GBLREF mur_gbls_t	murgbl;
DEBUG_ONLY(GBLREF mur_opt_struct	mur_options;)


jnl_tm_t mur_process_token_table(boolean_t *ztp_broken)
{
	boolean_t 	ztp_brkn;
	jnl_tm_t	min_broken_time;
	multi_struct	*multi;
	ht_ent_int8	*curent, *topent;

	assert(!mur_options.rollback);
	mur_multi_rehash();	/* To release memory and shorten the table */
	ztp_brkn = FALSE;
	min_broken_time = MAXUINT4;
	for (curent = murgbl.token_table.base, topent = murgbl.token_table.top; curent < topent; curent++)
	{
		if (HTENT_VALID_INT8(curent, multi_struct, multi))
		{
			do
			{
				if (0 < multi->partner)
				{
					if (min_broken_time > multi->time)
						min_broken_time = multi->time;
					ztp_brkn = ztp_brkn || (ZTPFENCE == multi->fence);
				}
				multi = (multi_struct *)multi->next;
			} while (NULL != multi);
		}
	}
	*ztp_broken = ztp_brkn;
	return min_broken_time;
}
