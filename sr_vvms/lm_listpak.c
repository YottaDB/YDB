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

/* lm_listpak.c: lists one pak record to stdout
   used in     : lm_register.c,lm_maint.c
 */

#include "mdef.h"
#include <ssdef.h>
#include <descrip.h>
#include "gtm_string.h"
#include "ladef.h"
#include "lmdef.h"

void lm_listpak (pak *p)
/*
pak	*p ;                               	buffer with the pak record
*/
{

	int4	*psid ;				/* pak SIDs             */
	int4	*pnid ;				/* pak NIDs		*/

	int4	fao[16] ;			/* fao arguments	*/
	char	buf[32] ;
	int	k ;
	short	w ;

	error_def(LA_HEAD) ;	/* Listing header    */
	error_def(LA_NAM) ;	/* Product           */
	error_def(LA_VER) ;	/* Version           */
	error_def(LA_X) ;	/* License value     */
	error_def(LA_UNLX) ;	/* Unlimited license */
	error_def(LA_T0) ;	/* Date available    */
	error_def(LA_T1) ;	/* Date expires      */
	error_def(LA_UNLT0) ;	/* Date unlimited    */
	error_def(LA_UNLT1) ;	/* Date unlimited    */
	error_def(LA_LID) ;	/* License ID        */
	error_def(LA_L) ;	/* Number of systems */
	error_def(LA_SID) ;	/* Hardware model    */
	error_def(LA_UNSID) ;	/* Unlimited hw.     */
	error_def(LA_NID) ;	/* Node ID           */
	error_def(LA_CS) ;	/* Check sum         */
	error_def(LA_STD) ;	/* Creation date     */
	error_def(LA_OID) ;	/* Operator ID       */

	psid= (char*)p + p->ph.l[3] ;
	pnid= (char*)p + p->ph.l[4] ;

	la_putmsgu(LA_HEAD,0,0) ;

	fao[0]= strlen(p->pd.nam) ; fao[1]= (p->pd.nam) ;
	la_putmsgu(LA_NAM,fao,2) ;
	fao[0]= strlen(p->pd.ver) ; fao[1]= (p->pd.ver) ;
	la_putmsgu(LA_VER,fao,2) ;
	if (p->pd.x!=0)			/* job limited license 	*/
	{
		la_putmsgu(LA_X,&(p->pd.x),1) ;
	}
	else				/* unlimited license  	*/
	{
		la_putmsgu(LA_UNLX,0,0) ;
	}
	if (p->pd.t0[1]!=0)		/* available date given */
	{
	        fao[0]= &(p->pd.t0) ;
		la_putmsgu(LA_T0,fao,1) ;
	}
	else				/* date not given 	*/
	{
		la_putmsgu(LA_UNLT0,0,0) ;
	}
	if (p->pd.t1[1]!=0)		/* available date given */
	{
	        fao[0]= &(p->pd.t1) ;
		la_putmsgu(LA_T1,fao,1) ;
	}
	else				/* date not given 	*/
	{
		la_putmsgu(LA_UNLT1,0,0) ;
	}
	la_putmsgu(LA_LID,&(p->pd.lid),1) ;
	la_putmsgu(LA_L,&(p->pd.L),1) ;
	for (k= 0;k!=(p->pd.L);k++)
	{
		if (psid[k]==0)
		{
			fao[0]= k ;
			la_putmsgu(LA_UNSID,fao,1) ;
		}
		else
		{
			w= la_mdl2nam(buf,psid[k]) ;
			fao[0]= k ; fao[1]= w ; fao[2]= buf ;
			la_putmsgu(LA_SID,fao,3) ;
		}
	}
	for (k= 0;k!=(p->pd.L);k++)
	{
		fao[0]= k ; fao[1]= pnid[k] ;
		la_putmsgu(LA_NID,fao,2) ;
	}

	fao[0]= p->ph.n ;
	fao[1]= fao[3]= fao[5]= fao[7]= 4 ;
        fao[2]= p->ph.cs ;
        fao[4]= p->ph.cs +  4 ;
        fao[6]= p->ph.cs +  8 ;
        fao[8]= p->ph.cs + 12 ;
	la_putmsgu(LA_CS,fao,9) ;
        fao[0]= &(p->pf.std) ;
	la_putmsgu(LA_STD,fao,1) ;
	fao[0]= strlen(p->pf.oid) ; fao[1]= (p->pf.oid) ;
	la_putmsgu(LA_OID,fao,2) ;
}
