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
#include "zbreak.h"

char *zr_get_last(z_records *z)
{
char *c;

if ((c = z->free - z->rec_size) >= z->beg)
	return(c);
else
	return((char*)0);
}
