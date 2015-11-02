/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* jobexam signal processor:

   a very simplistic stripped down version of generic_signal_handler(). It is
   only intended to handle terminal errors during execution of the $zjobexam
   function. These are currently SIGSEGV and SIGBUS. On receipt, we use
   rts error to drive the appropriate message and let the condition handler
   take care of it.

 */

#include "mdef.h"
#include "gtm_string.h"

#include <signal.h>
#include "gtm_inet.h"

#include "gtm_stdio.h"
#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "jobexam_signal_handler.h"
#include "send_msg.h"
#include "gtmmsg.h"

GBLREF	uint4			process_id;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		need_core;

DEBUG_ONLY(GBLREF boolean_t ok_to_UNWIND_in_exit_handling;)

LITREF	gtmImageName		gtmImageNames[];

void jobexam_signal_handler(int sig, siginfo_t *info, void *context)
{
	siginfo_t	exi_siginfo;
	gtmsiginfo_t	signal_info;
	gtm_sigcontext_t exi_context, *context_ptr;

	error_def(ERR_KILLBYSIG);
	error_def(ERR_KILLBYSIGUINFO);
	error_def(ERR_KILLBYSIGSINFO1);
	error_def(ERR_KILLBYSIGSINFO2);
	error_def(ERR_KILLBYSIGSINFO3);
	error_def(ERR_JOBEXAMFAIL);

	if (NULL != info)
		exi_siginfo = *info;
	else
		memset(&exi_siginfo, 0, SIZEOF(*info));
#if defined(__ia64) && defined(__hpux)
        context_ptr = (gtm_sigcontext_t *)context;	/* no way to make a copy of the context */
	memset(&exi_context, 0, SIZEOF(exi_context));
#else
	if (NULL != context)
		exi_context = *(gtm_sigcontext_t *)context;
	else
		memset(&exi_context, 0, SIZEOF(exi_context));
	context_ptr = &exi_context;
#endif
	extract_signal_info(sig, &exi_siginfo, context_ptr, &signal_info);
	switch(signal_info.infotype)
	{
		case GTMSIGINFO_NONE:
			send_msg(VARLSTCNT(6) ERR_KILLBYSIG, 4, GTMIMAGENAMETXT(image_type), process_id, sig);
			gtm_putmsg(VARLSTCNT(6) ERR_KILLBYSIG, 4, GTMIMAGENAMETXT(image_type), process_id, sig);
			break;
		case GTMSIGINFO_USER:
			send_msg(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.send_pid, signal_info.send_uid);
			gtm_putmsg(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.send_pid, signal_info.send_uid);
			break;
		case GTMSIGINFO_ILOC + GTMSIGINFO_BADR:
			send_msg(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.int_iadr, signal_info.bad_vadr);
			gtm_putmsg(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.int_iadr, signal_info.bad_vadr);
			break;
		case GTMSIGINFO_ILOC:
			send_msg(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.int_iadr);
			gtm_putmsg(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.int_iadr);
			break;
		case GTMSIGINFO_BADR:
			send_msg(VARLSTCNT(7) ERR_KILLBYSIGSINFO3, 5, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.bad_vadr);
			gtm_putmsg(VARLSTCNT(7) ERR_KILLBYSIGSINFO3, 5, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.bad_vadr);
			break;
		default:
			GTMASSERT;
	}
	send_msg(VARLSTCNT(3) ERR_JOBEXAMFAIL, 1, process_id);
	/* Create a core to examine later. Note this handler is only enabled for two fatal core types so we don't
	 * do any futher checking in this regard.
	 */
	need_core = TRUE;
	gtm_fork_n_core();
	/* Note this routine do NOT invoke create_fatal_error_zshow_dmp() because it would in turn call jobexam
	 * again which would loop us right back around to here. We basically want to do UNWIND(NULL, NULL) logic
	 * but the UNWIND macro can only be used in a condition handler so next is a block that pretends it is
	 * our condition handler and does the needful.
	 */
	{	/* Needs new block since START_CH declares a new var used in UNWIND() */
		int arg = 0;	/* Needed for START_CH macro if debugging enabled */
		START_CH;
		DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE);
		UNWIND(NULL, NULL);
	}
}
