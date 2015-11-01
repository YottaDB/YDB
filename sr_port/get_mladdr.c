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
#include "cmd_qlf.h"
#include "mmemory.h"
#include "gtm_caseconv.h"

GBLREF mlabel *mlabtab;
GBLREF command_qualifier cmd_qlf;

mlabel *get_mladdr(mident *c)
{
	mident	cap_ident;
	char	*ch, *tmp;
	mlabel	**p;
	int4	x;

	ch = c->c;
	if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
	{	lower_to_upper((uchar_ptr_t)&cap_ident.c[0], (uchar_ptr_t)ch, sizeof(mident));
		ch = &cap_ident.c[0];
	}
	for (p = &mlabtab; *p; )
	{
		if ((x = memcmp((*p)->mvname.c,ch,sizeof(mident) / sizeof(char))) < 0)
			p = &((*p)->rson);
		else if (x > 0)
			p = &((*p)->lson);
		else
			return *p;
	}
	*p = (mlabel *) mcalloc((unsigned int) sizeof(mlabel));
	memcpy((*p)->mvname.c, ch, sizeof(mident));
	assert(!(*p)->lson && !(*p)->rson);
	(*p)->formalcnt = NO_FORMALLIST;
	(*p)->gbl = TRUE;
	return *p;
}
