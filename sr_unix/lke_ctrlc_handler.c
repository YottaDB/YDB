/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>

#include "mlkdef.h"
#include "error.h"
#include "cmidef.h"
#include "lke.h"
#include "have_crit.h"

GBLREF	VSIG_ATOMIC_T util_interrupt;

void lke_ctrlc_handler(int sig)
{
	util_interrupt = 1;
}
