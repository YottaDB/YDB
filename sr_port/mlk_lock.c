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
#include <stddef.h>
#include "gtm_string.h"		/* for memcpy */

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
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "gtmimagename.h"
#include "cmidef.h"		/* for cmmdef.h */
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"		/* for curr_entry structure definition */

/* Include prototypes */
#include "mlk_garbage_collect.h"
#include "mlk_prcblk_add.h"
#include "mlk_prcblk_delete.h"
#include "mlk_shrblk_find.h"
#include "mlk_lock.h"
#include "t_retry.h"
#include "gvusr.h"


#ifdef VMS
GBLREF uint4	image_count;
#endif

GBLREF	int4		process_id;
GBLREF	short		crash_count;
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned int	t_tries;
GBLREF	tp_region	*tp_reg_free_list;	/* Ptr to list of tp_regions that are unused */
GBLREF  tp_region	*tp_reg_list;		/* Ptr to list of tp_regions for this transaction */

error_def(ERR_LOCKSPACEFULL);
error_def(ERR_LOCKSPACEINFO);
error_def(ERR_GCBEFORE);
error_def(ERR_GCAFTER);

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
	       UINTPTR_T  auxown,
	       bool       new)
{
	mlk_ctldata_ptr_t	ctl;
	mlk_shrblk_ptr_t	d;
	int			siz, retval, status;
	boolean_t		blocked, was_crit;
	sgmnt_addrs		*csa;
	connection_struct	*curr_entry;	/* for GT.CM GNP server */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (p->region->dyn.addr->acc_meth != dba_usr)
	{
		csa = &FILE_INFO(p->region)->s_addrs;
		ctl = p->ctlptr;
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
		retval = ctl->wakeups;
		assert(retval);
		/* this calculation is size of basic mlk_shrsub blocks plus the padded value length
		   that already contains the consideration for the length byte. This is so we get
		   room to put a bunch of nicely aligned blocks so the compiler can give us its
		   best shot at efficient code. */
		siz = MLK_PVTBLK_SHRSUB_SIZE(p, p->subscript_cnt);
		assert(siz >= 0);
		assert(ctl->blkcnt >= 0);
		if (ctl->subtop - ctl->subfree < siz || ctl->blkcnt < p->subscript_cnt)
		{
			mlk_garbage_collect(ctl, siz, p);
		}
		assert(!new || (0 == TREF(mlk_yield_pid)) || (MLK_FAIRNESS_DISABLED == TREF(mlk_yield_pid)));
		blocked = mlk_shrblk_find(p, &d, auxown);
		if (NULL != d)
		{
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
					if (auxown && IS_GTCM_GNP_SERVER_IMAGE)
					{	/* called from gtcml_lock_internal() */
						curr_entry = (connection_struct *)auxown;
						d->auxpid = curr_entry->pvec->jpv_pid;
						assert(SIZEOF(curr_entry->pvec->jpv_node) <= SIZEOF(d->auxnode));
						memcpy(d->auxnode,
							&curr_entry->pvec->jpv_node[0], SIZEOF(curr_entry->pvec->jpv_node));
						/* cases of calls from omi_prc_lock() and rc_prc_lock() are not currently handled */
					}
					d->sequence = p->sequence = csa->hdr->trans_hist.lock_sequence++;
					MLK_LOGIN(d);
					p->nodptr = d;
					retval = 0;
				}
			}
		} else if (!csa->nl->lockspacefull_logged)
		{ 	/* Needed to create a shrblk but no space was available. Resource starve. Print LOCKSPACEFULL to syslog
			 * and prevent printing LOCKSPACEFULL until (free space)/(lock space) ratio is above
			 * LOCK_SPACE_FULL_SYSLOG_THRESHOLD.
			 */
			csa->nl->lockspacefull_logged = TRUE;
			send_msg(VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, DB_LEN_STR(p->region));
			if (ctl->subtop - ctl->subfree >= siz)
			{
				send_msg(VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(p->region),
					 (ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
					 (ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt, LEN_AND_LIT(" not "));
			} else
			{
				send_msg(VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(p->region),
					 (ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
					 (ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt, LEN_AND_LIT(" "));
			}
		}
		if (FALSE == was_crit)
			rel_crit(p->region);
		if (!retval)
		{
			INCR_GVSTATS_COUNTER(csa, csa->nl, n_lock_success, 1);
		} else
		{
			INCR_GVSTATS_COUNTER(csa, csa->nl, n_lock_fail, 1);
		}
	} else	/* acc_meth = dba_usr */
		retval = gvusr_lock(p->total_length, &p->value[0], p->region);
	return retval;
}
