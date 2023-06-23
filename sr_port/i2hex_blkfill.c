/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

void i2hex_blkfill(int num, uchar_ptr_t addr, int len)
{
	unsigned char	buff[8];
	int		i;

	assert((SIZEOF(buff) >= len) && (0 < len));
	i2hex(num, buff, len);
	for ( i = 0; i < len - 1 ; i++)
	{
		if (buff[i] != '0')
			break;
		buff[i] = ' ';
	}
	memcpy(addr, buff, len);
}

void i2hexl_blkfill(qw_num num, uchar_ptr_t addr, int len)
{
	unsigned char	buff[16];
	int		i;

	assert((SIZEOF(buff) >= len) && (0 < len));
	i2hexl(num, buff, len);
	for ( i = 0; i < len - 1 ; i++)
	{
		if (buff[i] != '0')
			break;
		buff[i] = ' ';
	}
	memcpy(addr, buff, len);
}
