/****************************************************************
 *                                                              *
 *      Copyright 2006 Fidelity Information Services, Inc 	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

/* These are dummy routines to allow code to compile/link on VMS. These routines are not
   used but elimination of them from the code is detremental to the clarify of the UNIX
   codebase and since Alpha/VMS is a dead platform, these dummy routines also have a
   limited lifespan.. 9/2006 SE
*/

int     utf8_len_strict(unsigned char* ptr, int len)
{
	GTMASSERT;
}

void    utf8_badchar(int len, unsigned char* str, unsigned char *strtop, int chset_len, unsigned char* chset)
{
	GTMASSERT;
}
