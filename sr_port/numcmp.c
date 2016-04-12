/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
 *
 *	In order to better understand the numcmp algorithm, it would help to list a few representations of numerics (int & float).
 *	Note that SOME integers and decimal numbers upto 3 digits can have TWO representations.
 *	One is where the MV_INT bit is set. This is only if they are within a certain range.
 *	Another is where the MV_INT bit is not set.
 *	Examples of both are shown below.
 *	Note that the mantissa can be negative ONLY if MV_INT bit is set.
 *	If not, the sgn bit reflects the sign while the mantissa stays positive.
 *
 *	MV_NM | MV_INT representation
 *	-------------------------------
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,         -1} =>  -        0.001
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,        -10} =>  -        0.01
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,       -100} =>  -        0.1
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,      -1000} =>  -        1
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,     -10000} =>  -       10
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,    -100000} =>  -      100
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,   -1000000} =>  -     1000
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,  -10000000} =>  -    10000
 *	mvtype = 3, e =  0, sgn = 0, m = {        0, -100000000} =>  -   100000
 *	mvtype = 3, e =  0, sgn = 0, m = {        0, -999999000} =>  -   999999
 *	mvtype = 3, e =  0, sgn = 0, m = {        0, -999999876} =>  -   999999.876
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,          1} =>  +        0.001
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,         10} =>  +        0.01
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,        100} =>  +        0.1
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,       1000} =>  +        1
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,      10000} =>  +       10
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,     100000} =>  +      100
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,    1000000} =>  +     1000
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,   10000000} =>  +    10000
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,  100000000} =>  +   100000
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,  999999000} =>  +   999999
 *	mvtype = 3, e =  0, sgn = 0, m = {        0,  999999876} =>  +   999999.876
 *
 *	MV_NM only representation
 *	--------------------------
 *	mvtype = 1, e =  3, sgn = 0, m = {        0,  100000000} =>  1E-60 ; set a=1.23456789123456789E-43-(1.23456789123456788E-43)
 *								; above is the smallest positive non-zero numeric allowed in GT.M
 *	mvtype = 1, e = 19, sgn = 0, m = {        0,  100000000} =>  1E-44 ; set a=1.2E-43-(1.1E-43)
 *	mvtype = 1, e = 20, sgn = 0, m = {        0,  100000000} =>  1E-43 ; set a=1E-43
 *	mvtype = 1, e = 62, sgn = 0, m = {        0,  100000000} =>  0.1
 *	mvtype = 1, e = 63, sgn = 0, m = {        0,  100000000} =>  1
 *	mvtype = 1, e = 63, sgn = 0, m = {        0,  120000000} =>  1.2
 *	mvtype = 1, e = 71, sgn = 0, m = {        0,  123456789} =>  + 123456789
 *	mvtype = 1, e = 71, sgn = 0, m = {      876,  123456789} =>  + 123456789.876
 *	mvtype = 1, e = 73, sgn = 0, m = {        0,  100000000} =>  1E+10
 *	mvtype = 1, e = 83, sgn = 0, m = {        0,  100000000} =>  1E+20
 *	mvtype = 1, e = 93, sgn = 0, m = {        0,  100000000} =>  1E+30
 *	mvtype = 1, e =103, sgn = 0, m = {        0,  100000000} =>  1E+40
 *	mvtype = 1, e =110, sgn = 0, m = {898765432,  991234567} => ~1E+48 ; set a=1E46*99.1234567898765432
 *	mvtype = 1, e =110, sgn = 0, m = {999999999,  999999999} => ~1E+48 ; set a=1E46*99.9999999999999999
 *								; above is the largest positive numeric allowed n GT.M
 *	mvtype = 1, e = 69, sgn = 1, m = {        0,  100000000} =>  -1,000,000
 *	mvtype = 1, e = 68, sgn = 1, m = {        0,  999999000} =>  -  999,999
 *	mvtype = 1, e = 63, sgn = 1, m = {        0,  100000000} =>  -        1
 *	mvtype = 1, e =  0, sgn = 0, m = {        0,          0} =>  -        0
 *	mvtype = 1, e =  0, sgn = 0, m = {        0,          0} =>  +        0
 *	mvtype = 1, e = 63, sgn = 0, m = {        0,  100000000} =>  +        1
 *	mvtype = 1, e = 68, sgn = 0, m = {        0,  999999000} =>  +  999,999
 *	mvtype = 1, e = 69, sgn = 0, m = {        0,  100000000} =>  +1,000,000
 *
 */
long numcmp(mval *u, mval *v)
{
	mval		w;
	int		u_sgn, v_sgn, exp_diff, m1_diff, m0_diff;
	int		u_m0, v_m0, u_m1, v_m1;
	int		u_mvtype;

	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);

	/* If both are integer representations, just compare m[1]'s.  */
	u_mvtype = u->mvtype & MV_INT;
	if (u_mvtype & v->mvtype)
	{
		u_m1 = u->m[1];
		v_m1 = v->m[1];
		if      (u_m1 >  v_m1)     return  1;
		else if (u_m1 == v_m1)     return  0;
		else /* (u_m1  < v_m1) */  return -1;
	}
	/* If not both integer, promote either one that might be. */
	if (u_mvtype)
	{
		w = *u ;
		promote(&w) ;
		u = &w ;
	} else if (v->mvtype & MV_INT)
	{
		w = *v ;
		promote(&w) ;
		v = &w ;
	}
	/* Compare signs.  */
	u_sgn = (0 == u->sgn) ? 1 : -1;	/* 1 if positive, -1 if negative */
	v_sgn = (0 == v->sgn) ? 1 : -1;
	if (u_sgn != v_sgn)
		return u_sgn;
	/* Signs equal; compare exponents for magnitude and adjust sense depending on sign.  */
	exp_diff = u->e - v->e;
	if (exp_diff)
	{
		if (0 > exp_diff)
			return -u_sgn;
		else
			return u_sgn;
	}
	/* Signs and exponents equal; compare magnitudes.  */
	/* First, compare high-order 9 digits of magnitude.  */
	u_m1 = u->m[1];
	v_m1 = v->m[1];
	m1_diff = u_m1 - v_m1;
	if (m1_diff)
	{
		if (0 > m1_diff)
			return -u_sgn;
		else
			return u_sgn;
	}
	/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */
	if (0 == u_m1)	/* zero special case */
		return 0;
	u_m0 = u->m[0];
	v_m0 = v->m[0];
	m0_diff = u_m0 - v_m0;
	if (m0_diff)
	{
		if (0 > m0_diff)
			return -u_sgn;
		else
			return u_sgn;
	}
	/* Signs, exponents, high-order magnitudes, and low-order magnitudes equal.  */
	return 0;
}
