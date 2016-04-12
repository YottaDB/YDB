/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "cdb_sc.h"
#include "jnl.h"
#include "gdscc.h"
#include "gdskill.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "t_retry.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mlk_tree_wake_children.h"
#include "mlk_unlock.h"
#include "mlk_wake_pending.h"
#include "gvusr.h"
#include "min_max.h"

GBLREF	int4 		process_id;
GBLREF	short		crash_count;
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned int	t_tries;
GBLREF	tp_region	*tp_reg_free_list;	/* Ptr to list of tp_regions that are unused */
GBLREF  tp_region	*tp_reg_list;		/* Ptr to list of tp_regions for this transaction */

void mlk_unlock(mlk_pvtblk *p)
{
	int			status;
	mlk_shrblk_ptr_t	d, pnt;
	mlk_ctldata_ptr_t	ctl;
	bool			stop_waking, was_crit;
	sgmnt_addrs		*csa;
	float			ls_free;	/* Free space in bottleneck subspace */

	if (p->region->dyn.addr->acc_meth != dba_usr)
	{
		ctl = p->ctlptr;
		assert((ctl->max_blkcnt > 0) && (ctl->max_prccnt > 0) && ((ctl->subtop - ctl->subbase) > 0));
		csa = &FILE_INFO(p->region)->s_addrs;
		d = p->nodptr;
		if (csa->critical)
			crash_count = csa->critical->crashcnt;
		if (dollar_tlevel)
		{
			assert((CDB_STAGNATE > t_tries) || csa->now_crit);
			/* make sure this region is in the list in case we end up retrying */
			insert_region(p->region, &tp_reg_list, &tp_reg_free_list, SIZEOF(tp_region));
		}
		if (FALSE == (was_crit = csa->now_crit))
			grab_crit(p->region);
		if (d->owner == process_id && p->sequence == d->sequence)
		{
			d->owner = 0;
			d->sequence = csa->hdr->trans_hist.lock_sequence++;
			assert(!d->owner || d->owner == process_id);
			/* do not call mlk_tree_wake_children/mlk_wake_pending on "d" if d->owner is non-zero.
			 * for comments on why, see comments about d->owner in mlk_wake_pending.c
			 */
			stop_waking = (d->children && !d->owner)
						? mlk_tree_wake_children(ctl, (mlk_shrblk_ptr_t)R2A(d->children), p->region)
						: FALSE;
			for ( ; d ; d = pnt)
			{
				pnt = ((d->parent) ? (mlk_shrblk_ptr_t)R2A(d->parent) : 0);
				if (!stop_waking && d->pending && !d->owner)
				{
					mlk_wake_pending(ctl, d, p->region);
					stop_waking = TRUE;
				} else
					mlk_shrblk_delete_if_empty(ctl, d);
			}
		}
		/* Find the ratio of least free subspace. Here we intentionally ignore shr_sub_len to keep unlock lightweight. */
		ls_free = MIN(((float)ctl->blkcnt) / ctl->max_blkcnt, ((float)ctl->prccnt) / ctl->max_prccnt);
		if (ls_free >= LOCK_SPACE_FULL_SYSLOG_THRESHOLD)
			csa->nl->lockspacefull_logged = FALSE; /* Allow syslog writes if enough free space is established. */
		if (FALSE == was_crit)
			rel_crit(p->region);
	} else	/* acc_meth == dba_usr */
		gvusr_unlock(p->total_length, &p->value[0], p->region);
	return;
}
