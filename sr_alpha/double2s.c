/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stringpool.h"

#define MAX_NUM_SIZE	64

GBLREF spdesc stringpool;

static	char	pot_index [256] =
		{
			-78,  78, -77,  77,  77,  77, -76,  76,  76, -75,  75,  75, -74,  74,  74,  74,
			-73,  73,  73, -72,  72,  72, -71,  71,  71,  71, -70,  70,  70, -69,  69,  69,
			-68,  68,  68, -67,  67,  67,  67, -66,  66,  66, -65,  65,  65, -64,  64,  64,
			 64, -63,  63,  63, -62,  62,  62, -61,  61,  61,  61, -60,  60,  60, -59,  59,
			 59, -58,  58,  58,  58, -57,  57,  57, -56,  56,  56, -55,  55,  55,  55, -54,
			 54,  54, -53,  53,  53, -52,  52,  52,  52, -51,  51,  51, -50,  50,  50, -49,
			 49,  49,  49, -48,  48,  48, -47,  47,  47, -46,  46,  46,  46, -45,  45,  45,
			-44,  44,  44, -43,  43,  43,  43, -42,  42,  42, -41,  41,  41, -40,  40,  40,
			 40, -39,  39,  39, -38,  38,  38, -37,  37,  37, -36,  36,  36,  36, -35,  35,
			 35, -34,  34,  34, -33,  33,  33,  33, -32,  32,  32, -31,  31,  31, -30,  30,
			 30,  30, -29,  29,  29, -28,  28,  28, -27,  27,  27,  27, -26,  26,  26, -25,
			 25,  25, -24,  24,  24,  24, -23,  23,  23, -22,  22,  22, -21,  21,  21,  21,
			-20,  20,  20, -19,  19,  19, -18,  18,  18,  18, -17,  17,  17, -16,  16,  16,
			-15,  15,  15,  15, -14,  14,  14, -13,  13,  13, -12,  12,  12,  12, -11,  11,
			 11, -10,  10,  10,  -9,   9,   9,  -8,   8,   8,   8,  -7,   7,   7,  -6,   6,
			  6,  -5,   5,   5,   5,  -4,   4,   4,  -3,   3,   3,  -2,   2,   2,   2,  -1
		};

static	double	pot [79] =
		{
			1.701411834604692e+38 + 2.83e+22,
				1e+38,	1e+37,	1e+36,	1e+35,	1e+34,	1e+33,	1e+32,
			1e+31,	1e+30,	1e+29,	1e+28,	1e+27,	1e+26,	1e+25,	1e+24,
			1e+23,	1e+22,	1e+21,	1e+20,	1e+19,	1e+18,	1e+17,	1e+16,
			1e+15,	1e+14,	1e+13,	1e+12,	1e+11,	1e+10,	 1e+9,	 1e+8,
			 1e+7,	 1e+6,	 1e+5,	 1e+4,	 1e+3,	 1e+2,	 1e+1,	  1.0,
			 1e-1,	 1e-2,	 1e-3,	 1e-4,	 1e-5,	 1e-6,	 1e-7,	 1e-8,
			 1e-9,	1e-10,	1e-11,	1e-12,	1e-13,	1e-14,	1e-15,	1e-16,
			1e-17,	1e-18,	1e-19,	1e-20,	1e-21,	1e-22,	1e-23,	1e-24,
			1e-25,	1e-26,	1e-27,	1e-28,	1e-29,	1e-30,	1e-31,	1e-32,
			1e-33,	1e-34,	1e-35,	1e-36,	1e-37,	1e-38,	0
		};

#define POT_UNITY	39	/* Subscript of pot:  pot[POT_UNITY] == 1.0 */


struct	D_float		/* Format of D-floating point datum */
{
	unsigned int	     : 7;	/* fraction, bits 0:6 */
	unsigned int	exp  : 8;	/* exponent, bits 7:14 */
	unsigned int	sign : 1;	/* sign, bit 15 */
	unsigned int	     : 16;	/* fraction, bits 16:31 */
	unsigned int	     : 32;	/* fraction, bits 32:63 */
};

void	double2s (double *dp, mval *v)
{
	double		d = *dp;
	char		*p, *q;
	int		i, j, k;

	ENSURE_STP_FREE_SPACE(MAX_NUM_SIZE);
	assert (stringpool.free >= stringpool.base);
	v->mvtype = MV_STR;
	p = v->str.addr
	  = (char *)stringpool.free;

	if (d == 0.0)
		*p++ = '0';
	else
	{
		if (d < 0.0)
		{
			*p++ = '-';	/* plug in a minus sign */
			d = -d;		/* but make d positive */
		}

		i = pot_index[((struct D_float *)dp)->exp];
		if (i < 0)
		{
			i = -i;
			if (d < pot[i])
				++i;
		}
		i = POT_UNITY + 1 - i;

		/* "Normalize" the number;  i.e. adjust it to be between 0.0 and 1.0 */
		d *= pot[i + POT_UNITY];

		if (d < 5e-16)
			/* Call it zero */
			*p++ = '0';
		else
		{
			/* Round the sixteenth digit */
			d += 5e-16;

			if (d >= 1.0)
			{
				/* Readjust it to be between 0.0 and 1.0 */
				d /= 10.0;
				++i;
			}

			q = p;		/* q will point to the last non-zero byte */
			j = i;

			if (i <= 0)
			{
				*p++ = '.';
				for (;  i < 0;  ++i)
					*p++ = '0';
			}

			for (i = 15;  i > 0;  --i)
			{
				/* Multiply the value by ten, put the integer portion
				   of the result into k (0 <= k <= 9), and replace the
				   value with the fractional portion of the result	*/
				k = d *= 10.0;
				d -= k;

				*p++ = '0' + k;

				if (k > 0)
					q = p;
				if (--j == 0)
				{
					q = p;
					*p++ = '.';
				}
			}

			if (j > 0)
				do
					*p++ = '0';
				while (--j > 0);
			else
				p = q;
		}
	}

	v->str.len = p - (char *)stringpool.free;
	stringpool.free = (unsigned char *)p;
	assert(stringpool.free <= stringpool.top);

	return;
}
