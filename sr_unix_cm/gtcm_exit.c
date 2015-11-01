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
 *  gtcm_exit.c ---
 *
 *	Exit routine for the GTCM code.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "error.h"
#include "gtcm.h"

#ifdef GTCM_RC
#include "rc.h"
#endif /* defined(GTCM_RC) */

GBLREF int4	 gtcm_exi_condition;

void gtcm_exit()
{
	op_lkinit();
	op_unlock();
	gv_rundown();
#ifdef GTCM_RC
	rc_delete_cpt();
	rc_rundown();
#endif
	exit(gtcm_exi_condition);
}
