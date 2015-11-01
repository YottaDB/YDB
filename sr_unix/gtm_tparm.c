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

/*
 * This file wraps the curses tparm function to avoid a conflict with
 * the bool typedef in mdef.h and curses.h.  Since the only use
 * appears to be in sr_unix/iott_use.c, this is a reasonable detour.
 * The alternative would be to excise bool from all unix files, insuring
 * that persistent structures were correctly sized.
 */
#include "gtm_tparm.h"
#include <curses.h>
#include "gtm_term.h"

char *gtm_tparm(char *c, int p1, int p2)
{
	return tparm(c, p1, p2, 0, 0, 0, 0, 0, 0, 0);
}

