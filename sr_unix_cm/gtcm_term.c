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

/*
 *  gtcm_term.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include <sys/types.h>
#include <signal.h>

#include "mdef.h"
#include "gtcm.h"


int
gtcm_term(sig)
    int		 sig;
{
    extern int	 omi_exitp;

    omi_exitp = 1;

    return 0;

}
