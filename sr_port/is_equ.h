/****************************************************************
 *								*
 * Copyright (c) 2020-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IS_EQU_INCLUDED
#define IS_EQU_INCLUDED

#include "gtm_string.h"

/* This is kept as a macro to avoid overhead of a "is_equ()" function call from performance sensitive
 * callers like "op_equ_retbool.c". The macro has a 4th parameter "t" which is TRUE or FALSE depending on
 * the caller context and has code to return a result or the negation of that result accordingly. This
 * is done again for performance reasons to avoid having just one version of the macro and later do the
 * negation of the result in the caller. That way, this macro can be invoked by both IS_EQU and IS_NEQU
 * without code duplication and yet maintain the performance advantage (by a few instructions).
 */
#define	IS_EQU_COMMON(u, v, result, t)												\
{																\
	int	land, lor, utyp, vtyp;												\
																\
	utyp = u->mvtype;													\
	vtyp = v->mvtype;													\
	land = utyp & vtyp;													\
	lor = utyp | vtyp;													\
	for ( ; ; )	/* have a dummy for loop to be able to use "break" for various codepaths below */			\
	{															\
		if ((land & MV_NM) != 0 && (lor & MV_NUM_APPROX) == 0)								\
		{														\
			/* at this point, the mval's are both exact numbers, we can do a numeric comparison */			\
			if (land & MV_INT)	/* If they are both integers, compare only the relevant cells */		\
			{													\
				result = t ? (u->m[1] == v->m[1]) : (u->m[1] != v->m[1]);					\
				break;												\
			}													\
			if (lor & MV_INT)											\
			{	/* If one is an integer and the other is not, the two values cannot be equal */			\
				assert((vtyp & MV_INT) ? (u->m[1] || u->m[0]) : (v->m[1] || v->m[0]));				\
					/* above line asserts that the zero value must be INT */				\
				result = t ? 0 : 1;										\
				break;												\
			}													\
			/* They are both decimal floating numbers, do a full comparison */					\
			result = t												\
				? ((((mval_b *)u)->sgne == ((mval_b *)v)->sgne) && (u->m[1] == v->m[1]) && (u->m[0] == v->m[0]))\
				: ((((mval_b *)u)->sgne != ((mval_b *)v)->sgne) || (u->m[1] != v->m[1]) || (u->m[0] != v->m[0]));\
			break;													\
		}														\
		/* At least one of the numbers is not in numeric form or is not a canonical number, do a string compare */	\
		MV_FORCE_STR(u);												\
		MV_FORCE_STR(v);												\
		if ((u->str.len != v->str.len)											\
				|| (u->str.len && (u->str.addr != v->str.addr) && memcmp(u->str.addr, v->str.addr, u->str.len)))\
			result = t ? 0 : 1;											\
		else														\
			result = t ? 1 : 0;											\
		break;														\
	}															\
}

#define	IS_EQU(u, v, result)	IS_EQU_COMMON(u, v, result, TRUE)
#define	IS_NEQU(u, v, result)	IS_EQU_COMMON(u, v, result, FALSE)

/* The function is also made available (inspite of the above macro) for callers that are not performance sensitive.
 * The rest of callers use the above IS_EQU macro to avoid the overhead of a function call.
 */
boolean_t is_equ(mval *u, mval *v);

#endif /* IS_EQU_INCLUDED */
