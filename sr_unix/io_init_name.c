/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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
#include "gtm_strings.h"

#include "io.h"
#include "gtm_logicals.h"

#include "gtm_unistd.h"
#include "gtm_limits.h"

GBLDEF mstr	sys_input;
GBLDEF mstr	sys_output;
GBLDEF mstr	gtm_principal;

void io_init_name(void)
{
	char    temp[TTY_NAME_MAX + 1], *c;
	int   	i, size;

	if (isatty(0))
	{
		memset(temp, '\0', TTY_NAME_MAX + 1);
		TTYNAME_R(0, temp, TTY_NAME_MAX, i); /* ttyname_r() is MT-Safe */
		if (0 == i)
		{
			size = STRLEN(temp);
			sys_input.addr = (char *) malloc(size);
			memcpy(sys_input.addr, temp, size);
			sys_input.len = size;
		} else
		{
			sys_input.addr = "0";
			sys_input.len = 1;
		}
	}
	else
	{
		sys_input.addr = "0";
		sys_input.len = 1;
	}
	if (isatty(1))
	{
		memset(temp, '\0', TTY_NAME_MAX + 1);
		TTYNAME_R(1, temp, TTY_NAME_MAX, i);
		if (0 == i)
		{
			size = STRLEN(temp);
			sys_output.addr =  (char *) malloc(size);
			memcpy(sys_output.addr, temp, size);
			sys_output.len = size;
		}
		else
		{
			sys_output.addr = "&";
			sys_output.len = 1;
		}
	} else
	{
		sys_output.addr = "&";
		sys_output.len = 1;
	}
	gtm_principal.addr = GTM_PRINCIPAL;
	gtm_principal.len = STR_LIT_LEN(GTM_PRINCIPAL);
	return;
}
