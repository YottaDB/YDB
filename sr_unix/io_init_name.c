/****************************************************************
 *								*
 * Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "io.h"

#include "gtm_unistd.h"

GBLDEF mstr	sys_input;
GBLDEF mstr	sys_output;

void io_init_name(void)
{
	char    *temp, *c;
	short   i;

	if (isatty(0))
	{
		temp = TTYNAME(0);
		for(i = 1, c = temp; *c != '\0'; i++)
			c++;
		sys_input.addr = (char*) malloc(i);
		memcpy(sys_input.addr,temp, i);
		sys_input.len = i - 1;
	} else
	{
		sys_input.addr = "0";
		sys_input.len = 1;
	}
	if (isatty(1))
	{
		temp = TTYNAME(1);
		for(i = 1, c = temp; *c != '\0'; i++)
			c++;
		sys_output.addr = (char *)malloc(i);
		memcpy(sys_output.addr,temp, i);
		sys_output.len = i - 1;
	} else
	{
		sys_output.addr = "&";
		sys_output.len = 1;
	}
	return;
}
