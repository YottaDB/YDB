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
	boolean_t	dot;
	char		*c, *eos;
	int4		zeroes, sigdigs, exp;

	MV_FORCE_STR(v);
	c = v->str.addr;
	if (0 == v->str.len)
		return FALSE;
	else if ((1 == v->str.len) && ('0' == *c))
		return TRUE;
	eos = c + v->str.len;
	zeroes = sigdigs = exp = 0;
	if ('-' == *c)
	{
		c++;
		if (c == eos)
			return FALSE;
	}
	dot = FALSE;
	if (('9' >= *c) && ('0' < *c))
	{
		while ((c != eos) && ('9' >= *c) && ('0' <= *c))
		{
			if ('0' == *c)		/* don't count trailing zeroes on a big number */
				zeroes++;
			else
				zeroes = 0;
			sigdigs++;
			exp++;
			c++ ;
		}
		if ((c != eos) && ('.' == *c))
		{
			dot = TRUE;
			c++;
			while ((c != eos) && ('9' >= *c) && ('0' <= *c))
			{
				if ('0' == *c)		/* don't count trailing zeroes on a big number */
					zeroes++;
				else
					zeroes = 0;
				sigdigs++;
				c++;
			}
		}
		sigdigs -= zeroes;
	} else if ('.' == *c)
	{
		dot = TRUE; c++;
		while ((c != eos) && ('0' == *c))
		{
			exp--;
			c++;
		}
		while ((c != eos) && ('9' >= *c) && ('0' <= *c))
		{
			sigdigs++;
			c++;
		}
	} else
		return FALSE;
	exp += MV_XBIAS;
	if ((c != eos) || (dot && (('0' == *(c - 1)) || ('.' == *(c - 1))))
			|| (NUM_DEC_DG_2L < sigdigs) || (EXPLO > exp) || (EXPHI <= exp))
		return FALSE;
	return TRUE;
}
