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
#include "cmidef.h"
#include "cmmdef.h"

void gtcm_add_region(cnx,rh)
connection_struct *cnx;
cm_region_head *rh;
{
	cm_region_list *ptr, *list_head;

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
		insqti(ptr,rh);
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
