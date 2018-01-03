/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "mlkdef.h"
#include "mmemory.h"
#include "is_proc_alive.h"
#include "mlk_shrblk_find.h"
#include "mlk_shrblk_create.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlk_wake_pending.h"

GBLREF uint4 process_id;

boolean_t	mlk_find_blocking_child_lock(mlk_pvtblk *p, mlk_shrblk_ptr_t child, UINTPTR_T auxown);

boolean_t	mlk_find_blocking_child_lock(mlk_pvtblk *p, mlk_shrblk_ptr_t child, UINTPTR_T auxown)
{
	mlk_shrblk_ptr_t	d, dhead, d1;
	boolean_t		blocked;

	blocked = FALSE;
	for (dhead = d = child, d1 = NULL ; dhead != d1 && !blocked ; d = d1 = (mlk_shrblk_ptr_t)R2A(d->rsib))
	{
		/* There is similar code below to check if the process that owns the lock still exists.
		 * Any change in this part of the code should be propogated to those segments too.
		 */
		if (d->owner && (d->owner != process_id || d->auxowner != auxown))
		{	/* If owned and not owned by us check if owner is alive */
			if (is_proc_alive(d->owner, IMAGECNT(d->image_count)))
			{	/* Signal that this lock request is blocked by this node */
				p->blocked = d;
				p->blk_sequence = d->sequence;
				blocked = TRUE;
			} else
			{	/* Owner is dead so release this node */
				d->owner = 0;
				d->auxowner = 0;
			}
		}
		if (!blocked && d->children)
			blocked = mlk_find_blocking_child_lock(p, (mlk_shrblk_ptr_t)R2A(d->children), auxown);
	}
	return blocked;
}

mlk_shrblk_ptr_t mlk_shrhash_find(mlk_pvtblk *p, int subnum, unsigned char *subval, unsigned char sublen, mlk_shrblk_ptr_t parent);

