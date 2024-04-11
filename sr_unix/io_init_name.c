/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
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
#define SET_TTYNAME(FD, DEVICE, NOTTY_NAME)						\
{											\
	boolean_t	ttyname_set;							\
											\
	ttyname_set = FALSE;								\
	if (isatty(FD))									\
	{										\
		char    temp[TTY_NAME_MAX + 1];						\
		int	i;								\
											\
		memset(temp, '\0', TTY_NAME_MAX + 1);					\
		TTYNAME_R(FD, temp, TTY_NAME_MAX, i); /* ttyname_r() is MT-Safe */	\
		if (0 == i)								\
		{									\
			int	size;							\
											\
			size = STRLEN(temp);						\
			ttyname_set = TRUE;						\
			DEVICE.addr = (char *)malloc(size);				\
			memcpy(DEVICE.addr, temp, size);				\
			DEVICE.len = size;						\
		}									\
	}										\
	if (!ttyname_set)								\
	{										\
		DEVICE.addr = NOTTY_NAME;						\
		assert(1 == STRLEN(NOTTY_NAME));					\
		DEVICE.len = 1;								\
	}										\
}

void io_init_name(void)
{
	SET_TTYNAME(0, sys_input, "0");
	SET_TTYNAME(1, sys_output, "&");
	return;
}
