/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifdef VMS
#include <ssdef.h>
#endif

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "filestruct.h"
#include "lockdefs.h"
#include "lk_check_own.h"
#include "is_proc_alive.h"

GBLREF	short	crash_count;

/*
 * ------------------------------------------------
 * Check if owner process of the lock is still alive
 * If the process is not alive, clear the lock.
 *
 * Return:
 *	TRUE - cleared the owner
 *	FALSE - otherwise
 * ------------------------------------------------
 */
boolean_t	lk_check_own(mlk_pvtblk *x)
{
	int4		status;
	int4		icount, time[2];
	boolean_t	ret_val, was_crit;
	sgmnt_addrs	*csa;

	if (!x->blocked)
		return FALSE;
	csa = &FILE_INFO(x->region)->s_addrs;
	if (csa->critical)
		crash_count = csa->critical->crashcnt;
	was_crit = csa->now_crit;
	if (!was_crit)
		grab_crit(x->region);           /* check on process that owns lock */
	ret_val = FALSE;
	if (x->blocked->owner)
	{	/* There is an owner for the blocking node */
		if (x->blocked->sequence != x->blk_sequence)
		{	/* The node we were blocking on has been reused for something else so
			   we are no longer blocked on it and can pretend that the process
			   holding the lock went away */
			ret_val = TRUE;
		} else if (BLOCKING_PROC_ALIVE(x, time, icount, status))
		{	/* process that owned lock has died, free lock. */
			x->blocked->owner = 0;
			csa->hdr->trans_hist.lock_sequence++;
			ret_val = TRUE;
		}
	} else
		ret_val = TRUE;	/* There is no owner. Take credit for freeing it.. */
	if (!was_crit)
		rel_crit(x->region);
	return ret_val;
}
