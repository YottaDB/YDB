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

int i2hex_nofill(int num, uchar_ptr_t addr, int len)
{
	unsigned char	buff[8];
	int		i;

	assert(SIZEOF(buff) >= len);
	i2hex(num, buff, len);
	for (i = 0; i < len - 1 ; i++)
	{	if (buff[i] != '0')
			break;
	}
	memcpy(addr, &buff[i], len - i);
	return len - i;
}

int i2hexl_nofill(qw_num num, uchar_ptr_t addr, int len)
{
	unsigned char	buff[16];
	int	i;

	assert(SIZEOF(buff) >= len);
	i2hexl(num, buff, len);
	for (i = 0; i < len - 1 ; i++)
	{	if (buff[i] != '0')
			break;
	}
	memcpy(addr, &buff[i], len - i);
	return len - i;
}
