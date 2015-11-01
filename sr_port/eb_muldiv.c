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

/*	eb_muldiv - emulate extended precision (18-digit) multiplication and division */

#include "mdef.h"

#include "arit.h"
#include "eb_muldiv.h"

#define FLO_HI		1e9
#define FLO_LO		1e8
#define FLO_BIAS	1e3

#define DFLOAT2MINT(X,DF) (X[1] = DF, X[0] = (DF - (double)X[1])*FLO_HI)

#define RADIX		10000


/*	eb_int_mul - multiply two GT.M INT's
 *
 *	input
 *		v1, u1 - GT.M INT's
 *
 *	output
 *		if the product will fit into a GT.M INT format:
 *			function result = FALSE => no promotion necessary
 *			p[] = INT product of (v1*m1)
 *		else (implies overflow out of INT format):
 *			function result = TRUE => promotion to extended precision necessary
 *			p[] = undefined
 */

bool	eb_int_mul (int4 v1, int4 u1, int4 p[])
{
	double	pf;
	int4	tp[2], promote;

	promote = TRUE;	/* promote if overflow or too many significant fractional digits */
	pf = (double)u1*(double)v1/FLO_BIAS;
	if ((pf < FLO_HI)  &&  (pf > -FLO_HI))
	{
		DFLOAT2MINT(tp, pf);
		if (tp[0] == 0)	/* don't need extra precision */
		{
			promote = FALSE;
			p[0] = tp[0];  p[1] = tp[1];
		}
	}
	return promote;
}


/*	eb_mul - multiply two GT.M extended precision numeric values
 *
 *	input
 *		v[], u[] - GT.M extended precision numeric value mantissas
 *
 *	output
 *		function result = scale factor of result
 *		p[] = GT.M extended precision mantissa of (u*v)
 */

int4	eb_mul (int4 v[], int4 u[], int4 p[])	/* p = u*v */
{
	short	i, j;
	int4	acc, carry, m1[5], m2[5], prod[9], scale;

	/* Throughout, larger index => more significance. */

	for (i = 0 ;  i < 9 ;  i++)
		prod[i] = 0;

	/* Break up 2-4-(3/1)-4-4 */

	m1[0] = v[0] % RADIX;
	m2[0] = u[0] % RADIX;

	m1[1] = (v[0]/RADIX) % RADIX;
	m2[1] = (u[0]/RADIX) % RADIX;

	m1[2] = (v[1] % (RADIX/10))*10 + v[0]/(RADIX*RADIX);
	m2[2] = (u[1] % (RADIX/10))*10 + u[0]/(RADIX*RADIX);

	m1[3] = (v[1]/(RADIX/10)) % RADIX;
	m2[3] = (u[1]/(RADIX/10)) % RADIX;

	m1[4] = v[1]/((RADIX/10)*RADIX);
	m2[4] = u[1]/((RADIX/10)*RADIX);

	for (j = 0 ;  j <= 4 ;  j++)
	{
		if (m2[j] != 0)
		{
			for (i = 0, carry = 0 ;  i <= 4 ;  i++)
			{
				acc = m1[i]*m2[j] + prod[i+j] + carry;
				prod[i+j] = acc % RADIX;
				carry     = acc / RADIX;
			}
			if ( 9 > i+j)
				prod[i+j] = carry;
			else
				if (0 != carry)
					assert(FALSE);
		}
	}

	if (prod[8] >= RADIX/10)
	{
		/* Assemble back 4-4-1/3-4-2 */
		scale = 0;	/* no scaling needed */
		p[0] = ((prod[6]%1000)*RADIX + prod[5])*(RADIX/ 100) + (prod[4]/ 100);
		p[1] = ( prod[8]      *RADIX + prod[7])*(RADIX/1000) + (prod[6]/1000);
	}
	else /* prod[8] < RADIX/10  [means not normalized] */
	{
		/* Assemble back 3-4-2/2-4-3 */
		scale = -1;	/* to compensate for normalization */
		p[0] = ((prod[6]%100)*RADIX + prod[5])*(RADIX/ 10) + (prod[4]/ 10);
		p[1] = ( prod[8]     *RADIX + prod[7])*(RADIX/100) + (prod[6]/100);
	}

	return scale;
}


/*	eb_mvint_div - divide to GT.M INT's
 *
 *	input
 *		v, u	- INT's to be divided
 *
 *	output
 *		if the quotient will fit into a GT.M INT:
 *			function value = FALSE => no promotion necessary
 *			q[] = INT quotient of (v/u)
 *		else (implies overflow out of GT.M INT forat):
 *			function value = TRUE => promotion to extended precision necessary
 *			q[] = undefined
 */

bool	eb_mvint_div (int4 v, int4 u, int4 q[])
{
	double	qf;
	int4	tq[2], promote;

	promote = TRUE;	/* promote if overflow or too many significant fractional digits */
	qf = (double)v*FLO_BIAS/(double)u;
	if ((qf < FLO_HI)  &&  (qf > -FLO_HI))
	{
		DFLOAT2MINT(tq, qf);
		if (tq[0] == 0)	/* don't need extra word of precision */
		{
			promote = FALSE;
			q[0] = tq[0];  q[1] = tq[1];
		}
	}
	return promote;
}


