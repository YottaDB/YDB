/****************************************************************
 *								*
 *	Copyright 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "gtm_string.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"

#include "gtcm_find_reghead.h"

GBLDEF	cm_region_head	*reglist;

cm_region_head *gtcm_find_reghead(gd_region *reg)
{
	cm_region_head  *ptr;

	for (ptr = reglist; (NULL != ptr); ptr = ptr->next)
	{
		if (ptr->reg == reg)
			return ptr;
	}
	assert(FALSE);
	return NULL;
}
