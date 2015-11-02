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

#include <stddef.h>

#include "interlock.h"
#include "lockconst.h"
#include "add_inter.h"
#include "sleep_cnt.h"
#include "compswap.h"
#include "wcs_sleep.h"
#include "rel_quant.h"

GBLREF	int4		process_id;
GBLREF	volatile int4	fast_lock_count;		/* Used in wcs_stale */
GBLREF	int		num_additional_processors;

/* For those systems without an atomic increment compiler primative we have this flavor of
   atomic increment (by increment value passed in) based on a compare/exchange algorithm.
   Previous incarnations have used a lock-increment-unlock algorithm which on some architectures
   uses substantial numbers of memory barriers during the locking or unlocking process. The compswap
   approach uses only one set of barriers and those barriers on Itanium especially are fairly loose.
   This should be a higher performing incremental add than the micro lock version currently required
   on HPUX-HPPA.
*/

int4	add_inter(int val, sm_int_ptr_t addr, sm_global_latch_ptr_t latch)
{
	int4			cntrval, newcntrval, spins, maxspins, retries;
	boolean_t		cswpsuccess;
	sm_int_ptr_t volatile	cntrval_p;

	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);

	++fast_lock_count;
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	cntrval_p = addr;	/* Need volatile context especially on Itanium */
        for (retries = LOCK_TRIES - 1; 0 < retries; retries--)  /* - 1 so do rel_quant 3 times first */
        {
		for (spins = maxspins; 0 < spins; spins--)
		{
			cntrval = *cntrval_p;
			newcntrval = cntrval + val;
			/* This is (currently as of 08/2007) the only non-locking usage of compswap in GTM. We
			   are not passing compswap an actual sm_global_latch_ptr_t addr like its function would
			   normally dictate. However, since the address of the field we want to deal with is the
			   first int in the global_latch_t, we just pass our int address properly cast to the
			   type that compswap is expecting. The assert below verifies that this assumption has
			   not changed (SE 08/2007)
			*/
			assert(0 == OFFSETOF(global_latch_t, u.parts.latch_pid));
			IA64_ONLY(cswpsuccess = compswap_unlock(RECAST(sm_global_latch_ptr_t)cntrval_p, cntrval, newcntrval));
			NON_IA64_ONLY(cswpsuccess = compswap((sm_global_latch_ptr_t)cntrval_p, cntrval, newcntrval));
			if (cswpsuccess)
			{
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return newcntrval;
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
		}
	}
	--fast_lock_count;
	assert(FALSE);
	rts_error(VARLSTCNT(9) ERR_DBCCERR, 2, LEN_AND_LIT("*unknown*"), ERR_ERRCALL, 3, CALLFROM);
	return 0; /* To keep the compiler quiet */
}
