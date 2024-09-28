/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
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

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	uint4		process_id;

boolean_t	mlk_find_blocking_child_lock(mlk_pvtblk *p, mlk_shrblk_ptr_t child, UINTPTR_T auxown);

boolean_t	mlk_find_blocking_child_lock(mlk_pvtblk *p, mlk_shrblk_ptr_t child, UINTPTR_T auxown)
{
	boolean_t		blocked;
	mlk_shrblk_ptr_t	d, d1, dhead;

	blocked = FALSE;
	for (dhead = d = child, d1 = NULL ; dhead != d1 && !blocked ; d = d1 = (mlk_shrblk_ptr_t)R2A(d->rsib))
	{	/* There is similar code below to check if the process that owns the lock still exists.
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
		{
			CHECK_SHRBLKPTR(d->children, p->pvtctl);
			blocked = mlk_find_blocking_child_lock(p, (mlk_shrblk_ptr_t)R2A(d->children), auxown);
		}
	}
	return blocked;
}

mlk_shrblk_ptr_t mlk_shrhash_find(mlk_pvtblk *p, int subnum, unsigned char *subval, unsigned char sublen, mlk_shrblk_ptr_t parent);

/**
 * Searches the shrblk's for the shrblk belonging to the given pvtblk; if it doesn't find it, it creates it
 *
 * @param [in] p private block which contains the lock name
 * @param [out] ret will contain the address of shrblk for the last subscript of the given lock
 * @param [in] auxown who this lock is being requested on behalf of
 * @returns 0 if success, or 1 if we are blocked from getting that lock for some reason
 */
boolean_t	mlk_shrblk_find(mlk_pvtblk *p, mlk_shrblk_ptr_t *ret, UINTPTR_T auxown)
{
	boolean_t		blocked;
	int			i;
	mlk_ctldata_ptr_t	ctl;
	mlk_prcblk_ptr_t	pr;
	mlk_shrblk_ptr_t	d, d0, d1, dhead, pnt;
	ptroff_t		*chld_of_pnt;
	uint4			yield_pid;
	unsigned char		*cp, slen;
	DCL_THREADGBL_ACCESS;

	assert(INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state);
	blocked = FALSE;
	/* Note: If ever this function returns with "blocked" set to "TRUE",
	 * make sure TREF(mlk_yield_pid) is initialized appropriately.
	 */
	*ret = 0;
	SETUP_THREADGBL_ACCESS;
	MLK_PVTBLK_SUBHASH_SYNC(p);
	for (pnt = NULL , chld_of_pnt = (ptroff_t *)&p->pvtctl.ctl->blkroot , i = p->subscript_cnt , cp = p->value ;
		i > 0 ; i-- , pnt = d , chld_of_pnt = (ptroff_t *)&d->children, cp += slen)
	{
		slen = *cp++;
		if (!*chld_of_pnt)
		{
			assert(NULL == mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt));
			if (!(d = mlk_shrblk_create(p, cp, slen, pnt, chld_of_pnt, i)))
			{
				assert(NULL == mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt));
				CHECK_SHRBLKPTR(p->pvtctl.ctl->blkroot, p->pvtctl);
				return TRUE;
			}
			assert((dhead = mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt)) == d);
			assert(d->value != 0);
			A2R(d->lsib, d);
			CHECK_SHRBLKPTR(d->lsib, p->pvtctl);
			A2R(d->rsib, d);
			CHECK_SHRBLKPTR(d->rsib, p->pvtctl);
			CHECK_SHRBLKPTR(p->pvtctl.ctl->blkroot, p->pvtctl);
		} else
		{
			d = mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt);
			if (NULL != d)
			{	/* We found the right node */
				assertpro(d->rsib != 0);
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
						/* If we are waiting on two pending processes, disable fairness
						 * so we don't get livelocked */
						if (TREF(mlk_yield_pid) && p->blocked && (p->blocked != d))
							TREF(mlk_yield_pid) = MLK_FAIRNESS_DISABLED;
						else
							TREF(mlk_yield_pid) = pr->process_id;
						p->blocked = d;
						p->blk_sequence = d->sequence;
						blocked = TRUE;
						/* Give the first waiting process a nudge to wake up */
						ctl = p->pvtctl.ctl;
						mlk_wake_pending(&p->pvtctl, d);
					}
				}
			} else
			{	/* Add a new shrblk node to the end of the list */
				assert(*chld_of_pnt != 0);
				CHECK_SHRBLKPTR(*chld_of_pnt, p->pvtctl);
				d = (mlk_shrblk_ptr_t)R2A(*chld_of_pnt);
				assertpro(d->lsib);
				assertpro(d->lsib != INVALID_LSIB_MARKER);
				d0 = d;
				d1 = (mlk_shrblk_ptr_t)R2A(d->lsib);
				if (!(d = mlk_shrblk_create(p, cp, slen, pnt, NULL, i)))
				{
					assert(NULL == mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt));
					return TRUE;	/* resource starve -- no room for new shrblk */
				}
				assert((dhead = mlk_shrhash_find(p, p->subscript_cnt - i, cp, slen, pnt)) == d);
				A2R(d->rsib, d0);
				CHECK_SHRBLKPTR(d->rsib, p->pvtctl);
				d->lsib = 0;
				if (d1)
					A2R(d->lsib, d1);
				CHECK_SHRBLKPTR(d->lsib, p->pvtctl);
				A2R(d0->lsib, d);
				CHECK_SHRBLKPTR(d0->lsib, p->pvtctl);
				if (d1)
					A2R(d1->rsib, d);
				CHECK_SHRBLKPTR(d1->rsib, p->pvtctl);
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
	/* We successfully got shrblks/shrhashes for all nodes of the lock, so clear its fail count. */
	p->pvtctl.hash_fail_cnt = 0;
	return blocked;
}

mlk_shrblk_ptr_t mlk_shrhash_find(mlk_pvtblk *p, int subnum, unsigned char *subval, unsigned char sublen, mlk_shrblk_ptr_t parent)
{
	int			bi, si;
	mlk_shrblk_ptr_t	res = NULL, search_shrblk;
	mlk_shrhash_map_t	usedmap;
	mlk_shrhash_ptr_t	shrhash, bucket, search_bucket;
	mlk_shrsub_ptr_t	search_sub;
	uint4			hash, num_buckets;

	assert(LOCK_CRIT_HELD(p->pvtctl.csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	shrhash = p->pvtctl.shrhash;
	num_buckets = p->pvtctl.shrhash_size;
	assert(p->hash_seed == p->pvtctl.ctl->hash_seed);
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
		assert(0 != search_bucket->shrblk_idx);
		search_shrblk = MLK_SHRHASH_SHRBLK(p->pvtctl, search_bucket);
		/* if the parents don't match (either both null, or one null and the other not, or both non-null but
		 *  non-matching then we continue
		 */
		if (!((NULL == parent) && (0 == search_shrblk->parent))
				&& ((0 == search_shrblk->parent) || ((mlk_shrblk_ptr_t)R2A(search_shrblk->parent) != parent)))
			continue;
		search_sub = (mlk_shrsub_ptr_t)R2A(search_shrblk->value);
		if (0 != memvcmp(subval, sublen, search_sub->data, search_sub->length))
			continue;
		res = search_shrblk;
		assert(hash == res->hash);
		break;
	}
	return res;
}
