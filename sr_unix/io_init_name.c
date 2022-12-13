/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 732d6f04 (GT.M V7.0-005)
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

#include "gtm_unistd.h"
#include "gtm_limits.h"

GBLDEF mstr	sys_input;
GBLDEF mstr	sys_output;

/* We have seen "isatty()" return TRUE but "ttyname()" return NULL in some cases (e.g. K8s, nix etc.)
 * See https://gitlab.com/YottaDB/DB/YDB/-/issues/491#note_895186879 and surrounding discussion for details.
 * Hence we treat that case as if "isatty()" returned FALSE (i.e. it is not a terminal) and give it a default name.
 */
#define SET_TTYNAME(FD, DEVICE, NOTTY_NAME)				\
{									\
	boolean_t	ttyname_set;					\
									\
	ttyname_set = FALSE;						\
	if (isatty(FD))							\
	{								\
		char    *temp;						\
									\
		temp = TTYNAME(FD);					\
		if (NULL != temp)					\
		{							\
			int	i;					\
			char    *c;					\
									\
			ttyname_set = TRUE;				\
			for (i = 1, c = temp; *c != '\0'; i++)		\
				c++;					\
			DEVICE.addr = (char *)malloc(i);		\
			memcpy(DEVICE.addr, temp, i);			\
			DEVICE.len = i - 1;				\
		}							\
	}								\
	if (!ttyname_set)						\
	{								\
		DEVICE.addr = NOTTY_NAME;				\
		assert(1 == STRLEN(NOTTY_NAME));			\
		DEVICE.len = 1;						\
	}								\
}

void io_init_name(void)
{
<<<<<<< HEAD
	SET_TTYNAME(0, sys_input, "0");
	SET_TTYNAME(1, sys_output, "&");
=======
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
>>>>>>> 732d6f04 (GT.M V7.0-005)
	return;
}
