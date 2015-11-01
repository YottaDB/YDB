/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "locklits.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "mlk_pvtblk_insert.h"
#include "mlk_pvtblk_equ.h"

GBLREF mlk_pvtblk *mlk_pvt_root;
GBLREF short lks_this_cmd;
GBLREF bool remlkreq;

int mlk_pvtblk_insert(mlk_pvtblk *pblk)
{
	bool new;
	mlk_pvtblk *temp, *inlist1, *inlist2, *save_store;

	if (pblk->region->dyn.addr->acc_meth == dba_cm)
	{
		save_store = mlk_pvt_root;
		mlk_pvt_root = ((link_info *)pblk->region->dyn.addr->cm_blk->usr)->netlocks;
	}

	if (!mlk_pvt_root)
	{
		mlk_pvt_root = pblk;
		mlk_pvt_root->translev = 1;
		mlk_pvt_root->trans = TRUE;
		new = TRUE;
	}
	else
	{
		inlist1 = inlist2 = mlk_pvt_root;
		while (inlist1 && !mlk_pvtblk_equ(pblk,inlist1))
		{
			inlist2 = inlist1;
			inlist1 = inlist1->next;
		}
		if (inlist1)
		{
			new = FALSE;
			if (inlist1->trans)
			{	inlist1->translev++;
			}
			else
			{	inlist1->translev = 1;
				inlist1->trans = TRUE;
				inlist1->old = TRUE;
			}
			inlist2->next = inlist1->next;
			if (inlist1 != mlk_pvt_root)
			{
				inlist1->next = mlk_pvt_root;
				mlk_pvt_root = inlist1;
			}
		}
		else
		{
			pblk->translev = 1;
			pblk->trans = 1;
			pblk->next = mlk_pvt_root;
			mlk_pvt_root = pblk;
			new = TRUE;
		}
	}
	if (pblk->region->dyn.addr->acc_meth == dba_cm)
	{
		((link_info *)pblk->region->dyn.addr->cm_blk->usr)->netlocks = mlk_pvt_root;
		((link_info *)pblk->region->dyn.addr->cm_blk->usr)->lck_info |= REQUEST_PENDING;
		mlk_pvt_root = save_store;
		remlkreq = TRUE;
	}
	else
	{
		if (mlk_pvt_root->translev == 1)
			lks_this_cmd++;
	}
	return (new);
}
