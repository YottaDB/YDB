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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "gtm_string.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcm_add_region.h"
#include "relqueopi.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;

void gtcm_add_region(connection_struct *cnx,cm_region_head *rh)
{
	cm_region_list *ptr, *list_head;
	DEBUG_ONLY(gd_region	*r_save;)

	for (ptr = cnx->region_root; ptr ; ptr = ptr->next)
		if (ptr->reghead == rh)
			break;

	if (!ptr)
	{
		ptr = (cm_region_list *)malloc(sizeof(*ptr));
		memset(ptr,0,sizeof(*ptr));
		ptr->regnum = cnx->maxregnum++;
		ptr->reghead = rh;
		ptr->cs = cnx;
		DEBUG_ONLY(r_save = gv_cur_region; TP_CHANGE_REG(rh->reg)); /* for LOCK_HIST macro */
		INSQTI(ptr,rh);
		DEBUG_ONLY(TP_CHANGE_REG(r_save));	/* restore gv_cur_region */
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
