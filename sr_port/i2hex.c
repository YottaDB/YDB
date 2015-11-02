/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

void i2hex(UINTPTR_T val, uchar_ptr_t dest, int len)
{
	register uchar_ptr_t cp;
	register UINTPTR_T n;
	char x;
	n = val;
	cp = &dest[len];
	while (cp > dest)
	{
		x = n & 0xF;
		n >>= 4;
		*--cp = x + ((x > 9) ? 'A' - 10 : '0');
	}
	return;
}

#ifdef INT8_SUPPORTED
void i2hexl(qw_num val, uchar_ptr_t dest, int len)
{
	uchar_ptr_t	cp;
	qw_num		n;
	char		x;

	n = val;
	cp = &dest[len];

	while (cp > dest)
	{
		x = n & 0xF;
		n >>= 4;
		*--cp = x + ((x > 9) ? 'A' - 10 : '0');
	}
}
#else
void i2hexl(qw_num val, uchar_ptr_t dest, int len)
{
	if (4 > len)
	{	/* We only process some bytes of the low order word */
		i2hex(val.value[msb_index], dest, len);
		return;
	}
	i2hex(val.value[msb_index], dest, 8);	/* Process low order word */
	dest += 8;
	len -= 8;
	if (len)
		i2hex(val.value[lsb_index], dest, len);
}
#endif
