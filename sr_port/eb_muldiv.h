/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef EB_MULDIV_H_INCLUDED
#define EB_MULDIV_H_INCLUDED

bool	eb_int_mul (int4 v1, int4 u1, int4 p[]);
int4	eb_mul (int4 v[], int4 u[], int4 p[]);	/* p = u*v */
bool	eb_mvint_div (int4 v, int4 u, int4 q[]);
bool	eb_int_div (int4 v1, int4 u1, int4 q[]);
int4	eb_div (int4 x[], int4 y[], int4 q[]);	/* q = y/x */
void	eb_int_mod (int4 v1, int4 u1, int4 p[]);

#define EB_DIV(V, U, Q, C)											\
{														\
	/* Check whether the divisor or the dividend are too big. The compiler would have set such mvals	\
	 * to "literal_numoflow" (which has an exponent of EXPHI) in that case. If so, issue a numeric		\
	 * overflow error here. The "eb_div()" function has issues dealing with such numbers (it would fail	\
	 * with a %YDB-F-SIGINTDIV error or asserts otherwise).							\
	 */													\
	if ((EXPHI <= V->e) || (EXPHI <= U->e))									\
		rts_error(VARLSTCNT(1) ERR_NUMOFLOW);								\
	C = eb_div(V->m, U->m, Q->m);										\
}

#endif
