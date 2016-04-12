/****************************************************************
 *								*
 * Copyright (c) 2010-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_rel_quant.h"
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
GBLREF	uint4			process_id;
GBLREF	uint4			image_count;
GBLREF	int			num_additional_processors;
GBLREF	node_local_ptr_t	locknl;
GBLREF	gd_region		*gv_cur_region;

/* The below lock modules are modeled over shmpool.c. Change in one should be reflected in the other one as well */

boolean_t ss_get_lock(gd_region *reg)
{
	int			retries, spins, maxspins;
	int4			max_sleep_mask;
	sm_global_latch_ptr_t	latch;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;

	csa = &FILE_INFO(reg)->s_addrs;
	cnl = csa->nl;
	latch = &cnl->snapshot_crit_latch;
	max_sleep_mask = -1;	/* initialized to -1 to defer memory reference until needed */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	/* Since LOCK_TRIES is approx 50 seconds, give us 4X that long since IO is involved */
	++fast_lock_count;			 /* Disable wcs_stale for duration */
	for (retries = (LOCK_TRIES * 4) - 1; 0 < retries; retries--)
	{	/* this should use a mutex rather than a spin lock */
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
		REST_FOR_LATCH(latch, (-1 == max_sleep_mask) ? SPIN_SLEEP_MASK(csa->hdr) : max_sleep_mask, retries);
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
