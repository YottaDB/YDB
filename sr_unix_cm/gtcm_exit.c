/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

#include "gtm_stdlib.h"		/* for exit() */

#include "error.h"
#include "gtcm.h"

#ifdef GTCM_RC
#include "rc.h"
#endif /* defined(GTCM_RC) */

#include "gv_rundown.h"		/* for gv_rundown() prototype */
#include "op.h"			/* for op_unlock() and op_lkinit() prototype */

GBLREF int4	 gtcm_exi_condition;

void gtcm_exit()
{
	op_lkinit();
	op_unlock();
	SET_PROCESS_EXITING_TRUE;
	gv_rundown();
#ifdef GTCM_RC
	rc_delete_cpt();
	rc_rundown();
#endif
	exit(gtcm_exi_condition);
}
