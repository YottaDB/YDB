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

/* la_puthead.c : fills pak header in
   used in      : la_create.c
*/
#include "mdef.h"
#include "gtm_string.h"
#include "ladef.h"

void la_puthead (p)
pak *p ; 						/* pak data 	  */
{
	int k,u,w ;
	char *padr ;

	p->ph.l[1]= SIZEOF(phead) ;                    	/* offset to pfldr */
	p->ph.l[2]= p->ph.l[1] + SIZEOF(pfldr) ;        /* offset to pdata */
	p->ph.l[3]= p->ph.l[2] + SIZEOF(pdata) ;	/* offset to psid  */
	p->ph.l[4]= p->ph.l[3] + SIZEOF(int4)*(p->pd.L);/* offset to pnid  */
	p->ph.l[5]= p->ph.l[4] + SIZEOF(int4)*(p->pd.L);/* offset to padr  */

	padr= (char*)p + p->ph.l[5] ;
	w= u= 0 ;
	for (k= 0;k!=5;k++)
	{
		u= strlen(padr) ;
		padr += u + 1 ;
		w += u + 1 ;
	}
	p->ph.l[6]= p->ph.l[5] + w ;      		/* offset to pcom  */
	p->ph.l[0]= p->ph.l[6] + strlen(padr) ;		/* offset to next  */
}
