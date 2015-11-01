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
#include "mdq.h"
#include "mmemory.h"

triple *maketriple(opctype op)
{
GBLREF short int source_line,source_column;
triple *x;
x = (triple *) mcalloc((unsigned int) sizeof(triple));
x->opcode = op;
x->src.line = source_line;
x->src.column = source_column;
dqinit(&(x->backptr),que);
dqinit(&(x->jmplist),que);
return x;
}
