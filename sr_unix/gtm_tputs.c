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

#include <curses.h>
#include "gtm_term.h"
#include "gtm_tputs.h"

#ifdef __sparc
int gtm_tputs(char *c, int la, int (*putcfunc)(char))
#else
int gtm_tputs(char *c, int la, int (*putcfunc)(int))
#endif
{
	return tputs(c, la, putcfunc);
}
