/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <lckdef.h>
#include <prvdef.h>
#include <ssdef.h>

#include "gtmsecshr.h"
#include "locks.h"

/* while this has the same interface as sys$enq, it is intended only for conversions
 * in order to record the lock in the list of locks to deq at image termination,
 * the lock must first be taken out (generally in NL mode with gtm_enqw() */
uint4 gtm_enq(
	unsigned int	efn,
	unsigned int	lkmode,
	lock_sb		*lsb,
	unsigned int	flags,
	void		*resnam,
	unsigned int	parid,
	void		*astadr,
	unsigned int	astprm,
	void		*blkast,
	unsigned int	acmode,
	unsigned int	nullarg)
{
	uint4		status;
	uint4		prvadr[2], prvprv[2];

	GTMSECSHR_SET_DBG_PRIV(PRV$M_SYSLCK, status);
	if (SS$_NORMAL == status)
	{
		if ((flags & LCK$M_CONVERT) && lsb->lockid)
			status = sys$enq(efn, lkmode, lsb, flags, resnam, parid, astadr, astprm, blkast, acmode, nullarg);
		else
			status = SS$_CVTUNGRANT;
			/* the above is an indication to the caller that the lock was not first established in NL mode
			 * with gtm_enqw, so that it is registered in vms_lock_list in case of process termination
			 * if circumstances turn nasty, that condition can lead to a hang so don't permit it */
		GTMSECSHR_REL_DBG_PRIV;
	}
	return status;
}
