/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "filestruct.h"
#include "mlkdef.h"
#include "lockdefs.h"
#include "cdb_sc.h"
#include "jnl.h"
#include "gdscc.h"
#include "gdskill.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"

/* Include prototypes */
#include "mlk_garbage_collect.h"
#include "mlk_prcblk_add.h"
#include "mlk_prcblk_delete.h"
#include "mlk_shrblk_find.h"
#include "mlk_lock.h"
#include "t_retry.h"
#include "gvusr.h"

#ifdef VMS
	GBLREF int4 image_count;
	GBLREF int4 login_time[2];
#endif

GBLREF	int4		process_id;
GBLREF	short		crash_count;
GBLREF	short		dollar_tlevel;
GBLREF	unsigned int	t_tries;
GBLREF	tp_region	*tp_reg_free_list;	/* Ptr to list of tp_regions that are unused */
GBLREF  tp_region	*tp_reg_list;		/* Ptr to list of tp_regions for this transaction */

/*
 * ------------------------------------------------------
 * mlk_lock()
 * Low level lock.
 *
 * Return:
 *	0 - Locked
 *	> 0 - number of times blocked process was woken up
 * ------------------------------------------------------
 */
uint4 mlk_lock(mlk_pvtblk *p,
	       uint4 auxown,
	       bool new)
{
	mlk_ctldata_ptr_t	ctl;
	mlk_shrblk_ptr_t	d;
	int			siz, retval, status;
	bool			blocked, was_crit;
	sgmnt_addrs		*csa;

	if (p->region->dyn.addr->acc_meth != dba_usr)
	{
		csa = &FILE_INFO(p->region)->s_addrs;

		ctl = p->ctlptr;
		if (csa->critical)
			crash_count = csa->critical->crashcnt;

		if (0 < dollar_tlevel && !((t_tries < CDB_STAGNATE) || csa->now_crit)) /* Final retry and region not locked down */
		{	/* make sure this region is in the list in case we end up retrying */
			insert_region(p->region, &tp_reg_list, &tp_reg_free_list, sizeof(tp_region));
			/* insert_region() will additionally attempt CRIT on the region and restart if not possible */
			assert(csa->now_crit);
		}
                if (FALSE == (was_crit = csa->now_crit))
			grab_crit(p->region);

		retval = ctl->wakeups;
		/* this calculation is size of basic mlk_shrsub blocks plus the padded value length
		   that already contains the consideration for the length byte. This is so we get
		   room to put a bunch of nicely aligned blocks so the compiler can give us its
		   best shot at efficient code. */
		siz = p->subscript_cnt * (3 + sizeof(mlk_shrsub) - 1) + p->total_length;
		if (ctl->subtop - ctl->subfree < siz || ctl->blkcnt < p->subscript_cnt)
			mlk_garbage_collect(ctl, siz, p);
		blocked = mlk_shrblk_find(p, &d, auxown);
		if (!d)
		{	/* Needed to create a shrblk but no space was available */
			if (FALSE == was_crit)
				rel_crit(p->region);
			return retval;	/* Resource starve */
		}
		if (d->owner)
		{	/* The lock already exists */
			if (d->owner == process_id && d->auxowner == auxown)
			{	/* We are already the owner */
				p->nodptr = d;
				retval = 0;
			} else
			{	/* Someone else has it. Block on it */
				if (new)
					mlk_prcblk_add(p->region, ctl, d, process_id);
				p->nodptr = d;
				p->sequence = d->sequence;
				csa->hdr->trans_hist.lock_sequence++;
			}
		} else
		{	/* Lock was not previously owned */
			if (blocked)
			{	/* We can't have it right now because of child or parent locks */
				if (new)
					mlk_prcblk_add(p->region, ctl, d, process_id);
				p->nodptr = d;
				p->sequence = d->sequence;
				csa->hdr->trans_hist.lock_sequence++;
			} else
			{	/* The lock is graciously granted */
				if (!new)
					mlk_prcblk_delete(ctl, d, process_id);
				d->owner = process_id;
				d->auxowner = auxown;
				d->sequence = p->sequence = csa->hdr->trans_hist.lock_sequence++;
				MLK_LOGIN(d);
				p->nodptr = d;
				retval = 0;
			}
		}
		if (FALSE == was_crit)
			rel_crit(p->region);
	} else	/* acc_meth = dba_usr */
		retval = gvusr_lock(p->total_length, &p->value[0], p->region);

	return retval;
}