/*	eb_int_div - integer division of two GT.M INT's
 *
 *	input
 *		v1, u1 - GT.M INT's to be divided
 *
 *	output
 *		if result fits into a GT.M INT:
 *			function value = FALSE => no promotion necessary
 *			q[] = INT result of (v1\u1)
 *		else (implies some sort of overflow):
 *			function result = TRUE => promotion to extended precision necessary
 *			q[] = undefined
 */

bool	eb_int_div (int4 v1, int4 u1, int4 q[])
{
	double	qf;
	qf= (double)v1*FLO_BIAS/(double)u1;
	if (qf < FLO_HI  &&  qf > -FLO_HI)
	{
		DFLOAT2MINT(q,qf);
		q[1]= (q[1]/MV_BIAS)*MV_BIAS;
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


/*	eb_div - divide two GT.M extended precision numeric values
 *
 *	input
 *		x[], y[] - GT.M extended precision numeric value mantissas
 *
 *	output
 *		function result = scale factor of result
 *		q[] = GT.M extended precision mantissa of (y/x)
 */

int4	eb_div (int4 x[], int4 y[], int4 q[])	/* q = y/x */
{
	int4	borrow, carry, i, j, scale, prod, qx[5], xx[5], yx[10];

	for (i = 0 ;  i < 5 ;  i++)
		yx[i] = 0;
	if (x[1] < y[1]  ||  (x[1] == y[1]  &&  x[0] <= y[0]))	/* i.e., if x <= y */
	{
		/* Break y apart 3-4-2/2-4-3 */
		scale = 1;
		yx[5] = (y[0]%(RADIX/10))*10;
		yx[6] = (y[0]/(RADIX/10))%RADIX;
		yx[7] = (y[1]%(RADIX/100))*(RADIX/100) + y[0]/((RADIX/10)*RADIX);
		yx[8] = (y[1]/(RADIX/100))%RADIX;
		yx[9] =  y[1]/((RADIX/100)*RADIX);
	}
	else
	{
		/* Break y apart 4-4-1/3-4-2 */
		scale = 0;
		yx[5] = (y[0]%(RADIX/100))*100;
		yx[6] = (y[0]/(RADIX/100))%RADIX;
		yx[7] = (y[1]%(RADIX/1000))*(RADIX/10) + y[0]/((RADIX/100)*RADIX);
		yx[8] = (y[1]/(RADIX/1000))%RADIX;
		yx[9] =  y[1]/((RADIX/1000)*RADIX);
	}
	/* Break x apart 4-4-1/3-4-2 */
	xx[0] = (x[0]%(RADIX/100))*100;
	xx[1] = (x[0]/(RADIX/100))%RADIX;
	xx[2] = (x[1]%(RADIX/1000))*(RADIX/10) + x[0]/((RADIX/100)*RADIX);
	xx[3] = (x[1]/(RADIX/1000))%RADIX;
	xx[4] =  x[1]/((RADIX/1000)*RADIX);

	assert (yx[9] <= xx[4]);
	for (i = 4 ;  i >= 0 ;  i--)
	{
		qx[i] = (yx[i+5]*RADIX + yx[i+4]) / xx[4];
		if (qx[i] != 0)
		{
			/* Multiply x by qx[i] and subtract from remainder. */
			for (j = 0, borrow = 0 ;  j <= 4 ;  j++)
			{
				prod = qx[i]*xx[j] + borrow;
				borrow = prod/RADIX;
				yx[i+j] -= (prod%RADIX);
				if (yx[i+j] < 0)
				{
					yx[i+j] += RADIX;
					borrow ++;
				}
			}
			yx[i+5] -= borrow;

			while (yx[i+5] < 0)
			{
				qx[i] --;	/* estimate too high */
				for (j = 0, carry = 0 ;  j <= 4 ;  j++)
				{
					yx[i+j] += (xx[j] + carry);
					carry = yx[i+j]/RADIX;
					yx[i+j] %= RADIX;
				}
				yx[i+5] += carry;
			}
		}
		assert (0 <= qx[i]  &&  qx[i] < RADIX);	/* make sure in range */
		assert (yx[i+5] == 0);		/* check that remainder doesn't overflow */
	}

	/* Assemble q 4-4-1/3-4-2 */
	q[0] = ((qx[2]%1000)*RADIX + qx[1])*100 + (qx[0]/ 100);
	q[1] = ( qx[4]      *RADIX + qx[3])* 10 + (qx[2]/1000);

	assert (   (FLO_LO <= q[1]  &&  q[1] < FLO_HI)
		|| (q[1] == 0  &&  q[0] == 0  &&  y[1] == 0  &&  y[0] == 0) );

	return scale;
}


/*	eb_int_mod - INT modulus of two GT.M INT's
 *
 *	input
 *		v1, u1 - GT.M INT's
 *
 *	output
 *		p[] = INT value of (v1 mod u1) == (v1 - (u1*floor(v1/u1)))
 */

void	eb_int_mod (int4 v1, int4 u1, int4 p[])
{
	int4	quo, rat, neg;

	if (u1 == 0  ||  v1 == 0)
	{
		p[1]= 0;
	}
	else
	{
		quo = v1/u1;
		rat = v1 != quo*u1;
		neg = (v1 < 0  &&  u1 > 0) || (v1 > 0  && u1 < 0);
		p[1] = v1 - u1*(quo - (neg && rat));
	}
	return;
}
