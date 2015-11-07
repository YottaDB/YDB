/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <ssdef.h>
#include <climsgdef.h>
#include <descrip.h>

#define HLP$M_PROMPT 1
#define HELP_LIBRARY "GTM$HELP:CCE"

void cce_help(void)
{

	uint4 flags;
	char buff[256];
	$DESCRIPTOR(line, buff);
	$DESCRIPTOR(libr, HELP_LIBRARY);
	$DESCRIPTOR(ent, "QUERY");

	if (CLI$PRESENT(&ent) != CLI$_PRESENT || CLI$GET_VALUE(&ent,&line) != SS$_NORMAL)
		line.dsc$w_length = 0;
	flags = HLP$M_PROMPT;
	lbr$output_help(lib$put_output,0,&line,&libr,&flags,lib$get_input);
	return;

}
