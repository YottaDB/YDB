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

#include <iodef.h>
#include <efndef.h>
#include "cmihdr.h"
#include "cmidef.h"
#include "efn.h"

#define TIMER_FLAGS 0

GBLREF struct NTD *ntd_root;

uint4 cmi_close(struct CLB *lnk)
{
	uint4  efn_mask, status;
	static readonly int4	delta_1_sec[2] = { -10000000, -1 };
	struct CLB *previous;
        qio_iosb iosb;

	lnk->sta = CM_CLB_DISCONNECT;
	previous = RELQUE2PTR(lnk->cqe.fl);
	remqti(previous);
        efn_mask = (0x1 << efn_cmi_immed_wait | 0x1 << efn_2timer);

	/* DECnet OSI does not currently time out if the channel is gone, so protect it with our own timer */
        status = sys$setimr(efn_2timer, &delta_1_sec, 0, lnk, TIMER_FLAGS);
	if (status & 1)
	{
		/* If we can't get the timer (which we believe should be exceedingly rare), just blow it away to prevent a hang */

		/* First, request a disconnect (also transmits all pending messages before disconnecting).  */
                status = SYS$QIO(efn_cmi_immed_wait, lnk->dch, IO$_DEACCESS | IO$M_SYNCH, &iosb, 0, 0, 0, 0, 0, 0, 0, 0);
		/* Ignore previous status return because we're going to deassign the link regardless.  */

                status = sys$wflor(efn_2timer, efn_mask);
		/* Unless in test, ignore previous status return because we're going to deassign the link regardless.  */
                assert(status & 1);
	}else
		assert(0);	/* In testing trap the timer failure */

        sys$cantim(lnk, 0);     /* in case still running */
	/* abort the link in case the timer went off */
        status = SYS$QIOW(EFN$C_ENF, lnk->dch, IO$_DEACCESS | IO$M_ABORT, &iosb, 0, 0, 0, 0, 0, 0, 0, 0);
	/* Ignore previous status return because we're going to deassign the link regardless.  */
	status = SYS$CANCEL(lnk->dch);
	/* Ignore previous status for same reason.  */

	status = SYS$DASSGN(lnk->dch);
	if ((status & 1) == 0)
		return status;

	lib$free_vm(&SIZEOF(*lnk), &lnk, 0);
	return status;
}
