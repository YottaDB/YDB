/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#ifndef SORTS_AFTER_H_INCLUDED
#define SORTS_AFTER_H_INCLUDED

#include "gtm_string.h"
#include "error.h"
#include "mmemory.h"
#include "numcmp.h"
#include "compiler.h"
#include "collseq.h"

/* The following SORTS_AFTER macro is there to avoid overhead of a "sorts_after()" function call
 * from performance sensitive callers like "op_sortsafter_retbool.c".
 *
 *	Determines the relative sorting order of two mval's.
 *	Uses an alternate local collation sequence if present.
 *
 *	Returns:
 *		> 0  :  lhs  ]] rhs (lhs ]] rhs is true)
 *		  0  :  lhs  =  rhs (lhs ]] rhs is false)
 *		< 0  :  lha ']] rhs (lhs ]] rhs is false)
 */
#define	SORTS_AFTER(lhs, rhs, result)												\
{																\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	for ( ; ; )	/* have a dummy for loop to be able to use "break" for various codepaths below */			\
	{															\
		assert(!MV_IS_SQLNULL(lhs));	/* caller should have ensured this */						\
		assert(!MV_IS_SQLNULL(rhs));	/* caller should have ensured this */						\
		if (!TREF(local_coll_nums_as_strings))										\
		{	/* If numbers collate normally (ahead of strings), check if either of the operands is a number */	\
			MV_FORCE_DEFINED(lhs);											\
			MV_FORCE_DEFINED(rhs);											\
			if (MV_IS_CANONICAL(lhs))										\
			{	/* lhs is a number */										\
				if (MV_IS_CANONICAL(rhs))									\
				{	/* Both lhs and rhs are numbers */							\
					NUMCMP_SKIP_SQLNULL_CHECK(lhs, rhs, result);						\
					break;											\
				}												\
				/* lhs is a number, but rhs is a string; return false unless rhs is null */			\
				result = (0 == rhs->str.len) ? 1 : -1;								\
				break;												\
			}													\
			/* lhs is a string */											\
			if (MV_IS_CANONICAL(rhs))										\
			{	/* lhs is a string, but rhs is a number; return true unless lhs is null */			\
				result = (0 != lhs->str.len) ? 1 : -1;								\
				break;												\
			}													\
		}														\
		/* In case either lhs or rhs is not of type MV_STR, force them to be, as we are only doing string		\
		 * comparisons beyond this point.										\
		 */														\
		MV_FORCE_STR(lhs);												\
		MV_FORCE_STR(rhs);												\
		/* At this point, we are guaranteed both lhs and rhs do not contain $ZYSQLNULL					\
		 * so we can safely go ahead with collation transformations.							\
		 */														\
		if (TREF(local_collseq))											\
			result = sorts_after_lcl_colseq(lhs, rhs);								\
		else														\
		{	/* Do a regular string comparison if no collation options are specified */				\
			MEMVCMP(lhs->str.addr, lhs->str.len, rhs->str.addr, rhs->str.len, result);				\
		}														\
		break;														\
	}															\
}

/* The function is also made available (inspite of the above macro) for callers that are not performance sensitive.
 * The rest of callers use the above SORTS_AFTER macro to avoid the overhead of a function call.
 */
long sorts_after(mval *lhs, mval *rhs);

/* The below is a helper function used by the SORTS_AFTER macro. Needed because that code uses ESTABLISH_RET macro
 * and therefore cannot be used inside the SORTS_AFTER macro directly.
 */
long sorts_after_lcl_colseq(mval *lhs, mval *rhs);

#endif
