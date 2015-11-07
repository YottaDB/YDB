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

/* lmu : License Management main program */

#include "mdef.h"
#include <climsgdef.h>
#include <descrip.h>
#include <ssdef.h>
#include <rmsdef.h>
#include "ladef.h"
extern int lm_cmnd() ;

lmu ()
{
        int 	cli$dispatch() ;
	int4 	stat0 ;
	int4 	stat1 ;
	int4	fl= 0 ;
        char 	buf[256] ;
	char	*ln[2]= { LMDB,LMFILE } ;
        short 	len ;
	bool	rep ;
        $DESCRIPTOR (prompt,"LMU> ") ;
        $DESCRIPTOR (dbuf,buf) ;

	stat0= lib$get_foreign(&dbuf,0,&len,&fl) ;
	rep  = (len==0) ;
	do {
		stat1= cli$dcl_parse(&dbuf,&lm_cmnd,&lib$get_input,&lib$get_input,&prompt) ;
	        if (stat1==CLI$_NORMAL)
		{
			stat0= cli$dispatch(ln) ;
		}
		if (stat0!=RMS$_EOF && rep)
		{
			stat0= lib$get_foreign(&dbuf,&prompt,&len,&fl) ;
		}
	} while (stat0!=RMS$_EOF && rep) ;
	return  (stat0==RMS$_EOF ? SS$_NORMAL : stat0) ;
}
