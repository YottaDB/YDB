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
#include "mlkdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmlkdef.h"

GBLREF cm_lckblkreg		*blkdlist;
GBLREF connection_struct	*curr_entry;
GBLREF int			errno;

void gtcml_blklck(cm_region_list *region, mlk_pvtblk *lock, uint4 wake)
{
	cm_lckblkreg	*b, *b1;
	cm_lckblklck	*l, *l1, *l2, *tl1;
	cm_lckblkprc	*p, *p1, *p2, *pl1;
	uint4		status;
	boolean_t	new;
	error_def(CMERR_CMSYSSRV);

	region->blkd = lock;
	if (curr_entry->state != CMMS_L_LKACQUIRE)
		return;
	new = FALSE;
	if (!region->reghead->wakeup)
		region->reghead->wakeup = wake;
	for (b1 = b = blkdlist; b; b1 = b, b = b->next)
		if (b->region == region->reghead)
			break;
	if (!b)
	{	new = TRUE;
		b = (cm_lckblkreg *)malloc (sizeof(cm_lckblkreg));
		b->next = 0;
		b->region = region->reghead;
		b->pass = CM_BLKPASS;
		b->lock	 = (cm_lckblklck *)malloc (sizeof(cm_lckblklck));
		l = (cm_lckblklck *)malloc (sizeof(cm_lckblklck));
		l->node = lock->nodptr;
		l->next = l->last = b->lock;
		l->seq = lock->sequence;
		if (time((time_t *)&l->blktime[0]) == -1)
			rts_error(VARLSTCNT(3) CMERR_CMSYSSRV,0,errno);
		p = (cm_lckblkprc *)malloc(sizeof(cm_lckblkprc));
		p->user = curr_entry;
		p->blocked = lock->blocked;
		p->next = p->last = p;
		l->prc = p;
		b->lock->next = b->lock->last = l;
	}
	if (!blkdlist)
	{
		blkdlist = b;
		return;
	}
	if (b1 != b)
		b1->next = b;
	if (!new)
	{
		for (l1 = b->lock->next, l2 = l1->next; l2 != b->lock;l1 = l1->next, l2 = l1->next)
			if (l1->node == lock->nodptr)
				break;
		if (l1->node != lock->nodptr)
		{
			tl1 = (cm_lckblklck *)malloc (sizeof(cm_lckblklck));
			tl1->node = lock->nodptr;
			tl1->next = l1->next;
			tl1->last = l1;
			l1->next->last = tl1;
			l1->next = tl1;
			tl1->prc = 0;
			/* get blking time */
			if (time((time_t *)&tl1->blktime[0]) == -1)
				rts_error(VARLSTCNT(3) CMERR_CMSYSSRV,0,errno);
			l1 = tl1;
		}
		l1->seq = lock->sequence;
		p1 = l1->prc;
		if (p1)
			for (p2 = p1->next; p2 != l1->prc;p1 = p1->next, p2 = p1->next)
				if (p1->user == curr_entry)
					break;
		if (!p1 || p1->user != curr_entry)
		{
			pl1 = (cm_lckblkprc *)malloc(sizeof(cm_lckblkprc));
			if (!p1)
			{
				l1->prc = pl1;
				pl1->next = pl1->last = pl1;
			} else
			{
				pl1->next = p1->next;
				pl1->last = p1;
				p1->next->last = pl1;
				p1->next = pl1;
			}
			pl1->user = curr_entry;
			pl1->blocked = lock->blocked;
		}
	}
}