boolean_t	mlk_shrblk_find(mlk_pvtblk *p, mlk_shrblk_ptr_t *ret, UINTPTR_T auxown)
{
	boolean_t		blocked;
	int			i, j, slen;
	mlk_ctldata_ptr_t	ctl;
	mlk_prcblk_ptr_t	pr;
	mlk_shrblk_ptr_t	pnt, d, d0, d1, dhead;
	mlk_shrsub_ptr_t	dsub;
	ptroff_t		*chld_of_pnt;
	unsigned char		*cp;
	uint4			yield_pid;
#	ifdef DEBUG
	mlk_shrblk_ptr_t	dh;
#	endif
	DCL_THREADGBL_ACCESS;

	blocked = FALSE;
	/* Note: If ever this function returns with "blocked" set to "TRUE",
	 * make sure TREF(mlk_yield_pid) is initialized appropriately.
	 */
	*ret = 0;
	SETUP_THREADGBL_ACCESS;
	for (pnt = NULL , chld_of_pnt = (ptroff_t *)&p->ctlptr->blkroot , i = p->subscript_cnt , cp = p->value ;
		i > 0 ; i-- , pnt = d , chld_of_pnt = (ptroff_t *)&d->children, cp += slen)
	{
		slen = *cp++;
		if (!*chld_of_pnt)
		{
			assert(NULL == mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt));
			if (!(d = mlk_shrblk_create(p, cp, slen, pnt, chld_of_pnt, i)))
			{
				assert(NULL == mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt));
				return TRUE;
			}
			assert((dh = mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt)) == d);
			A2R(d->lsib, d);
			A2R(d->rsib, d);
		} else
		{
			d = mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt);
			if (NULL != d)
			{	/* We found the right node */
				if (d->owner)
				{
					if (d->owner != process_id || d->auxowner != auxown)
					{	/* If owned and not owned by us check if owner is alive */
						if (is_proc_alive(d->owner, IMAGECNT(d->image_count)))
						{	/* Signal that this lock request is blocked by this node */
							p->blocked = d;
							p->blk_sequence = d->sequence;
							TREF(mlk_yield_pid) = 0;
							blocked = TRUE;
						} else
						{	/* Owner is dead so release this node */
							d->owner = 0;
							d->auxowner = 0;
						}
					}
				} else if ((MLK_FAIRNESS_DISABLED != TREF(mlk_yield_pid)) && d->pending)
				{	/* If not owned by us, but there is another process waiting for it at the start of
					 * the wait queue, then yield to it once. If we find the same process at the start
					 * of the wait queue again, then dont yield anymore to avoid starvation. If we find
					 * a different pid though, note it down and give it a fresh new chance. Since
					 * additions to the wait queue happen at the end, we will eventually get our turn
					 * this way (won't starve).  Fairness algorithm will not kick in if it is disabled
					 * which is indicated by setting TREF(mlk_yield_pid) to MLK_FAIRNESS_DISABLED.
					 */
					pr = (mlk_prcblk_ptr_t)R2A(d->pending);
					yield_pid = TREF(mlk_yield_pid);
					assert(yield_pid != process_id);
					assert(pr->process_id);
					if ((pr->process_id != yield_pid) && (process_id != pr->process_id))
					{
						p->blocked = d;
						p->blk_sequence = d->sequence;
						TREF(mlk_yield_pid) = pr->process_id;
						blocked =TRUE;
						/* Give the first waiting process a nudge to wake up */
						ctl = p->ctlptr;
						mlk_wake_pending(ctl, d, p->region);
					}
				}
			} else
			{	/* Add a new shrblk node to the end of the list */
				d = (mlk_shrblk_ptr_t)R2A(*chld_of_pnt);
				d0 = d;
				d1 = (mlk_shrblk_ptr_t)R2A(d->lsib);
				if (!(d = mlk_shrblk_create(p, cp, slen, pnt, NULL, i)))
				{
					assert(NULL == mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt));
					return TRUE;	/* resource starve -- no room for new shrblk */
				}
				assert((dh = mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt)) == d);
				A2R(d->rsib, d0);
				A2R(d->lsib, d1);
				A2R(d0->lsib, d);
				A2R(d1->rsib, d);
			}
		}
		/* When we get to the last "subscript", it's node is our lock target */
		if (i == 1)
			*ret = d;
	}
	if (*chld_of_pnt && !blocked)
	{	/* look at the subtree owners to see if we will be blocked by someone underneath */
		blocked = mlk_find_blocking_child_lock(p, (mlk_shrblk_ptr_t)R2A(*chld_of_pnt), auxown);
		TREF(mlk_yield_pid) = 0;	/* clear this just in case "blocked" came back as TRUE */
	}
	return blocked;
}

mlk_shrblk_ptr_t mlk_shrhash_find(mlk_pvtblk *p, int subnum, unsigned char *subval, unsigned char sublen, mlk_shrblk_ptr_t parent)
{
	mlk_shrblk_ptr_t	res = NULL, search_shrblk;
	mlk_shrsub_ptr_t	search_sub;
	int			bi, si;
	uint4			hash, num_buckets, usedmap;
	mlk_shrhash_ptr_t	shrhash, bucket, search_bucket;

	shrhash = (mlk_shrhash_ptr_t)R2A(p->ctlptr->blkhash);
	num_buckets = p->ctlptr->num_blkhash;
	hash = MLK_PVTBLK_SUBHASH(p, subnum);
	bi = hash % num_buckets;
	bucket = &shrhash[bi];
	usedmap = bucket->usedmap;
	for (si = bi ; 0 != usedmap ; (si = (si + 1) % num_buckets), (usedmap >>= 1))
	{
		if (0 == (usedmap & 1U))
			continue;
		search_bucket = &shrhash[si];
		if (search_bucket->hash != hash)
			continue;
		assert(0 != search_bucket->shrblk);
		search_shrblk = (mlk_shrblk_ptr_t)R2A(search_bucket->shrblk);
		if ((!((NULL == parent) && (0 == search_shrblk->parent)))
				&& ((mlk_shrblk_ptr_t)R2A(search_shrblk->parent) != parent))
			continue;
		search_sub = (mlk_shrsub_ptr_t)R2A(search_shrblk->value);
		if (0 != memvcmp(subval, sublen, search_sub->data, search_sub->length))
			continue;
		res = search_shrblk;
		break;
	}
	return res;
}

