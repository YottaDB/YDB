/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stddef.h>
#include <sys/shm.h>
#include "mdef.h"
#include "gtm_string.h"		/* for memcpy */
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"	/* Needed for AIX's silly open to open64 translations */
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
#include "tpnotacid_chk_inline.h"
#include "gtmimagename.h"
#include "cmidef.h"		/* for cmmdef.h */
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"		/* for curr_entry structure definition */
#include "do_shmat.h"

/* Include prototypes */
#include "mlk_ops.h"
#include "mlk_garbage_collect.h"
#include "mlk_prcblk_add.h"
#include "mlk_prcblk_delete.h"
#include "mlk_shrblk_find.h"
#include "mlk_shrhash_resize.h"
#include "mlk_rehash.h"
#include "mlk_lock.h"
#include "t_retry.h"
#include "gvusr.h"
#include "interlock.h"
#include "rel_quant.h"
#include "have_crit.h"

GBLREF	intrpt_state_t  intrpt_ok_state;
GBLREF	tp_region	*tp_reg_free_list;	/* Ptr to list of tp_regions that are unused */
GBLREF	tp_region	*tp_reg_list;		/* Ptr to list of tp_regions for this transaction */
GBLREF	uint4		dollar_tlevel, process_id;
#ifdef	DEBUG
GBLREF	unsigned int	t_tries;
#endif

error_def(ERR_LOCKSPACEFULL);
error_def(ERR_LOCKSPACEINFO);

