/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"
#include "term_setup.h"
#include "gtm_signal.h"
#include "sig_init.h"

GBLREF	boolean_t	ctrlc_on, hup_on;	/* TRUE in cenable mode; FALSE in nocenable mode */
GBLREF	int4		outofband;		/* enumerated: ctrap, ctrlc or ctrly*/
GBLREF	io_pair		io_std_device;		/* standard device */
GBLREF	void		(*ctrlc_handler_ptr)();

void  term_setup(boolean_t ctrlc_enable)
{
	struct sigaction	act;

	outofband = 0;
	ctrlc_on = (tt == io_std_device.in->type) ? ctrlc_enable : FALSE;
	if (hup_on && (tt == io_std_device.in->type))
	{	/* If $PRINCIPAL, enable the hup_handler - similar to iop_hupenable code in iott_use.c */
		if (!USING_ALTERNATE_SIGHANDLING)
		{
			sigemptyset(&act.sa_mask);
			act.sa_flags = YDB_SIGACTION_FLAGS;
			act.sa_sigaction = ctrlc_handler_ptr;
			sigaction(SIGHUP, &act, 0);
		} else
		{
			SET_ALTERNATE_SIGHANDLER(SIGHUP, &ydb_altmain_sighandler);
		}
	}
}
