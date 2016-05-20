/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"	/* needed for gtmsiginfo.h */

#include "gtmmsg.h"
#include "send_msg.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "forced_exit_err_display.h"

GBLREF	int			forced_exit_err;
GBLREF	gtmsiginfo_t		signal_info;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			process_id;

LITREF	gtmImageName		gtmImageNames[];

error_def(ERR_KILLBYSIG);
error_def(ERR_KILLBYSIGUINFO);
error_def(ERR_KILLBYSIGSINFO1);
error_def(ERR_KILLBYSIGSINFO2);
error_def(ERR_KILLBYSIGSINFO3);

void forced_exit_err_display(void)
{
	assert(forced_exit_err);
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
}
