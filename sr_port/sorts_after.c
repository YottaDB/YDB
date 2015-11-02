/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "error.h"
#include "collseq.h"
#include "mmemory.h"
#include "do_xform.h"
#include "numcmp.h"
#include "sorts_after.h"
#include "gtm_maxstr.h"

long	sorts_after (mval *lhs, mval *rhs)
{
	int	cmp;
	int	length1;
	int	length2;
	mstr	tmstr1;
	mstr	tmstr2;
	MAXSTR_BUFF_DECL(tmp)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!TREF(local_coll_nums_as_strings))
	{	/* If numbers collate normally (ahead of strings), check if either of the operands is a number */
		if (MV_IS_CANONICAL(lhs))
		{	/* lhs is a number */
			if (MV_IS_CANONICAL(rhs))
			{	/* Both lhs and rhs are numbers */
				return numcmp(lhs, rhs);
			}
			/* lhs is a number, but rhs is a string; return false unless rhs is null */
			return (0 == rhs->str.len) ? 1 : -1;
		}
		/* lhs is a string */
		if (MV_IS_CANONICAL(rhs))
		{	/* lhs is a string, but rhs is a number; return true unless lhs is null */
			return (0 != lhs->str.len) ? 1 : -1;
		}
	}
	/* In case either lhs or rhs is not of type MV_STR, force them to be, as we are only doing string
	 * comparisons beyond this point.
	 */
	MV_FORCE_STR(lhs);
	MV_FORCE_STR(rhs);
	if (TREF(local_collseq))
	{
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
		return cmp != 0 ? cmp : length1 - length2;
	}
	/* Do a regular string comparison if no collation options are specified */
	return memvcmp(lhs->str.addr, lhs->str.len, rhs->str.addr, rhs->str.len);
}
