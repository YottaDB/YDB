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

/*
 *	SORTS_AFTER.C
 *
 *	Determine the relative sorting order of two mval's.
 *	Uses an alternate local collation sequence if present.
 *
 *	Returns:
 *		> 0  :  lhs  ]] rhs (lhs ]] rhs is true)
 *		  0  :  lhs  =  rhs (lhs ]] rhs is false)
 *		< 0  :  lha ']] rhs (lhs ]] rhs is false)
 */

#include "mdef.h"

#include "gtm_string.h"

#include "collseq.h"
#include "hashdef.h"
#include "lv_val.h"
#include "mmemory.h"
#include "do_xform.h"
#include "numcmp.h"
#include "sorts_after.h"

GBLREF collseq		*local_collseq;
GBLREF char		*lcl_coll_xform_buff;


int	sorts_after (mval *lhs, mval *rhs)
{
	if (local_collseq)
	{
		int	cmp;
		int	length1;
		int	length2;
		char	tmp[MAX_LCL_COLL_XFORM_BUFSIZ];
		mstr	tmstr1;
		mstr	tmstr2;

		tmstr1.len = MAX_LCL_COLL_XFORM_BUFSIZ;
		tmstr1.addr = lcl_coll_xform_buff;
		assert(NULL != lcl_coll_xform_buff);
		do_xform(local_collseq->xform, &lhs->str, &tmstr1, &length1);

		tmstr2.len = MAX_LCL_COLL_XFORM_BUFSIZ;
		tmstr2.addr = tmp;
		do_xform(local_collseq->xform, &rhs->str, &tmstr2, &length2);

		cmp = memcmp(tmstr1.addr, tmstr2.addr, length1 <= length2 ? length1 : length2);
		return cmp != 0 ? cmp : length1 - length2;
	}

	if (nm_iscan(lhs) != 0)
	{
		/* lhs is a number */
		if (nm_iscan(rhs) != 0)
			/* Both lhs and rhs are numbers */
			return numcmp(lhs, rhs);

		/* lhs is a number, but rhs is a string;
		   return false unless rhs is null */
		if (rhs->str.len == 0)
			return 1;
		return -1;
	}

	/* lhs is a string */
	if (nm_iscan(rhs) != 0)
	{
		/* lhs is a string, but rhs is a number;
		   return true unless lhs is null */
		if (lhs->str.len == 0)
			return -1;
		return 1;
	}

	/* lhs and rhs are both strings */
	return memvcmp(lhs->str.addr, lhs->str.len, rhs->str.addr, rhs->str.len);

}
