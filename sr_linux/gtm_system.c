/****************************************************************
*                                                              *
*      Copyright 2007 Fidelity Information Services, Inc *
*                                                              *
*      This source code contains the intellectual property     *
*      of its copyright holder(s), and is made available       *
*      under a license.  If you do not know the terms of       *
*      the license, please stop and do not read further.       *
*                                                              *
****************************************************************/

#ifdef __CYGWIN__

/* The version of system() included in Cygwin 1.54-2		*
 * (aka newlib libc/stdlib/system.c 1.8) does not properly	*
 * handle signals or EINTR.  For proper functioning of GT.M,	*
 * replace this routine with a good version of system()		*
 * until a Cygwin version fixes this problem.			*
*/

#include "gtm_stdlib.h"

int gtm_system(const char *line)
{
	return system(line);
}
#endif
