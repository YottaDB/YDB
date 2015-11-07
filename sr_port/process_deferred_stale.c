/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Process deferred stale timer interrupts.

   Flushing of queues is deferred if the process is in crit. When a transaction
   is complete, we will come here to process the deferred interrupt.
*/

#include "mdef.h"

#include <sys/mman.h>
#ifdef VMS
#  include <ssdef.h>
#  include "ast.h"
#endif

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsbgtr.h"
#include "tp_change_reg.h"
#include "dpgbldir.h"
#include "process_deferred_stale.h"
#ifdef UNIX
# include "jnl.h"
#endif

GBLREF	boolean_t	unhandled_stale_timer_pop;
GBLREF	gd_region	*gv_cur_region;
#ifdef UNIX
GBLREF	jnl_gbls_t	jgbl;
#endif

void process_deferred_stale(void)
{
	gd_region	*r_cur, *r_top, *save_gv_cur_region;
	gd_addr		*addr_ptr;
	sgmnt_addrs	*csa;
	int4		status;

	assert(unhandled_stale_timer_pop);

        save_gv_cur_region = gv_cur_region;

	for (addr_ptr = get_next_gdr(NULL); NULL != addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (r_cur = addr_ptr->regions, r_top = r_cur + addr_ptr->n_regions; r_cur < r_top; r_cur++)
		{
			if (r_cur->open)
			{
				csa = &FILE_INFO(r_cur)->s_addrs;
				/* We don't expect to be in crit or commit when process_deferred_stale is invoked. The only
				 * exceptions are :
				 * (a) ONLINE ROLLBACK - holds the crit for the entire duration. In this case, do the flush anyways.
				 *     While this might slowdown the online rollback run, with our current knowledge, we dont have
				 *     any benchmarks to decide whether it is a good thing to let the buffer flush at the end (in
				 *     gds_rundown) take care of all the updates done by rollback or do it periodically.
				 * (b) A concurrent process in t_end detected an ONLINE ROLLBACK in the final or penultimate retry
				 *     and invoked gvcst_redo_root_search to redo the root search of the concerned gv_target. This
				 *     root search ended up calling t_end and noticed an unhandled timer pop and thereby coming
				 *     here. Since root search is invoked in the 2nd or 3rd retry, the process sets hold_onto_crit.
				 *     So, assert that is indeed the case. Also, have a much stricter assert to ensure that
				 *     gv_target->root = DIR_ROOT (indicating we are in gvcst_root_search). Also, since we want to
				 *     avoid flushing buffers if already holding crit, continue to the next region.
				 * (c) In case of DSE commands (like DSE MAPS -RESTORE), we could come here with csa->hold_onto_crit
				 *     being TRUE (see t_begin_crit which DSE invokes).
				 */
				assert(!T_IN_CRIT_OR_COMMIT_OR_WRITE(csa) || csa->hold_onto_crit
					UNIX_ONLY(|| jgbl.onlnrlbk || (NULL == gv_target) || (DIR_ROOT == gv_target->root)));
				if (UNIX_ONLY(!jgbl.onlnrlbk && )csa->now_crit)
					continue;
				if (csa->stale_defer)
				{
					gv_cur_region = r_cur;
					tp_change_reg();
					DCLAST_WCS_WTSTART(r_cur, 0, status);
					csa->stale_defer = FALSE;
					BG_TRACE_ANY(csa, stale_defer_processed);
				}
			}
		}
	}
	unhandled_stale_timer_pop = FALSE;
	gv_cur_region = save_gv_cur_region;
	tp_change_reg();
}
