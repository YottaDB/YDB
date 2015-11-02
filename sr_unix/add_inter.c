/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "interlock.h"
#include "lockconst.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "add_inter.h"
#include "sleep_cnt.h"
#include "performcaslatchcheck.h"
#include "wcs_sleep.h"
#include "rel_quant.h"

GBLREF	int4		process_id;
GBLREF	gd_region	*gv_cur_region;
GBLREF	volatile int4	fast_lock_count;		/* Used in wcs_stale */
GBLREF	int		num_additional_processors;

int4	add_inter(int val, sm_int_ptr_t addr, sm_global_latch_ptr_t latch)
{
	int4	ret, retries, spins, maxspins;
	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);

	++fast_lock_count;
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	for (retries = LOCK_TRIES - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{
			if (GET_SWAPLOCK(latch))
			{
				*addr += val;
				ret = *addr;
				RELEASE_SWAPLOCK(latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return ret;
			}
		}
		if (retries & 0x3)
			/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			assert(0 == (LOCK_TRIES % 4)); /* assures there are 3 rel_quants prior to first wcs_sleep() */
			/* If near end of loop, see if target is dead and/or wake it up */
			if (RETRY_CASLATCH_CUTOFF == retries)
				performCASLatchCheck(latch, LOOP_CNT_SEND_WAKEUP);
		}
	}
	--fast_lock_count;
	DUMP_LOCKHIST();
	assert(FALSE);
	rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, LEN_AND_LIT("*unknown*"), ERR_ERRCALL, 3, CALLFROM);
	return 0; /* To keep the compiler quite */
}


