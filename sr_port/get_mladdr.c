/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

GBLREF mlabel 			*mlabtab;
GBLREF command_qualifier 	cmd_qlf;

mlabel *get_mladdr(mident *lab_name)
{
	mident_fixed	upper_ident;
	mident		*lname, upper_lname;
	mlabel		**p;
	int4		x;
	mstr		lab_str;

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
		if (x < 0)
			p = &((*p)->rson);
		else if (x > 0)
			p = &((*p)->lson);
		else
			return *p;
	}
	lab_str.len = lname->len;
	lab_str.addr = lname->addr;
	s2pool_align(&lab_str);
	*p = (mlabel *) mcalloc(SIZEOF(mlabel));
	(*p)->mvname.len = lab_str.len;
	(*p)->mvname.addr = lab_str.addr;
	assert(!(*p)->lson && !(*p)->rson);
	(*p)->formalcnt = NO_FORMALLIST;
	(*p)->gbl = TRUE;
	return *p;
}