#define LOCKGCINTP		"A LOCK requiring LOCK_SPACE cleanup"	 /* Garbage collection in the third retry is TPNOTACID */

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
gtm_uint64_t mlk_lock(mlk_pvtblk *p, UINTPTR_T auxown, boolean_t new)
{
	boolean_t		added, blocked, was_crit;
	connection_struct	*curr_entry;	/* for GT.CM GNP server */
	gtm_uint64_t		retval;
	int			siz;
	intrpt_state_t		prev_intrpt_state;
	mlk_ctldata_ptr_t	ctl;
	mlk_shrblk_ptr_t	our;
	sgmnt_addrs		*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (p->pvtctl.region->dyn.addr->acc_meth != dba_usr)
	{
		csa = p->pvtctl.csa;
		ctl = p->pvtctl.ctl;
		if (dollar_tlevel)
		{
			assert((CDB_STAGNATE > t_tries) || csa->now_crit || !csa->lock_crit_with_db);
			/* make sure this region is in the list in case we end up retrying */
			insert_region(p->pvtctl.region, &tp_reg_list, &tp_reg_free_list, SIZEOF(tp_region));
		}
		GRAB_LOCK_CRIT_AND_SYNC(&p->pvtctl, was_crit);
		DEFER_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
		retval = ctl->wakeups;
		assert(retval);
		/* this calculation is size of basic mlk_shrsub blocks plus the padded value length
		   that already contains the consideration for the length byte. This is so we get
		   room to put a bunch of nicely aligned blocks so the compiler can give us its
		   best shot at efficient code. */
		siz = MLK_PVTBLK_SHRSUB_SIZE(p, p->subscript_cnt);
		assert(siz >= 0);
		assert(ctl->blkcnt >= 0);
		if (ctl->gc_needed || ctl->resize_needed || ctl->rehash_needed
			|| (ctl->subtop - ctl->subfree < siz) || (ctl->blkcnt < p->subscript_cnt))
		{
			REL_LOCK_CRIT(&p->pvtctl, was_crit);
			ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
			TPNOTACID_CHECK(LOCKGCINTP);
			prepare_for_gc(&p->pvtctl);
			GRAB_LOCK_CRIT_AND_SYNC(&p->pvtctl, was_crit);
			DEFER_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
			assert(ctl->lock_gc_in_progress.u.parts.latch_pid == process_id);
			if (ctl->rehash_needed)
				mlk_rehash(&p->pvtctl);
			if (ctl->resize_needed)
				mlk_shrhash_resize(&p->pvtctl);
			else if (ctl->gc_needed || (ctl->subtop - ctl->subfree < siz) || (ctl->blkcnt < p->subscript_cnt))
				mlk_garbage_collect(p, siz, FALSE);
			assert(ctl->lock_gc_in_progress.u.parts.latch_pid == process_id);
			RELEASE_SWAPLOCK(&ctl->lock_gc_in_progress);
		}
		assert(!new || (0 == TREF(mlk_yield_pid)) || (MLK_FAIRNESS_DISABLED == TREF(mlk_yield_pid)));
		blocked = mlk_shrblk_find(p, &our, auxown);
		if (NULL != our)
		{
			if (our->owner)
			{	/* The lock already exists */
				if ((our->owner == process_id) && (our->auxowner == auxown))
				{	/* We are already the owner */
					p->nodptr = our;
					retval = 0;
				} else
				{	/* Someone else has it. Block on it */
					assert(blocked);
					added = TRUE;
					/* If we get a new prcblk, we should update the values
					 *   if we attempt to get a new prcblk and fail, we should update the transaction number
					 *   but take no further action */
					if (new)
						added = mlk_prcblk_add(p->pvtctl.region, ctl, our, process_id);
					if (added)
					{
						p->nodptr = our;
						p->sequence = our->sequence;
					}
					csa->hdr->trans_hist.lock_sequence++;
				}
			} else
			{	/* Lock was not previously owned */
				if (blocked)
				{	/* We can't have it right now because of child or parent locks */
					added = TRUE;
					if (new)
						added = mlk_prcblk_add(p->pvtctl.region, ctl, our, process_id);
					if (added)
					{
						p->nodptr = our;
						p->sequence = our->sequence;
					}
					csa->hdr->trans_hist.lock_sequence++;
				} else
				{	/* The lock is graciously granted */
					if (!new)
						mlk_prcblk_delete(&p->pvtctl, our, process_id);
					our->owner = process_id;
					our->auxowner = auxown;
					if (auxown && IS_GTCM_GNP_SERVER_IMAGE)
					{	/* called from gtcml_lock_internal() */
						curr_entry = (connection_struct *)auxown;
						our->auxpid = curr_entry->pvec->jpv_pid;
						assert(SIZEOF(curr_entry->pvec->jpv_node) <= SIZEOF(our->auxnode));
						memcpy(our->auxnode,
							&curr_entry->pvec->jpv_node[0], SIZEOF(curr_entry->pvec->jpv_node));
						/* cases of calls from omi_prc_lock() and rc_prc_lock() are not currently handled */
					}
					our->sequence = p->sequence = csa->hdr->trans_hist.lock_sequence++;
					MLK_LOGIN(our);
					p->nodptr = our;
					retval = 0;
				}
			}
		} else if (!ctl->lockspacefull_logged)
		{ 	/* Needed to create a shrblk but no space was available. Resource starve. Print LOCKSPACEFULL to syslog
			 * and prevent printing LOCKSPACEFULL until (free space)/(lock space) ratio is above
			 * LOCK_SPACE_FULL_SYSLOG_THRESHOLD.
			 */
			ctl->lockspacefull_logged = TRUE;
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, DB_LEN_STR(p->pvtctl.region));
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(p->pvtctl.region),
					(ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
					(ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt,
					(ctl->subfree - ctl->subbase), (ctl->subtop - ctl->subbase));
		}
		REL_LOCK_CRIT(&p->pvtctl, was_crit);
		ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
		if (!retval)
		{
			INCR_GVSTATS_COUNTER(csa, csa->nl, n_lock_success, 1);
		} else
		{
			INCR_GVSTATS_COUNTER(csa, csa->nl, n_lock_fail, 1);
		}
	} else	/* acc_meth = dba_usr */
		retval = gvusr_lock(p->nref_length, &p->value[0], p->pvtctl.region);
	return retval;
}
