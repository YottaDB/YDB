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
#include <descrip.h>
#include <libdef.h>
#include "quad2asc.h"

/* convert a GOQ data value to a canonical MUMPS string */

int quad2asc(int4 mantissa[2],char exponent,unsigned char *outaddr,unsigned short outaddrlen,unsigned short *actual_len)
/*
int4 mantissa[2];	a quad word which is the 'mantissa'
char exponent;		the decimal exponent
unsigned char *outaddr;	the address to put the output string
unsigned short outaddrlen; the maximum length of the output string
unsigned short *actual_len; the actual length of the output string
*/
/* value returned: status code */
{
	struct	dsc$descriptor in, out;
	int4 status;
	int n,m;
	unsigned char *cp;
	int4 exp;

	in.dsc$w_length = SIZEOF(mantissa);
	in.dsc$b_dtype = DSC$K_DTYPE_Q;
	in.dsc$b_class = DSC$K_CLASS_S;
	in.dsc$a_pointer = mantissa;
	out.dsc$w_length = outaddrlen;
	out.dsc$b_dtype = DSC$K_DTYPE_T;
	out.dsc$b_class = DSC$K_CLASS_S;
	out.dsc$a_pointer = outaddr;
	status = lib$cvt_dx_dx(&in,&out,actual_len);
	if ((status & 1) == 0)
		rts_error(VARLSTCNT(1) status);
	exp = exponent;
	if (*actual_len == 1 && *outaddr == '0')
		return status;
	if (exp > 0)
	{
		n = exp + *actual_len;
		if (n > outaddrlen)
			return LIB$_DESSTROVF;
		for (m = exp, cp = outaddr + *actual_len ; m > 0 ; m--)
			*cp++ = '0';
		*actual_len = n;
	}
	else if (exp < 0)
	{
		exp = - exp;
		n = *actual_len;
		cp = outaddr;
		if (*cp == '-')
		{
			n--;
			cp++;
		}
		m = n - exp;
		if (m <= 0)
		{
			m = - m;
			if (n + m + 1 > outaddrlen)
				return LIB$_DESSTROVF;
			*actual_len += m + 1;
			memcpy(cp + m + 1, cp, n);
			*cp++ = '.';
			memset(cp, '0', m);
		} else
		{
			if (n + 1 > outaddrlen)
				return LIB$_DESSTROVF;
			*actual_len += 1;
			cp += m;
			memcpy(cp + 1, cp, n - 1);
			*cp = '.';
		}
		for (cp = outaddr + *actual_len, n = 0; cp > outaddr + 1; *actual_len -= 1)
		{
			if (*--cp != '0')
				break;
		}
			if (*cp == '.')
				*actual_len -= 1;
	}
	return status;
}
