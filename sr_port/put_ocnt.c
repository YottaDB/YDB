/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"

/* Parameter type OCNT_REF Offset from CALL to NEXT TRIPLE. Used by OC_CALL,
   OC_CALLSP, and OC_FORLCLDO. These triples generate a call to the routines
   by the same name but need a parameter that has the length of the jump
   sequence that the triple builds. That length is this parameter and is
   added to the return address and saved in the stackframe these routines
   build as the return point for when the M code these routines execute
   completes. They then return, hit the branch instruction, and branch to
   the subroutine. The subroutine runs, then returns via this saved address
   to the instruction following that jump instruction. But it all depends
   on us getting the right length for the generated code of the jump
   instruction. See emit_tip() and emit_call_xfer() in emit_code.c for
   more details.
*/
oprtype put_ocnt(void)
{
	oprtype a;

	a.oprclass = OCNT_REF;
	a.oprval.offset = 0;
	return a;
}
