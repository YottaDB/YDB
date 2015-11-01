/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

 /* Add a cntrl-C handler for mupip upgrade */

#include "mdef.h"
#include "mu_upgrd.h"
#include "mu_upgrd_sig_init.h"

#include <signal.h>

void	 mu_upgrd_sig_init(void)
{
	struct sigaction 	act;

	/* for mupip upgrade, we need special ctrl-C handler */
	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_handler = mu_upgrd_ctrlc_handler;
	sigaction(SIGINT, &act, 0);
}
