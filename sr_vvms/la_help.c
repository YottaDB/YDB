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

/* la_help.c: displays the help file
   used in  : la_cmnd.cld
*/

#include "mdef.h"
#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>
#include "ladef.h"

int la_help ()
{
        int 	cli$get_getvalue();
	int	lbr$output_help() ;
	int e ;
	int4 fl= 1 ;
 	short w ;
	char buf[256] ;
        $DESCRIPTOR (dent,"topic");
        $DESCRIPTOR (dbuf,buf)    ;
        $DESCRIPTOR (dlib,LAHP)   ;

        e = cli$get_value(&dent,&dbuf,&w) ;
	if (e!=SS$_NORMAL)
	{
		dbuf.dsc$w_length= 0 ;
	}
	lbr$output_help(lib$put_output,0,&dbuf,&dlib,&fl,lib$get_input) ;

	return e ;
}
