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
#include "jnl.h"
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
#include "util.h"		/* For OUT_BUFF_SIZE */

GBLREF	int4			process_id;
GBLREF	int			num_additional_processors;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnl_gbls_t		jgbl;
#ifdef DEBUG
GBLREF	node_local_ptr_t	locknl;
GBLREF	gd_region		*gv_cur_region;
GBLREF	boolean_t		is_src_server;
#endif

error_def(ERR_REPLREQROLLBACK);
error_def(ERR_TEXT);
/* Note we don't increment fast_lock_count as part of getting the latch and decrement it when releasing it because ROLLBACK
 * can hold onto this latch for a long while and can do updates in this duration and we should NOT have a non-zero fast_lock_count
 * as many places like t_begin/dsk_read have asserts to this effect. It is okay to NOT increment fast_lock_count as ROLLBACK
 * anyways have logic to disable interrupts the moment it starts doing database updates.
 */
boolean_t	grab_gtmsource_srv_latch(sm_global_latch_ptr_t latch, uint4 max_timeout_in_secs, uint4 onln_rlbk_action)
{
	int			spins, maxspins, retries, max_retries;
	unix_db_info		*udi;
	sgmnt_addrs		*repl_csa;
	boolean_t		cycle_mismatch;
	char			scndry_msg[OUT_BUFF_SIZE];

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
				if (jnlpool.repl_inst_filehdr->file_corrupt && !jgbl.onlnrlbk)
				{
					/* Journal pool indicates an abnormally terminated online rollback. Cannot continue until
					 * the rollback command is re-run to bring the journal pool/file and instance file to a
					 * consistent state.
					 */
					SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Instance file header has file_corrupt field set to "
							"TRUE");
					/* No need to release the latch before rts_error (mupip_exit_handler will do it for us) */
					rts_error(VARLSTCNT(8) ERR_REPLREQROLLBACK, 2, LEN_AND_STR(udi->fn),
						ERR_TEXT, 2, LEN_AND_STR(scndry_msg));
				}
				cycle_mismatch = (repl_csa->onln_rlbk_cycle != jnlpool.jnlpool_ctl->onln_rlbk_cycle);
				assert((ASSERT_NO_ONLINE_ROLLBACK != onln_rlbk_action) || !cycle_mismatch);
				if ((HANDLE_CONCUR_ONLINE_ROLLBACK == onln_rlbk_action) && cycle_mismatch)
				{
					assert(is_src_server);
					SYNC_ONLN_RLBK_CYCLES;
					gtmsource_onln_rlbk_clnup(); /* side-effect : sets gtmsource_state */
					rel_gtmsource_srv_latch(latch);
				}
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
	assert(jnlpool.gtmsource_local && jnlpool.gtmsource_local->gtmsource_pid);
	rts_error(VARLSTCNT(5) ERR_SRVLCKWT2LNG, 2, max_timeout_in_secs, jnlpool.gtmsource_local->gtmsource_pid);
	return FALSE; /* to keep the compiler happy */
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
