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
#include "mdef.h"
#include "cmidef.h"
#include "gtm_time.h"
#include "gtm_unistd.h"

#ifdef CMI_DEBUG
#include "gtm_stdio.h"
#endif

GBLREF struct NTD *ntd_root;

void cmj_handler(int signo, siginfo_t *info, void *context)
{
	struct CLB *lnk;
	sigset_t oset;

	CMI_DPRINT(("Enter cmj_handler signo = %d\n", signo));

	switch (signo) {
	case SIGIO:
		ntd_root->sigio_interrupt = TRUE;
		break;
	case SIGURG:
		ntd_root->sigurg_interrupt = TRUE;
		break;
	}
}
