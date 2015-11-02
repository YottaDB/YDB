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
#include "compiler.h"

GBLREF triple *curtchain;

void tnxtarg(a)
oprtype *a;
{
	/*return a reference to the next triple to be produced */
	a->oprclass = TNXT_REF;
	a->oprval.tref = curtchain->exorder.bl;
	return;
}
