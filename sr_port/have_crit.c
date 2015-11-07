/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "filestruct.h"
#include "jnl.h"
#include "dpgbldir.h"
#include "have_crit.h"
#include "send_msg.h"

GBLREF volatile int4		crit_count;
GBLREF jnlpool_addrs		jnlpool;
GBLREF uint4			process_id;
GBLREF uint4			crit_deadlock_check_cycle;
GBLREF uint4			dollar_tlevel;
GBLREF unsigned int		t_tries;
#if defined(UNIX) && defined(DEBUG)
GBLREF jnl_gbls_t		jgbl;
#endif

error_def(ERR_MUTEXRELEASED);

/* Return number of regions (including jnlpool dummy region) if have or are aquiring crit or in_wtstart
 * ** NOTE **  This routine is called from signal handlers and is thus called asynchronously.
 * If CRIT_IN_COMMIT bit is set, we check if in middle of commit (PHASE1 inside crit or PHASE2 outside crit) on some region.
 * If CRIT_RELEASE bit is set, we release crit on region(s) that:
 *   1)  we hold crit on (neither CRIT_IN_COMMIT NOR CRIT_TRANS_NO_REG is specified)
 *   2)  are part of the current transactions except those regions that are marked as being valid
 *       to have crit in by virtue of their crit_check_cycle value is the same as crit_deadlock_check_cycle.
 * Note: CRIT_RELEASE implies CRIT_ALL_REGIONS
 * If CRIT_ALL_REGIONS bit is set, go through the entire list of regions
 */
uint4 have_crit(uint4 crit_state)
{
	gd_region	*r_top, *r_local;
	gd_addr		*addr_ptr;
	sgmnt_addrs	*csa;
	uint4		crit_reg_cnt = 0;

	/* in order to proper release the necessary regions, CRIT_RELEASE implies going through all the regions */
	if (crit_state & CRIT_RELEASE)
	{
		UNIX_ONLY(assert(!jgbl.onlnrlbk)); /* should not request crit to be released if online rollback */
		crit_state |= CRIT_ALL_REGIONS;
	}
	if ((0 != crit_count) && (crit_state & CRIT_HAVE_ANY_REG))
	{
		crit_reg_cnt++;
		if (0 == (crit_state & CRIT_ALL_REGIONS))
			return crit_reg_cnt;
	}
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
		{
			if (r_local->open && !r_local->was_open)
			{
				csa = &FILE_INFO(r_local)->s_addrs;
				if (NULL != csa)
				{
					if ((csa->now_crit) && (crit_state & CRIT_HAVE_ANY_REG))
					{
						crit_reg_cnt++;
						/* It is possible that if DSE has done a CRIT REMOVE and stolen our crit, it
						 * could be given to someone else which would cause this test to fail. The
						 * current thinking is that the state DSE put this process is no longer viable
						 * and it should die at the earliest opportunity, there being no way to know if
						 * that is what happened anyway.
						 */
						if (csa->nl->in_crit != process_id)
							GTMASSERT;
						/* If we are releasing (all) regions with critical section or if special
						 * TP case, release if the cycle number doesn't match meaning this is a
						 * region we should not hold crit in (even if it is part of tp_reg_list).
						 */
						if ((0 != (crit_state & CRIT_RELEASE)) &&
						    (0 == (crit_state & CRIT_NOT_TRANS_REG) ||
						     crit_deadlock_check_cycle != csa->crit_check_cycle))
						{
							assert(WBTEST_HOLD_CRIT_ENABLED);
							assert(!csa->hold_onto_crit);
							rel_crit(r_local);
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_MUTEXRELEASED, 6, process_id,
								     process_id,  DB_LEN_STR(r_local), dollar_tlevel, t_tries);
						}
						if (0 == (crit_state & CRIT_ALL_REGIONS))
							return crit_reg_cnt;
					}
					/* In Commit-crit is defined as the time since when early_tn is 1 + curr_tn upto when
					 * t_commit_crit is set to FALSE. Note that the first check should be done only if we
					 * hold crit as otherwise we could see inconsistent values.
					 */
					if ((crit_state & CRIT_IN_COMMIT)
						&& (csa->now_crit && (csa->ti->early_tn != csa->ti->curr_tn) || csa->t_commit_crit))
					{
						crit_reg_cnt++;
						if (0 == (crit_state & CRIT_ALL_REGIONS))
							return crit_reg_cnt;
					}
					if ((crit_state & CRIT_IN_WTSTART) && csa->in_wtstart)
					{
						crit_reg_cnt++;
						if (0 == (crit_state & CRIT_ALL_REGIONS))
							return crit_reg_cnt;
					}
				}
			}
		}
	}
	if (NULL != jnlpool.jnlpool_ctl)
	{
		csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		if ((NULL != csa) && csa->now_crit && (crit_state & CRIT_HAVE_ANY_REG))
		{
			crit_reg_cnt++;
			if (0 != (crit_state & CRIT_RELEASE))
			{
				assert(!csa->hold_onto_crit);
				rel_lock(jnlpool.jnlpool_dummy_reg);
			}
		}
	}
	return crit_reg_cnt;
}
