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

/* This function is supplied for those microprocessors that are especially lacking
   in registers.  Wherever possible, the frame_pointer value is kept in a register
   in generated code; where not possible, it is necessary to obtain the current
   value from the external variable.  Because the external variable is linked into
   the GT.M executable rather than into generated code, it is better to obtain
   the value from a function called via the transfer table in order to avoid the
   possibility of the location of frame_pointer changing between releases of the
   GT.M executable.  Otherwise, customers on machines with fewer registers (e.g.,
   the Intel 80x86 series) would have to recompile all of their programs with
   every new release of GT.M.  */

#include "mdef.h"
#include <rtnhdr.h>
#include "stack_frame.h"

GBLREF	stack_frame	*frame_pointer;

stack_frame *op_get_msf (void)
{
	return frame_pointer;
}
