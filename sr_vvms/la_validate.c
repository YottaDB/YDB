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

/* la_validate.c: valid == 'val is in the message string given by code'
   used in      : la_edit.c
*/

#include "mdef.h"
#include <ssdef.h>
#include "ladef.h"
#include <descrip.h>
#include "gtm_string.h"
#define B (e==SS$_NORMAL)

bool la_validate (int4 code,char *val)
/*
int4 code ;				mes. code for validation str
char *val ;                          	validated value
*/
{
	int sys$getmsg() ;

	error_def(LA_CHOOSE) ;		/* Choose a value from the list	*/
	char buf[256] ;			/* list of valid values 	*/
	$DESCRIPTOR(dbuf,buf) ;

	unsigned short w ;		/* res. string length   */
	int e,k,n,len    ;
	bool valid,prop  ;

	dbuf.dsc$w_length= 256 ;
	e= sys$getmsg(code,&w,&dbuf,1,0)  ; if (!B) lib$signal(e) ;

	len= strlen(val) ;
	n= 0 ; prop= TRUE ; valid= (n==len) ;
	while(!valid && n<w)
	{
		if (prop)
		{
			k= 0 ; valid= TRUE ;
			while(valid && n!=w && buf[n]!=',')
			{
				valid= (buf[n]==val[k]) ;
				n++ ; k++ ;
			}
			valid= (valid && k==len) ;
		}
		prop= (buf[n]==',') ;
		n++ ;
	}
	if (!valid)
	{
		la_putmsgu(LA_CHOOSE,0,0) ;
		la_putmsgu(code,0,0) ;
	}
	return(valid) ;
}
