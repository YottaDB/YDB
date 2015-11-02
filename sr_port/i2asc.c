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

#include "gtm_string.h"

GBLREF	seq_num		seq_num_zero;

uchar_ptr_t i2asc(uchar_ptr_t p, unsigned int n)
{
	unsigned char	ar[MAX_DIGITS_IN_INT], *q;
	unsigned	m, len;

	q = ar + SIZEOF(ar);
	if (!n)
		*--q = '0';
	else
	{
		while (n)
		{
			m = n / 10;
			*--q = n - (m * 10) + '0';
			n = m;
		}
	}
	assert((uintszofptr_t)q >= (uintszofptr_t)ar);
	len = (unsigned int)(ar + SIZEOF(ar) - q);
	memcpy(p, q, len);
	return p + len;
}

uchar_ptr_t i2ascl(uchar_ptr_t p, qw_num n)
{
	unsigned char	ar[MAX_DIGITS_IN_INT8], *q;
	uint4		len;
	long		 r;

	q = ar + SIZEOF(ar);
	if (QWEQ(n, seq_num_zero))
		*--q = '0';
	else
	{
		while (QWNE(n, seq_num_zero))
		{
			QWDIVIDEBYDW(n, 10, n, r);
			*--q = r + '0';
		}
	}
	assert((uintszofptr_t)q >= (uintszofptr_t)ar);
	len = (uint4)(ar + SIZEOF(ar) - q);
	memcpy(p, q, len);
	return (unsigned char *)(p + len) ;
}
#ifdef INT8_SUPPORTED
uchar_ptr_t i2asclx(uchar_ptr_t p, qw_num n)
{
	unsigned char	ar[MAX_HEX_DIGITS_IN_INT8], *q;
	uint4		len;
	qw_num		m;

	q = ar + SIZEOF(ar);
	if (!n)
		*--q = '0';
	else
	{
		while (n)
		{
			m = n & 0xF;
			if (m <= 9)
				*--q = m + '0';
			else
				*--q = m - 0xa + 'A';
			n = n >> 4;
		}
	}
	assert((uintszofptr_t)q >= (uintszofptr_t)ar);
	len = (uint4)(ar + SIZEOF(ar) - q);
	memcpy(p, q, len);
	return p + len;
}
#else
uchar_ptr_t i2asclx(uchar_ptr_t p, qw_num n)
{
	unsigned char	ar[24], *q;
	uint4		msb, lsb, len, nibble;
	int		i;

	q = ar + SIZEOF(ar);
	lsb = n.value[lsb_index];
	msb = n.value[msb_index];
	if (msb)
	{
		for (i = 0; i < 8; i++)		/* 8 to denote 8 nibbles per 4-byte value */
		{
			nibble = lsb & 0xF;
			if (nibble <= 9)
				*--q = nibble + '0';
			else
				*--q = nibble - 0xa + 'A';
			lsb = lsb >> 4;
		}
		lsb = msb;
	}
	if (!lsb)
		*--q = '0';
	else
	{
		while (lsb)
		{
			nibble = lsb & 0xF;
			if (nibble <= 9)
				*--q = nibble + '0';
			else
				*--q = nibble - 0xa + 'A';
			lsb = lsb >> 4;
		}
	}
	assert((uintszofptr_t)q >= (uintszofptr_t)ar);
	len = ar + SIZEOF(ar) - q;
	memcpy(p, q, len);
	return p + len;
}
#endif
