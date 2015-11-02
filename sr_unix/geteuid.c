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
}
