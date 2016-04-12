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

#include <errno.h>
#include "mdef.h"
#include "cmidef.h"
#include "gtm_socket.h"

void cmj_exception_interrupt(struct CLB *lnk, int signo)
{
	int rval;

	if (lnk->mun == -1)
		return;
	if (signo == SIGURG)
	{
		while ((-1 == (rval = (int)recv(lnk->mun, (void *)&lnk->urgdata, 1, MSG_OOB))) && EINTR == errno)
			;
		/* test to see if there is ANY oob data */
		if (-1 == rval && (CMI_IO_WOULDBLOCK(errno) || errno == EINVAL))
			return;
		if (0 == rval || -1 == rval)
		{
			cmj_err(lnk, CMI_REASON_STATUS, (0 == rval ? ECONNRESET : errno));
			return;
		}
		/* flag urgent data */
		lnk->deferred_event = TRUE;
		lnk->deferred_reason = CMI_REASON_INTMSG;
		return;
	}
}
