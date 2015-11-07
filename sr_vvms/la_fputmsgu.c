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

/* la_fputmsgu.c : outputs and formats message for user message code
	  	  and user FAO arguments to a file.
 */
#include "mdef.h"

#include <ssdef.h>
#include <rms.h>
#include <descrip.h>
#include "ladef.h"
#include "la_putline.h"
#include "la_fputmsgu.h"

#define B (e==SS$_NORMAL)

 void la_fputmsgu (struct RAB *rab,int4 c,int4 fao[],short n)
 {
	int k,e;
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
	msgvec.newopt=  0x0001 ;

	for (k= 0;k!=n;k++) msgvec.fao[k]= fao[k] ;

	e= sys$putmsg(&msgvec,&la_putline,0,rab) ;
	if (!B) lib$signal(e) ;

}
