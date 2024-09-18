/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

#ifndef NUMCMP_INCLUDED
#define NUMCMP_INCLUDED

#include "promodemo.h"

long numcmp(mval *u, mval *v);

/* The main code of "numcmp.c" is kept here in this macro for performance reasons to skip a MV_IS_SQLNULL check
 * in case the caller has already done that (e.g. "op_get_retbool.c").
 */
#define	NUMCMP_SKIP_SQLNULL_CHECK(u, v, ret)									\
{														\
	mval		w;											\
	int		u_sgn, v_sgn, exp_diff, m1_diff, m0_diff;						\
	int		u_m0, v_m0, u_m1, v_m1;									\
	int		u_mvtype;										\
														\
	MV_FORCE_NUM(u);											\
	MV_FORCE_NUM(v);											\
	for ( ; ; )	/* have a dummy for loop to be able to use "break" for various codepaths below */	\
	{													\
		/* If both are integer representations, just compare m[1]'s.  */				\
		u_mvtype = u->mvtype & MV_INT;									\
		if (u_mvtype & v->mvtype)									\
		{												\
			u_m1 = u->m[1];										\
			v_m1 = v->m[1];										\
			if      (u_m1 >  v_m1)     ret = 1;							\
			else if (u_m1 == v_m1)     ret = 0;							\
			else /* (u_m1  < v_m1) */  ret = -1;							\
			break;											\
		}												\
		/* If not both integer, promote either one that might be. */					\
		if (u_mvtype)											\
		{												\
			w = *u ;										\
			promote(&w) ;										\
			u = &w ;										\
		} else if (v->mvtype & MV_INT)									\
		{												\
			w = *v ;										\
			promote(&w) ;										\
			v = &w ;										\
		}												\
		/* Compare signs.  */										\
		u_sgn = (0 == u->sgn) ? 1 : -1;	/* 1 if positive, -1 if negative */				\
		v_sgn = (0 == v->sgn) ? 1 : -1;									\
		if (u_sgn != v_sgn)										\
		{												\
			ret = u_sgn;										\
			break;											\
		}												\
		/* Signs equal; compare exponents for magnitude and adjust sense depending on sign.  */		\
		exp_diff = u->e - v->e;										\
		if (exp_diff)											\
		{												\
			if (0 > exp_diff)									\
				ret = -u_sgn;									\
			else											\
				ret = u_sgn;									\
			break;											\
		}												\
		/* Signs and exponents equal; compare magnitudes.  */						\
		/* First, compare high-order 9 digits of magnitude.  */						\
		u_m1 = u->m[1];											\
		v_m1 = v->m[1];											\
		m1_diff = u_m1 - v_m1;										\
		if (m1_diff)											\
		{												\
			if (0 > m1_diff)									\
				ret = -u_sgn;									\
			else											\
				ret = u_sgn;									\
			break;											\
		}												\
		/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */			\
		if (0 == u_m1)	/* zero special case */								\
		{												\
			ret = 0;										\
			break;											\
		}												\
		u_m0 = u->m[0];											\
		v_m0 = v->m[0];											\
		m0_diff = u_m0 - v_m0;										\
		if (m0_diff)											\
		{												\
			if (0 > m0_diff)									\
				ret = -u_sgn;									\
			else											\
				ret = u_sgn;									\
			break;											\
		}												\
		/* Signs, exponents, high-order magnitudes, and low-order magnitudes equal.  */			\
		ret = 0;											\
		break;												\
	}													\
}

#endif /* NUMCMP_INCLUDED */
