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

#include "mdef.h"
#include "opcode.h"
#include "compiler.h"

oprtype put_ilit(mint x)
{
	triple *ref;

	ref = newtriple(OC_ILIT);
	ref->operand[0].oprclass = ILIT_REF;
	ref->operand[0].oprval.ilit = x;
	return put_tref(ref);
}
