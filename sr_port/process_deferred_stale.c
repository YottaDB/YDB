/****************************************************************
 *								*
 *	Copyright 2005, 2008 Fidelity Information Services, Inc	*
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

GBLREF	boolean_t	unhandled_stale_timer_pop;
GBLREF	gd_region	*gv_cur_region;

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
				assert(!T_IN_CRIT_OR_COMMIT_OR_WRITE(csa));
				if (csa->stale_defer)
				{
					gv_cur_region = r_cur;
					tp_change_reg();
#if defined(UNIX) && defined(UNTARGETED_MSYNC)
					if (csa->ti->last_mm_sync != csa->ti->curr_tn)
					{
						boolean_t    was_crit;

						was_crit = csa->now_crit;
						if (FALSE == was_crit)
							grab_crit(r_cur);
						msync((caddr_t)csa->db_addrs[0],
							(size_t)(csa->db_addrs[1] - csa->db_addrs[0]),
							MS_SYNC);
						/* Save when did last full sync */
						csa->ti->last_mm_sync = csa->ti->curr_tn;
						if (FALSE == was_crit)
							rel_crit(r_cur);
					}
#else
					DCLAST_WCS_WTSTART(r_cur, 0, status);
#endif
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
