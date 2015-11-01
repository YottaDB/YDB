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
#include "promodemo.h"
#include "numcmp.h"

/*	numcmp compares two mvals.
 *
 *	entry:
 *		u, v	- pointers to mvals
 *
 *	exit:
 *		function value:
 *			  1, if u >  v
 *			  0, if u == v
 *			 -1, if u <  v
 *
 *	NOTE: Some of numcmp's callers depend on the value returned being one of (1, 0, -1),
 *	so it is not appropriate to return the difference of the two mval values.
 */

int4 numcmp (mval *u, mval *v)
{
	mval	w;
	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);

	/* If both are integer representations, just compare m[1]'s.  */
	if ((u->mvtype & MV_INT & v->mvtype)  !=  0)
	{
		if      (u->m[1] >  v->m[1])     return  1;
		else if (u->m[1] == v->m[1])     return  0;
		else /* (u->m[1]  < v->m[1]) */  return -1;
	}

	/* If not both integer, promote either one that might be.  */
	if ((u->mvtype & MV_INT)  !=  0)
	{
		w = *u ;
		promote(&w) ;
		u = &w ;
	}
	else if ((v->mvtype & MV_INT)  !=  0)
	{
		w = *v ;
		promote(&w) ;
		v = &w ;
	}

	/* Compare signs.  */
	if ((u->sgn != 0)  &&  (v->sgn == 0))	/* u <  0  and  v >= 0 */
		return -1;

	if ((u->sgn == 0)  &&  (v->sgn != 0))	/* u >= 0  and  v <  0 */
		return 1;

	/* Signs equal; compare exponents for magnitude and adjust sense depending on sign.  */
	if (u->e < v->e)
	{
		if (u->sgn == 0)	/* u,v >= 0 */
			return -1;
		else 			/* u,v <  0 */
			return  1 ;
	}

	if (u->e > v->e)
	{
		if (u->sgn == 0)	/* u,v >= 0 */
			return  1;
		else			/* u,v <  0 */
			return -1;
	}

	/* Signs and exponents equal; compare magnitudes.  */
	/* First, compare high-order 9 digits of magnitude.  */
	if (u->m[1] < v->m[1])
	{
		if (u->sgn == 0)	/* u,v >= 0 */
			return -1;
		else			/* u,v <  0 */
			return  1;
	}

	if (u->m[1] > v->m[1])
	{
		if (u->sgn == 0)	/* u,v >= 0 */
			return  1;
		else			/* u,v <  0 */
			return -1;
	}

	/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */
	if (u->m[1] == 0)	/* zero special case */
		return 0;

	if (u->m[0] < v->m[0])
	{
		if (u->sgn == 0)	/* u,v >= 0 */
			return -1;
		else			/* u,v <  0 */
			return  1;
	}

	if (u->m[0] > v->m[0])
	{
		if (u->sgn == 0)	/* u,v >= 0 */
			return  1;
		else			/* u,v <  0 */
			return -1;
	}

	/* Signs, exponents, high-order magnitudes, and low-order magnitudes equal.  */
	return 0;
}
