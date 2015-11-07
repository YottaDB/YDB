/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "probe.h"

GBLDEF lock_sb 	vms_lock_list[MAX_VMS_LOCKS + 1];
GBLDEF int	vms_lock_tail;

/* see also gtm_enq */
uint4 gtm_enqw(
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
	int		index;
	uint4		status;
	uint4		prvadr[2], prvprv[2];

	GTMSECSHR_SET_DBG_PRIV(PRV$M_SYSLCK, status);
	if (SS$_NORMAL == status)
	{
		/* send the vms_lock_list element in place of the lsb
		 * to ensure that the list is up to date in case of a sudden loss of cabin pressure */
		if (!GTM_PROBE(SIZEOF(*lsb), (unsigned char *)lsb, READ))
			return SS$_ACCVIO;
		vms_lock_list[vms_lock_tail] = *lsb;
		status = sys$enqw(efn, lkmode, &vms_lock_list[vms_lock_tail], flags, resnam, parid,
				astadr, astprm, blkast, acmode, nullarg);
		*lsb = vms_lock_list[vms_lock_tail];
		if (SS$_NORMAL == status)
			status = lsb->cond;
		if (SS$_NORMAL == status)
		{
			if (!(flags & LCK$M_CONVERT))
			{
				if (vms_lock_tail < MAX_VMS_LOCKS)
					vms_lock_tail++;
				else	/* vms_lock_tail basically sticks at MAX_VMS_LOCKS once it hits it */
				{	/* the following looks for open slots to reuse */
					for (index = 0;  index < vms_lock_tail;  index++)
					{
						if (0 == vms_lock_list[index].lockid)
						{
							vms_lock_list[index] = *lsb;
							vms_lock_list[vms_lock_tail].lockid = 0;
							break;
						}
					}
					if (index >= vms_lock_tail)
					{	/* if no open slots, release the lock */
						(void)sys$deq(vms_lock_list[vms_lock_tail].lockid, 0, acmode, flags);
						/* the following should be interpretted by the caller as too many database files;
						 * borrowing the condition code avoids merrors, and "should" never happen,
						 * as GT.M only nests sub-locks to a depth of 1 */
						status = SS$_EXDEPTH;
					}
				}
			}
		} else
			vms_lock_list[vms_lock_tail].lockid = 0;
		GTMSECSHR_REL_DBG_PRIV;
	}
	return status;
}
