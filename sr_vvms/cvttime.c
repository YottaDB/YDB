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
#include <descrip.h>
#include "cvttime.h"


int4 cvttime(mval *src, int4 tim[2])
{
	$DESCRIPTOR	(dsrc,src->str.addr) ;
	int4            cnx= 0, fl= 127;
	int4		status;

	dsrc.dsc$w_length= src->str.len ;
	status= lib$convert_date_string(&dsrc,tim,&cnx,&fl,0,0) ;
	return status;
}

