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
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"

int expratom(oprtype *a)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch(TREF(window_token))
	{
	case TK_IDENT:
	case TK_CIRCUMFLEX:
	case TK_ATSIGN:
		return glvn(a);
	default:
		return expritem(a);
	}
}
