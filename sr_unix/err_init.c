/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include <setjmp.h>
#include <errno.h>
#include "gtm_string.h"

#include "unistd.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#include "invocation_mode.h"
#include "gtmimagename.h"
#include "error.h"
#include "send_msg.h"
#include "have_crit.h"
#include "eintr_wrappers.h"

#define	COREDUMPFILTERFN	"/proc/%i/coredump_filter"
#define FILTERPARMSIZE		(8 + 2 + 1)		/* 8 bytes for number (normally 7), 2 bytes for 0x, and 1 null terminator */
#define FILTERENABLEBITS	0x0000073		/* Bits 0, 1, 4, 5, 6 - 32-bit can only handle 10 bytes total */

GBLREF enum gtmImageTypes	image_type;
GBLDEF char                    **gtmenvp;

error_def(ERR_SYSCALL);

/* 1. Allocate a basic initial condition handler stack that can be expanded later if necessary.
 * 2. On Linux, make sure bits 0,1, 4, 5, and 6 are set in /proc/PID/coredump_filter so dumps the sections that GT.M
 *    cores need to have in them.
 */
void err_init(void (*x)())
{
#	ifdef __linux__
	int		rc;
	unsigned int	filterbits;
	char		procfn[SIZEOF(COREDUMPFILTERFN) + MAX_DIGITS_IN_INT];	/* File name of file to update */
	char		filter[FILTERPARMSIZE], *filterend;			/* Value read in & written out */
	char		*rcc, *bytes_buf;
	FILE		*filterstrm;						/* filter stream file block */
#	endif
	chnd = (condition_handler *)malloc((CONDSTK_INITIAL_INCR + CONDSTK_RESERVE) * SIZEOF(condition_handler));
	chnd[0].ch_active = FALSE;
	chnd[0].save_active_ch = NULL;
	active_ch = ctxt = &chnd[0];
	ctxt->ch = x;
	chnd_end = &chnd[CONDSTK_INITIAL_INCR]; /* chnd_end is the end of the condition handler stack */
	chnd_incr = CONDSTK_INITIAL_INCR * 2;
#	ifdef __linux__
	/* Read the coredump_filter value from /proc for this process, update the value if necessary so we have the proper
	 * flags set to get the info we (and gtmpcat) need to properly process a core file. Note any errors we encounter just
	 * send a message to the operator log and return as nothing here should prevent GT.M from running.
	 *
	 * Note "man 5 core" on x86-64 Linux (Ubuntu 12.04) notes that the /proc/PID/coredump_filter file is only provided when
	 * the Linux kernel is built with the CONFIG_ELF_CORE configuration option. This *seems* to control whether or not the
	 * kernel supports the ELF loader or not. To date, all Linux flavors GT.M supports use ELF so we regard this as largely
	 * mandatory though in the future it may happen that GT.M works yet runs with something other than ELF. In that case,
	 * we'd need to change the below to avoid the operator log messages every time GT.M initializes.
	 *
	 * Note use simple basic methods since this early in initialization not everything is necessarily setup to
	 * be able to properly use the *print*() wrapper functions.
	 *
	 */
	bytes_buf = GETENV("gtm_coredump_filter");
	if ((NULL == bytes_buf) || (0 != strncmp("-1", bytes_buf, 3)))
	{

		rc = snprintf(procfn, SIZEOF(procfn), COREDUMPFILTERFN, getpid());
		if (0 > rc)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("sprintf()"), CALLFROM, rc);
			return;
		}
		if (NULL == bytes_buf)
		{
			/* If $gtm_coredump_filter is not defined, set the filter to provide a full dump  including huge pages */
			Fopen(filterstrm, procfn, "r");
			if (NULL == filterstrm)
			{
				rc = errno;
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("fopen()"), CALLFROM, rc);
				return;
			}
			rcc = fgets(filter, SIZEOF(filter), filterstrm);
			if (NULL == rcc)
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("fgets()"), CALLFROM, rc);
				return;
			}
			FCLOSE(filterstrm, rc);
			if (0 > rc)
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("fclose()"), CALLFROM, rc);
				return;
			}
			filterend = filter + SIZEOF(filter);
			filterbits = (unsigned int)strtol(filter, &filterend, 16);
			if (FILTERENABLEBITS != (filterbits & FILTERENABLEBITS))
			{	/* At least one flag was missing - reset them */
				filterbits = filterbits | FILTERENABLEBITS;
			}
			snprintf(filter, FILTERPARMSIZE, "0x%07x", filterbits);
			bytes_buf = filter;
		}
		Fopen(filterstrm, procfn, "w");
		if (NULL == filterstrm)
		{
			rc = errno;
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fopen()"),
				     CALLFROM, rc);
			return;
		}
		rc = fprintf(filterstrm, "%s", bytes_buf);
		if (0 > rc)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fprintf"),
				     CALLFROM, rc);
			return;
		}
		FCLOSE(filterstrm, rc);
		if (0 > rc)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fclose()"),
				     CALLFROM, rc);
			return;
		}

	}
#	endif
}
