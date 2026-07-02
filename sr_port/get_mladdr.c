/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
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
#include "cmd_qlf.h"
#include "mmemory.h"
#include "gtm_caseconv.h"
#include "min_max.h"
#include "stringpool.h"
#include "gcol_list.h"

GBLREF mlabel 			*mlabtab;
GBLREF command_qualifier 	cmd_qlf;

mlabel *get_mladdr(mident *lab_name)
{
	mident_fixed	upper_ident;
	mident		*lname, upper_lname;
	mlabel		**p;
	int4		x;

	lname = lab_name;
	if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
	{
		lower_to_upper((uchar_ptr_t)&upper_ident.c[0], (uchar_ptr_t)lab_name->addr, lab_name->len);
		upper_lname.len = lab_name->len;
		upper_lname.addr = &upper_ident.c[0];
		lname = &upper_lname;
	}
	for (p = &mlabtab; *p; )
	{
		MIDENT_CMP(&(*p)->mvname, lname, x);
		if (0 > x)
			p = &((*p)->rson);
		else if (0 < x)
			p = &((*p)->lson);
		else
			return *p;
	}
	*p = (mlabel *)mcalloc(SIZEOF(mlabel));
	glist_first_init_str(&(*p)->mvname);
	glist_protect_str(&(*p)->mvname);
	(*p)->mvname.umstr = *lname;
	s2pool(&(*p)->mvname);
	assert(!(*p)->lson && !(*p)->rson);
	(*p)->formalcnt = NO_FORMALLIST;
	(*p)->gbl = TRUE;
	return *p;
}
