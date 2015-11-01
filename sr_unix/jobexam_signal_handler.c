/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include <netinet/in.h>

#include "gtm_stdio.h"
#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "jobexam_signal_handler.h"

GBLREF	uint4			process_id;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	gtmImageName		gtmImageNames[];

void jobexam_signal_handler(int sig, siginfo_t *info, void *context)
{
	siginfo_t	exi_siginfo;
	gtmsiginfo_t	signal_info;
#if defined(__osf__) || defined(_AIX) || defined(Linux390)
	struct sigcontext exi_context;
#else
	ucontext_t	exi_context;
#endif

	error_def(ERR_KILLBYSIG);
	error_def(ERR_KILLBYSIGUINFO);
	error_def(ERR_KILLBYSIGSINFO1);
	error_def(ERR_KILLBYSIGSINFO2);

	if (NULL != info)
		exi_siginfo = *info;
	else
		memset(&exi_siginfo, 0, sizeof(*info));
	if (NULL != context)
	{
#if defined(__osf__) || defined(_AIX) || defined(Linux390)
		exi_context = *(struct sigcontext *)context;
#else
		exi_context = *(ucontext_t *)context;
#endif
	} else
		memset(&exi_context, 0, sizeof(exi_context));
	extract_signal_info(sig, &exi_siginfo, &exi_context, &signal_info);
	switch(signal_info.infotype)
	{
		case GTMSIGINFO_NONE:
			rts_error(VARLSTCNT(6) ERR_KILLBYSIG, 4, GTMIMAGENAMETXT(image_type), process_id, sig);
			break;
		case GTMSIGINFO_USER:
			rts_error(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.send_pid, signal_info.send_uid);
			break;
		case GTMSIGINFO_ILOC + GTMSIGINFO_BADR:
			rts_error(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.int_iadr, signal_info.bad_vadr);
			break;
		case GTMSIGINFO_ILOC:
			rts_error(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
				 process_id, sig, signal_info.int_iadr);
			break;
		default:
			GTMASSERT;
	}
}
