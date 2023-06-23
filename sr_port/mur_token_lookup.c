/****************************************************************
 *								*
 * Copyright (c) 2003-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF	mur_opt_struct	mur_options;

multi_struct *mur_token_lookup(token_num token, off_jnl_t rec_time, enum rec_fence_type fence)
{
	ht_ent_int8	*tabent;
	multi_struct 	*multi;

	if (NULL != (tabent = lookup_hashtab_int8(&murgbl.token_table, (gtm_uint64_t *)&token)))
	{
		if (mur_options.rollback)
		{
			assert(NULL != ((multi_struct *)tabent->value));
			assert(NULL == ((multi_struct *)tabent->value)->next);
			return (multi_struct *)tabent->value;	/* The way it is generated that always unique */
		}
		for (multi = (multi_struct *)tabent->value; NULL != multi; multi = (multi_struct *)multi->next)
		{
			if ((ZTPFENCE == fence) || (multi->time == rec_time))
				return multi;
		}
	}
	return NULL;
}
