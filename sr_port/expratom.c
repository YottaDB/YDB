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
#include "opcode.h"
#include "toktyp.h"

GBLREF char window_token;

int expratom(oprtype *a)
{

	switch(window_token)
	{
	case TK_IDENT:
	case TK_CIRCUMFLEX:
	case TK_ATSIGN:
		return glvn(a);
	default:
		return expritem(a);
	}
}
