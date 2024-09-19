/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "sorts_after.h"

#include "do_xform.h"
#include "gtm_maxstr.h"

long sorts_after_lcl_colseq(mval *lhs, mval *rhs)
{
	int		cmp;
	int		length1;
	int		length2;
	mstr		tmstr1;
	mstr		tmstr2;
	long		result;
	MAXSTR_BUFF_DECL(tmp)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Assert a few things first. Caller (currently only the macro SORTS_AFTER) should have ensured all of this. */
	assert(MV_IS_STRING(lhs));
	assert(MV_IS_STRING(rhs));
	assert(!MV_IS_SQLNULL(lhs));
	assert(!MV_IS_SQLNULL(rhs));

	ALLOC_XFORM_BUFF(lhs->str.len);
	tmstr1.len = TREF(max_lcl_coll_xform_bufsiz);
	tmstr1.addr = TREF(lcl_coll_xform_buff);
	assert(NULL != TREF(lcl_coll_xform_buff));
	do_xform(TREF(local_collseq), XFORM, &lhs->str, &tmstr1, &length1);
	MAXSTR_BUFF_INIT_RET;
	tmstr2.addr = tmp;
	tmstr2.len = MAXSTR_BUFF_ALLOC(rhs->str.len, tmstr2.addr, 0);
	do_xform(TREF(local_collseq), XFORM, &rhs->str, &tmstr2, &length2);
	MAXSTR_BUFF_FINI;
	cmp = memcmp(tmstr1.addr, tmstr2.addr, length1 <= length2 ? length1 : length2);
	result = ((0 != cmp) ? cmp : (length1 - length2));
	return result;
}

