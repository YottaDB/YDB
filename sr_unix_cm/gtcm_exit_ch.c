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
 *  gtcm_exit_ch.c ---
 *
 *	Exit handler for the GTCM code.
 *
 */

#include "mdef.h"

#include "error.h"
#include "gtcm.h"
#include "util.h"	/* for util_out_print() prototype */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF int4	 gtcm_exi_condition;

CONDITION_HANDLER(gtcm_exit_ch)
{
    PRN_ERROR;

    gtcm_exi_condition = SIGNAL;

    gtcm_exit();

}
