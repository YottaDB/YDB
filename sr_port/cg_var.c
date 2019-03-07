/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
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
#include "obj_file.h"
#include "cg_var.h"
#include "stringpool.h"
#include "hashtab_mname.h"

GBLREF spdesc stringpool;

void cg_var(mvar *v, var_tabent **p)
{	/* Copy mident with variable name to variable table entry */
	assert(stringpool.base <= (unsigned char *)v->mvname.addr && (unsigned char *)v->mvname.addr < stringpool.top);
	(*p)[v->mvidx].var_name = v->mvname;
	COMPUTE_HASH_MNAME(&((*p)[v->mvidx]));
	(*p)[v->mvidx].var_name.addr = (char *)(v->mvname.addr - (char *)stringpool.base);
	(*p)[v->mvidx].marked = FALSE;
}
