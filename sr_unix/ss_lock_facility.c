/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "relqop.h"
#include "copy.h"
#include "wcs_sleep.h"
#include "caller_id.h"
#include "rel_quant.h"
#include "sleep_cnt.h"
#include "interlock.h"
#include "is_proc_alive.h"
#include "mupipbckup.h"
#include "send_msg.h"
#include "performcaslatchcheck.h"
#include "gdsbgtr.h"
#include "lockconst.h"
#include "memcoherency.h"
#include "ss_lock_facility.h"

GBLREF	volatile int4		fast_lock_count;
GBLREF	pid_t			process_id;
GBLREF	uint4			image_count;
GBLREF	int			num_additional_processors;
GBLREF	node_local_ptr_t	locknl;
GBLREF	gd_region		*gv_cur_region;

/* The below lock modules are modeled over shmpool.c. Change in one should be reflected in the other one as well */

boolean_t ss_get_lock(gd_region *reg)
{
	int			retries, spins, maxspins;
	sm_global_latch_ptr_t	latch;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;

	csa = &FILE_INFO(reg)->s_addrs;
	cnl = csa->nl;
	latch = &cnl->snapshot_crit_latch;
	++fast_lock_count;			 /* Disable wcs_stale for duration */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	/* Since LOCK_TRIES is approx 50 seconds, give us 4X that long since IO is involved */
	for (retries = (LOCK_TRIES * 4) - 1; 0 < retries; retries--)  /* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{	/* We better not hold it if trying to get it */
			assert(latch->u.parts.latch_pid != process_id
			       VMS_ONLY(|| latch->u.parts.latch_image_count != image_count));
                        if (GET_SWAPLOCK(latch))
			{
				DEBUG_ONLY(locknl = csa->nl);
				LOCK_HIST("OBTN", latch, process_id, retries);
				DEBUG_ONLY(locknl = NULL);
				/* Note that fast_lock_count is kept incremented for the duration that we hold the lock
				   to prevent our dispatching an interrupt that could deadlock getting this lock
				*/
				return TRUE;
			}
		}
		if (retries & 0x3)
		{	/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		} else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			/* If near end of loop segment (LOCK_TRIES iters), see if target is dead and/or wake it up */
			if (RETRY_CASLATCH_CUTOFF == (retries % LOCK_TRIES))
				performCASLatchCheck(latch, TRUE);
		}
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return FALSE;
}

boolean_t ss_get_lock_nowait(gd_region *reg)
{
	sm_global_latch_ptr_t	latch;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;

	csa = &FILE_INFO(reg)->s_addrs;
	cnl = csa->nl;
	latch = &cnl->snapshot_crit_latch;
	++fast_lock_count;			/* Disable wcs_stale for duration */
	/* We better not hold it if trying to get it */
	assert(latch->u.parts.latch_pid != process_id VMS_ONLY(|| latch->u.parts.latch_image_count != image_count));
	if (GET_SWAPLOCK(latch))
	{
		DEBUG_ONLY(locknl = csa->nl);
		LOCK_HIST("OBTN", latch, process_id, -1);
		DEBUG_ONLY(locknl = NULL);
		/* Note that fast_lock_count is kept incremented for the duration that we hold the lock
		 * to prevent our dispatching an interrupt that could deadlock getting this lock
		 */
		return TRUE;
	}
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	return FALSE;
}

void ss_release_lock(gd_region *reg)
{
	sm_global_latch_ptr_t	latch;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;

	csa = &FILE_INFO(reg)->s_addrs;
	cnl = csa->nl;
	latch = &cnl->snapshot_crit_latch;
	assert(process_id == latch->u.parts.latch_pid VMS_ONLY(&& image_count == latch->u.parts.latch_image_count));
	DEBUG_ONLY(locknl = csa->nl);
	LOCK_HIST("RLSE", latch, process_id, 0);
	RELEASE_SWAPLOCK(latch);
	DEBUG_ONLY(locknl = NULL);
	--fast_lock_count;
	assert(0 <= fast_lock_count);
}

boolean_t ss_lock_held_by_us(gd_region *reg)
{
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;

	csa = &FILE_INFO(reg)->s_addrs;
	cnl = csa->nl;
	return GLOBAL_LATCH_HELD_BY_US(&cnl->snapshot_crit_latch);
}
