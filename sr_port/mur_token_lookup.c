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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF	mur_opt_struct	mur_options;

#if defined(UNIX)
multi_struct *mur_token_lookup(token_num token, uint4 pid, off_jnl_t rec_time, enum rec_fence_type fence)
#elif defined(VMS)
multi_struct *mur_token_lookup(token_num token, uint4 pid, int4 image_count, off_jnl_t rec_time, enum rec_fence_type fence)
#endif
{
	ht_entry	*hentry;
	multi_struct 	*multi;

	if (NULL != (hentry = ht_get(&murgbl.token_table, (mname *)&token)))
	{
		if (mur_options.rollback)
		{
			assert(NULL != ((multi_struct *)hentry->ptr));
			assert(NULL == ((multi_struct *)hentry->ptr)->next);
			return (multi_struct *)hentry->ptr;	/* The way it is generated that always unique */
		}
		for (multi = (multi_struct *)hentry->ptr; NULL != multi; multi = (multi_struct *)multi->next)
		{
			if (multi->pid == pid && VMS_ONLY(multi->image_count == image_count &&)
					(ZTPFENCE == fence || multi->time == rec_time))
				return multi;
		}
	}
	return NULL;
}
