/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_termios.h"
#include "gtm_signal.h"	/* for SIGPROCMASK used inside Tcsetattr */
#include "gtm_unistd.h"

#include "io.h"
#include "iottdef.h"
#include "gtmio.h"
#include "iott_setterm.h"
#include "eintr_wrappers.h"
#include "gtm_isanlp.h"
#include "svnames.h"
#include "util.h"
#include "op.h"
#include "send_msg.h"

GBLREF	uint4		process_id;
GBLREF	boolean_t	prin_in_dev_failure, prin_out_dev_failure;
GBLREF	boolean_t	exit_handler_active;

error_def(ERR_TCSETATTR);

void  iott_resetterm(io_desc *iod)
{
	int		status;
	int		save_errno;
	struct termios 	t;
	d_tt_struct	*ttptr;

	ttptr = (d_tt_struct *) iod->dev_sp;
	if (process_id != ttptr->setterm_done_by)
		return;	/* "iott_resetterm" already done */
	if (ttptr->ttio_struct)
	{
	        t = *ttptr->ttio_struct;
		Tcsetattr(ttptr->fildes, TCSANOW, &t, status, save_errno, CHANGE_TERM_FALSE);
		if (0 != status)
		{
			assert(-1 == status);
			assert(ENOTTY != save_errno);
			/* Skip TCSETATTR error for ENOTTY (in case fildes is no longer a terminal) */
			if ((ENOTTY != save_errno) && (0 == gtm_isanlp(ttptr->fildes)))
			{
				ISSUE_NOPRINCIO_BEFORE_RTS_ERROR_IF_APPROPRIATE(iod);	/* just like is done in "iott_use.c" */
				/* If we are already in the exit handler, do not issue an error for this event as this
				 * could cause a condition handler overrun (i.e. invoke "ch_overrun()") which would create
				 * a core file. Since this error most likely means the terminal has gone away, it is better
				 * to terminate the process without resetting a non-existent terminal than creating a core file.
				 */
				if (!exit_handler_active)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TCSETATTR, 1, ttptr->fildes, save_errno);
			}
		}
		ttptr->setterm_done_by = 0;
	}
	return;
}
