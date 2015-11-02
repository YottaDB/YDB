/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include <errno.h>
#include "aswp.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "copy.h"
#include "interlock.h"
#include "performcaslatchcheck.h"
#include "relqop.h"
#include "wcs_sleep.h"
#include "caller_id.h"
#include "rel_quant.h"
#include "sleep_cnt.h"
#include "gtmsource_srv_latch.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "have_crit.h"

GBLREF	int4			process_id;
GBLREF	int			num_additional_processors;
GBLREF	jnlpool_addrs		jnlpool;
#ifdef DEBUG
GBLREF	node_local_ptr_t	locknl;
GBLREF	gd_region		*gv_cur_region;
#endif


/* Note we don't increment fast_lock_count as part of getting the latch and decrement it when releasing it because ROLLBACK
 * can hold onto this latch for a long while and can do updates in this duration and we should NOT have a non-zero fast_lock_count
 * as many places like t_begin/dsk_read have asserts to this effect. It is okay to NOT increment fast_lock_count as ROLLBACK
 * anyways have logic to disable interrupts the moment it starts doing database updates.
 */
boolean_t	grab_gtmsource_srv_latch(sm_global_latch_ptr_t latch, uint4 max_timeout_in_secs)
{
	int			spins, maxspins, retries, max_retries;
	unix_db_info		*udi;
	sgmnt_addrs		*repl_csa;

	assert(!have_crit(CRIT_HAVE_ANY_REG));
	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	repl_csa = &udi->s_addrs;
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	max_retries = max_timeout_in_secs * 4 * 1000; /* outer-loop : X minutes, 1 loop in 4 is sleep of 1 ms */
	for (retries = max_retries - 1; 0 < retries; retries--)
	{
		for (spins = maxspins; 0 < spins; spins--)
		{
			assert(latch->u.parts.latch_pid != process_id); /* We better not hold it if trying to get it */
			if (GET_SWAPLOCK(latch))
			{
				DEBUG_ONLY(locknl = repl_csa->nl); /* Use the journal pool to maintain lock history */
				LOCK_HIST("OBTN", latch, process_id, retries);
				DEBUG_ONLY(locknl = NULL);
				return TRUE;
			}
		}
		if (retries & 0x3)
		{	/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();
		} else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			if (RETRY_CASLATCH_CUTOFF == (retries % LOCK_TRIES))
				performCASLatchCheck(latch, TRUE);
		}
	}
	DUMP_LOCKHIST();
	assert(FALSE);
	return FALSE;
}

boolean_t	rel_gtmsource_srv_latch(sm_global_latch_ptr_t latch)
{
	sgmnt_addrs			*repl_csa;

	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	DEBUG_ONLY(locknl = repl_csa->nl);
	LOCK_HIST("RLSE", latch, process_id, 0);
	DEBUG_ONLY(locknl = NULL);
	assert(process_id == latch->u.parts.latch_pid);
	RELEASE_SWAPLOCK(latch);
	return TRUE;
}

boolean_t	gtmsource_srv_latch_held_by_us()
{
	return (process_id == jnlpool.gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid);
}
