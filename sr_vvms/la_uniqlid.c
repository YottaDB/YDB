/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* la_uniqlid.c : uniqlid == license ID is unique in the data base
   used         : la_create.c
*/

#include "mdef.h"
#include "ladef.h"

bool la_uniqlid (char *h,int4 lid)
/*
h - data base
id -license ID
*/
{

	error_def(LA_NOTUNIQ) ;
	bool unique ;
	int n ;
	la_prolog *prol;				/* db prolog	 */
	pak  *p ;					/* pak record	 */

	prol= h ;
	p= h + SIZEOF(la_prolog) ;

	n= 0 ;
	unique= TRUE ;
	while (unique && n!=prol->N)
	{
		unique= (p->pd.lid!=lid) ;
		n++ ;
		p = (char *)p + p->ph.l[0] ;
	}
	if (!unique)
	{
		la_putmsgu(LA_NOTUNIQ,0,0) ;
	}
	return(unique);
}
