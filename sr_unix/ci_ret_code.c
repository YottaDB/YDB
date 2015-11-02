/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "error.h"
#include "gtmci.h"
#include "op.h"
#include "error_trap.h"

GBLREF stack_frame 	*frame_pointer;
GBLREF unsigned char	*msp;
GBLREF int		mumps_status;

/* Routine executed at level 1 (SFF_CI frame) of the current gtm environment
 * to return control from M to gtm_ci(). the longjmp returns control to dm_start
 * which in turn returns to gtm_ci(). */
void ci_ret_code(void)
{
	assert(active_ch->ch == &mdb_condition_handler);
	longjmp(active_ch->jmp, -1);
}

/* Routine executed at level 0 from the bottom-most frame (base frame) of the
   current gtm environment - called when M routine does zgoto 0 */
void ci_ret_code_exit(void)
{
	assert(active_ch->ch == &mdb_condition_handler);
	ci_ret_code_quit();
	mumps_status = 0;
	longjmp(active_ch->jmp, -1);
}

/* Exit from the current Call-in environment */
void ci_ret_code_quit(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (frame_pointer->flags & SFF_CI)
		op_unwind();
	gtmci_isv_restore(); /* restore $ECODE/$STACK of previous level in the nested call-ins */
	op_unwind(); 	/* base frame of this call-in environment */

	(TREF(gtmci_nested_level))--;
	/* restore frame_pointer stored at msp (see base_frame.c) */
	frame_pointer = *(stack_frame**)msp;
	msp += SIZEOF(frame_pointer);
}
