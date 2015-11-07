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

/* la_putline : writes message supplied by sys$putmsg to file given by its RAB
   used in    : la_fputmsgu.c
 */

#include "mdef.h"
#include <rms.h>
#include <descrip.h>
#include "la_io.h"

int la_putline (
 struct dsc$descriptor *line ,
 struct RAB *rab )
{
	int4 status ;
	status=  bwrite(rab,line->dsc$a_pointer,line->dsc$w_length) ;
	if ((status & 1)==0)
	{
		lib$signal(status) ;
	}
	return 0 ;
}
