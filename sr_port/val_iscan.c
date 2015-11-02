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

#include "arit.h"

int	val_iscan(mval *v)
{
	bool	dot;
	char 	*c, *eos;
	int4	zeroes, sigdigs, exp;

	MV_FORCE_STR(v);

	c = v->str.addr;
	if (v->str.len == 0)
		return FALSE;
	else if (v->str.len == 1 && *c == '0')
		return TRUE;
	eos = c + v->str.len;
	zeroes = sigdigs = exp = 0;
	if (*c == '-')
	{	c++;
		if (c == eos)
			return FALSE;
	}
	dot = FALSE;
	if (*c <= '9' && *c > '0')
	{
		while (c != eos && *c <= '9' && *c >= '0')
		{	if (*c == '0')		/* don't count trailing zeroes on a big number */
				zeroes++;
			else
				zeroes = 0;
			sigdigs++;
			exp++;
			c++ ;
		}
		if (c != eos && *c == '.')
		{	dot = TRUE;
			c++;
			while (c != eos && *c <= '9' && *c >= '0')
			{	if (*c == '0')		/* don't count trailing zeroes on a big number */
					zeroes++;
				else
					zeroes = 0;
				sigdigs++;
				c++;
			}
		}
		sigdigs -= zeroes;
	} else if (*c == '.')
	{
		dot = TRUE ; c++;
		while (c != eos && *c == '0')
		{	exp--;
			c++;
		}
		while (c != eos && *c <= '9' && *c >= '0')
		{	sigdigs++;
			c++;
		}
	} else
		return FALSE;
	exp += MV_XBIAS;
	if (c != eos || (dot && (*(c-1) == '0' || *(c-1) == '.')) || (NUM_DEC_DG_2L < sigdigs)|| (EXPLO > exp) || (EXPHI <= exp))
		return FALSE;
	return TRUE;
}
