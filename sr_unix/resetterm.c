/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
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
#include "eintr_wrappers.h"
#include "setterm.h"
#include "gtm_isanlp.h"

GBLREF	uint4		process_id;

error_def(ERR_TCSETATTR);

void  resetterm(io_desc *iod)
{
	int		status;
	int		save_errno;
	struct termios 	t;
	d_tt_struct	*ttptr;

	ttptr = (d_tt_struct *) iod->dev_sp;
	if (process_id != ttptr->setterm_done_by)
		return;	/* "resetterm" already done */
	if (ttptr->ttio_struct)
	{
	        t = *ttptr->ttio_struct;
		Tcsetattr(ttptr->fildes, TCSANOW, &t, status, save_errno);
		if (0 != status)
		{
			assert(-1 == status);
			assert(ENOTTY != save_errno);
			/* Skip TCSETATTR error for ENOTTY (in case fildes is no longer a terminal) */
			if ((ENOTTY != save_errno) && (0 == gtm_isanlp(ttptr->fildes)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TCSETATTR, 1, ttptr->fildes, save_errno);
		}
		ttptr->setterm_done_by = 0;
	}
	return;
}
