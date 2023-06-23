/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <signal.h>
#include "gtm_limits.h"

#include "io.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "crit_wake.h"
#include "send_msg.h"
#include "have_crit.h"

error_def(ERR_NOSUCHPROC);

int crit_wake (sm_uint_ptr_t pid)
{
	if (0 == kill(*pid, SIGALRM))
		return 0;
	else if (ESRCH == errno)
	{
		send_msg(VARLSTCNT(5) ERR_NOSUCHPROC, 3, *pid, RTS_ERROR_LITERAL("wake"));
		return(ESRCH);
	} else
		assert(EINVAL != errno);
#	ifdef NOT_CURRENTLY_USED
	/* The code segment below basically disabled M LOCK wakeup via gtmsecshr (via send_mesg2gtmsecshr() call below). The
	 * requisite gtmsecshr support is temporarily also being #ifdef'd out until the M LOCK wakeup mechanism used to wakeup
	 * processes with different userids can be run outside of crit. At that point, this code will be re-enabled without
	 * the crit-check below once again allowing improved wakeup times for differing userids without waiting for a 100ms
	 * poll timer to expire.
	 */
	/* if you are in crit don't send, the other process's timer will wake it up any way */
	if (0 != have_crit(CRIT_HAVE_ANY_REG))
		return 0;
	return send_mesg2gtmsecshr(WAKE_MESSAGE, *pid, (char *)NULL, 0);
#	else
	return 0;
#	endif
}
