/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "main_pragma.h"
#include <stdio.h>

#define MAX_LEN	32
int main()
{
	unsigned char	inbuf[MAX_LEN];
	int		i;

	fread(inbuf, MAX_LEN, MAX_LEN, stdin);
	for (i = 0; i < MAX_LEN; i++)
		printf("%02X", inbuf[i]);
	return 0;
}
