/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "main_pragma.h"

#include <sys/types.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"

int main(int argc, char **argv)
{
	if (geteuid() == 0)
		PRINTF("root\n");
	else
		PRINTF("other\n");
	return 0;
}
