/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Perform necessary functions for signal handling that was deferred.
   Based on deferred_signal_handler() but without references to have_crit()
   that pull in half of the GT.M world.
*/
#include "mdef.h"

#include "gtm_stdlib.h"		/* for exit() */
#include <signal.h>

#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblkops.h"
#include "have_crit.h"
#include "dbcertify.h"

GBLREF	int4			exi_condition;
GBLREF	int			forced_exit_err;
GBLREF	uint4			process_id;
GBLREF	gtmsiginfo_t		signal_info;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		exit_handler_active;

LITREF	gtmImageName		gtmImageNames[];

error_def(ERR_KILLBYSIG);
error_def(ERR_KILLBYSIGUINFO);
error_def(ERR_KILLBYSIGSINFO1);
error_def(ERR_KILLBYSIGSINFO2);
error_def(ERR_KILLBYSIGSINFO3);

void dbcertify_deferred_signal_handler(void)
{
	/* To avoid nested calls to this routine, we advance the status of forced_exit. */
	SET_FORCED_EXIT_STATE_ALREADY_EXITING;

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
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_KILLBYSIG, 4,
			GTMIMAGENAMETXT(image_type), process_id, signal_info.signal);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_KILLBYSIG, 4,
			GTMIMAGENAMETXT(image_type), process_id, signal_info.signal);
	} else if (ERR_KILLBYSIGUINFO == forced_exit_err)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type), process_id,
			signal_info.signal, signal_info.send_pid, signal_info.send_uid);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type), process_id,
						signal_info.signal, signal_info.send_pid, signal_info.send_uid);
	} else if (ERR_KILLBYSIGSINFO1 == forced_exit_err)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
			 process_id, signal_info.signal, signal_info.int_iadr, signal_info.bad_vadr);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
			   process_id, signal_info.signal, signal_info.int_iadr, signal_info.bad_vadr);
	} else if (ERR_KILLBYSIGSINFO2 == forced_exit_err)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
			 process_id, signal_info.signal, signal_info.int_iadr);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
			   process_id, signal_info.signal, signal_info.int_iadr);
	} else if (ERR_KILLBYSIGSINFO3 == forced_exit_err)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_KILLBYSIGSINFO3, 5, GTMIMAGENAMETXT(image_type),
			 process_id, signal_info.signal, signal_info.bad_vadr);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_KILLBYSIGSINFO3, 5, GTMIMAGENAMETXT(image_type),
			   process_id, signal_info.signal, signal_info.bad_vadr);
	} else
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) forced_exit_err);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) forced_exit_err);
	}
	/* Drive the exit handler to terminate */
	exit(-exi_condition);
}
