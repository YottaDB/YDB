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

/* la_initpak.c: initializes the pak record
   used in     : la_create.c
 */

#include "mdef.h"
#include <ssdef.h>
#include <descrip.h>
#include "ladef.h"

void la_initpak (llid,p)
int4 llid ;				/* last license ID	*/
pak *p ;                               	/* buff. for the pak rec*/
{
	int k ;
	char *q ;

	q= (char *)p ;
	for (k= 0;k!=PBUF;k++)
	{
		*(q++) = 0 ;
	}

	p->ph.n= 0 ;
	p->ph.cs[0]= 0 ;

	p->pd.nam[0]= 0 ;
	p->pd.ver[0]= 0 ;
	p->pd.x= 0 ;
	p->pd.t0[1]= 0 ;
	p->pd.t1[1]= 0 ;
	p->pd.lid= llid + 1 ;
	p->pd.L= 0 ;

}
