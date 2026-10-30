/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

error_def(ERR_EXPRTOODEEP);

int expratom(oprtype *a)
{
	int	rval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	EXPR_NEST_DEPTH_INCREMENT_OR_RETURN(FALSE);
	switch(TREF(window_token))
	{
	case TK_IDENT:
	case TK_CIRCUMFLEX:
	case TK_ATSIGN:
		rval = glvn(a);
		break;
	default:
		rval = expritem(a);
		break;
	}
	EXPR_NEST_DEPTH_DECREMENT;
	return rval;
}
