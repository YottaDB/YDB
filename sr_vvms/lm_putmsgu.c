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

/* lm_putmsgu.c : outputs and formats message for user message code
		  and user FAO arguments; includes all prefixes.
 */
#include "mdef.h"

#include <ssdef.h>

 void lm_putmsgu (c,fao,n)
 int4 c ;				/* message code		    */
 int4 fao[] ;				/* fao arguments	    */
 short n ;				/* number of fao arg.s	    */
 {
	int k;
	struct { short argc ;           /* structure longword count */
                 short opt   ;          /* message display options  */
                 int4 code   ;          /* message code 	    */
	         short count ;          /* FAO count    	    */
	         short newopt;          /* new options  	    */
	         int4 fao[16];          /* fao arguments	    */
               } msgvec ;

	msgvec.argc=    n+2 ;
	msgvec.opt=     0x0001 ;
	msgvec.code=    c ;
	msgvec.count=   n ;
	msgvec.newopt=  0x000F ;

	for (k= 0;k!=n;k++) msgvec.fao[k]= fao[k] ;
	sys$putmsg(&msgvec) ;
}
