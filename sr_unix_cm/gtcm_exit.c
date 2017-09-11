/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "gtm_stdlib.h"		/* for EXIT() */

#include "error.h"
#include "gtcm.h"

#ifdef GTCM_RC
#include "rc.h"
#endif /* defined(GTCM_RC) */

#include "gv_rundown.h"		/* for gv_rundown() prototype */
#include "op.h"			/* for op_unlock() and op_lkinit() prototype */
#include "gtmcrypt.h"

GBLREF int4	 gtcm_exi_condition;

void gtcm_exit()
{
	op_unlock();
	SET_PROCESS_EXITING_TRUE;
	gv_rundown();
#ifdef GTCM_RC
	rc_delete_cpt();
	rc_rundown();
#endif
	GTMCRYPT_CLOSE;
	EXIT(gtcm_exi_condition);
}
