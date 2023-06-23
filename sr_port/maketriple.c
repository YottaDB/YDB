/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "compiler.h"
#include "mdq.h"
#include "mmemory.h"

GBLREF int	 source_column;

triple *maketriple(opctype op)
{
	triple *x;
        DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;
	x = (triple *)mcalloc(SIZEOF(triple));
	x->opcode = op;
	x->src.line = TREF(source_line);
	x->src.column = source_column;
	dqinit(&(x->backptr), que);
	dqinit(&(x->jmplist), que);
	return x;
}
