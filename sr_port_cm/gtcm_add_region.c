/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_string.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_add_region.h"
#include "relqueopi.h"

GBLREF	node_local_ptr_t	locknl;

void gtcm_add_region(connection_struct *cnx,cm_region_head *rh)
{
	cm_region_list *ptr, *list_head;

	for (ptr = cnx->region_root; ptr ; ptr = ptr->next)
		if (ptr->reghead == rh)
			break;

	if (!ptr)
	{
		ptr = (cm_region_list *)malloc(SIZEOF(*ptr));
		memset(ptr,0,SIZEOF(*ptr));
		ptr->regnum = cnx->maxregnum++;
		assert(!cnx->region_array[ptr->regnum]);
		cnx->region_array[ptr->regnum] = ptr;
		ptr->reghead = rh;
		ptr->cs = cnx;
		DEBUG_ONLY(locknl = FILE_INFO(rh->reg)->s_addrs.nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		INSQTI(ptr,rh);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		rh->refcnt++;
		if (cnx->region_root == 0)
			cnx->region_root = ptr;
		else
		{
			list_head = cnx->region_root;
			cnx->region_root = ptr;
			ptr->next = list_head;
		}
	}
	cnx->current_region = ptr;
}
