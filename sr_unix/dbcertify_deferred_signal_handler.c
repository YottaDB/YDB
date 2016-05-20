/****************************************************************
 *								*
 * Copyright (c) 2005-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_signal.h"

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
#include "forced_exit_err_display.h"

GBLREF	int4			exi_condition;
GBLREF	uint4			process_id;
GBLREF	gtmsiginfo_t		signal_info;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		exit_handler_active;

LITREF	gtmImageName		gtmImageNames[];

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
	forced_exit_err_display();
	/* Drive the exit handler to terminate */
	EXIT(-exi_condition);
}
