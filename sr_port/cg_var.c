/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include <rtnhdr.h>
#include "obj_file.h"
#include "cg_var.h"
#include "stringpool.h"
#include "hashtab_mname.h"

GBLREF spdesc stringpool;

void cg_var(mtreenode *node, void *var_tabent_arg)
{	/* Copy mident with variable name to variable table entry */
	var_tabent **p = var_tabent_arg;

	assert(stringpool.base <= (unsigned char *)node->var.mvname.addr && (unsigned char *)node->var.mvname.addr < stringpool.top);
	(*p)[node->var.mvidx].var_name = node->var.mvname;
	COMPUTE_HASH_MNAME(&((*p)[node->var.mvidx]));
	(*p)[node->var.mvidx].var_name.addr = (char *)(node->var.mvname.addr - (char *)stringpool.base);
	(*p)[node->var.mvidx].marked = FALSE;
}
