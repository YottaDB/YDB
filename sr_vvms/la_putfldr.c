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

/* la_putfldr.c : fills the pak folder in
   used in      : la_create.c
*/

#include <ssdef.h>
#include "mdef.h"
#include <jpidef.h>
#include "ladef.h"
#include <descrip.h>
#define B (e==SS$_NORMAL)

void la_putfldr ( pfldr *pf )
{
	$DESCRIPTOR(doid,&(pf->oid))  ;
	int sys$gettim();
	int   e ;
	short w ;
	int4 item= JPI$_USERNAME ;

	e= sys$gettim(&(pf->std)) ;
	if (!B) lib$signal(e) ;

	doid.dsc$w_length= 16 ;
	e= lib$getjpi(&item,0,0,0,&doid,&w) ;
	if B (pf->oid)[w]= 0  ;
	else lib$signal(e)    ;
}
