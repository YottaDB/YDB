/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Perform necessary functions for signal handling that was deferred */
#include "mdef.h"

#include "gtm_stdlib.h"		/* for exit() */

#include <signal.h>

#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "send_msg.h"
#include "have_crit.h"
#include "deferred_signal_handler.h"
#include "gtmmsg.h"

GBLREF	VSIG_ATOMIC_T		forced_exit;
GBLREF	int4			exi_condition;
GBLREF	void			(*call_on_signal)();
GBLREF	int			forced_exit_err;
GBLREF	uint4			process_id;
GBLREF	gtmsiginfo_t		signal_info;
GBLREF	gtmImageName		gtmImageNames[];
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		exit_handler_active;

void deferred_signal_handler(void)
{
	void (*signal_routine)();

	error_def(ERR_KILLBYSIG);
	error_def(ERR_KILLBYSIGUINFO);
	error_def(ERR_KILLBYSIGSINFO1);
	error_def(ERR_KILLBYSIGSINFO2);

	/* To avoid nested calls to this routine, we set forced_exit to FALSE at the very beginning */
	forced_exit = FALSE;

	if (exit_handler_active)
	{
		assert(FALSE);	/* at this point in time (June 2003) there is no way we know of to get here, hence the assert */
		return;	/* since anyway we are exiting currently, resume exit handling instead of reissuing another one */
	}
	/* For signals that get a delayed response so we can get out of crit, we also delay the messages.
	 * This routine will output those delayed messages from the appropriate structures to both the
	 * user and the system console.
	 */
	/* note can't use switch here because ERR_xxx are not defined as constants */
	if (ERR_KILLBYSIG == forced_exit_err)
	{
		send_msg(VARLSTCNT(6) ERR_KILLBYSIG, 4, GTMIMAGENAMETXT(image_type), process_id, signal_info.signal);
		gtm_putmsg(VARLSTCNT(6) ERR_KILLBYSIG, 4, GTMIMAGENAMETXT(image_type), process_id, signal_info.signal);
	} else if (ERR_KILLBYSIGUINFO == forced_exit_err)
	{
		send_msg(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type), process_id,
						signal_info.signal, signal_info.send_pid, signal_info.send_uid);
		gtm_putmsg(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type), process_id,
						signal_info.signal, signal_info.send_pid, signal_info.send_uid);
	} else if (ERR_KILLBYSIGSINFO1 == forced_exit_err)
	{
		send_msg(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
			 process_id, signal_info.signal, signal_info.int_iadr, signal_info.bad_vadr);
		gtm_putmsg(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
			   process_id, signal_info.signal, signal_info.int_iadr, signal_info.bad_vadr);
	} else if (ERR_KILLBYSIGSINFO2 == forced_exit_err)
	{
		send_msg(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
			 process_id, signal_info.signal, signal_info.int_iadr);
		gtm_putmsg(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
			   process_id, signal_info.signal, signal_info.int_iadr);
	} else
	{
		send_msg(VARLSTCNT(1) forced_exit_err);
		gtm_putmsg(VARLSTCNT(1) forced_exit_err);
	}
	/* As on VMS, a mupip stop does not drive the condition handlers unless we are in crit */
	if ((0 != have_crit(CRIT_HAVE_ANY_REG) || SIGTERM != exi_condition) && CHANDLER_EXISTS)
		DRIVECH(0);
	/* If a special routine was registered to be driven on a signal, drive it now */
	if (0 != exi_condition && call_on_signal)
	{
		signal_routine = call_on_signal;
		call_on_signal = NULL;		/* So we don't recursively call ourselves */
		(*signal_routine)();
	}
	/* If the condition handler didn't cause an exit, drive the defined exit handler */
	exit(-exi_condition);
}
