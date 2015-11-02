/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "error.h"			/* Declares all needed globals */

void gtm_asm_establish(void);		/* Only needs to be declared here as is only called from assembler routines */

/* This routine is called from assembler routines (basically dm_start) who need to do an ESTABLISH. We do all of the ESTABLISH
 * here except for the actual setjmp() call which needs to be in the assembler macro itself.
 */

void gtm_asm_establish(void)
{
	CHTRACEPOINT;
	ctxt++;
	if (ctxt >= (chnd_end + (!process_exiting ? 0 : CONDSTK_RESERVE)))
		condstk_expand();
	CHECKHIGHBOUND(ctxt);
	ctxt->save_active_ch = active_ch;
	ctxt->ch_active = FALSE;
	active_ch = ctxt;
}
