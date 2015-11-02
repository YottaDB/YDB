/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"

GBLREF triple *curtchain,pos_in_chain;

void ins_errtriple(int4 in_error)
{
	triple *x, *y, *triptr;

	x = pos_in_chain.exorder.bl;
	y = curtchain;
	if (!IS_STX_WARN(in_error))
	{
		dqdelchain(x,y,exorder);
		assert(pos_in_chain.exorder.bl->exorder.fl  == curtchain);
		assert(curtchain->exorder.bl == pos_in_chain.exorder.bl);
	}
	triptr = newtriple(OC_RTERROR);
	triptr->operand[0] = put_ilit(in_error);
	triptr->operand[1] = put_ilit(FALSE);	/* not a subroutine reference */
}
