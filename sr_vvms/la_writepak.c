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

/* la_writepak.c: writes a pak record to a file
   used in      : la_maint.c
 */

#include "mdef.h"
#include <rms.h>
#include "gtm_string.h"
#include "ladef.h"
#include "la_fputmsgu.h"
#include "la_writepak.h"

void la_writepak (struct FAB *f,pak *p)
/*
struct FAB *f ;				output file
pak *p ;                                pak record
*/
{

	int4 *psid ;			/* pak SIDs             */
	int4 *pnid ;			/* pak NIDs		*/
	char *padr ;			/* pak address		*/
	char *pcom ;			/* pak comment 		*/

	int4 	fao[16] ;		/* fao arguments	*/
	int4	mdl ;
	int 	k, j ;
	short	w ;
	char	buf[32] ;
	char	hex[8] ;		/* hex version of hw. mdl */
	static readonly char x[17] = "0123456789ABCDEF" ;

	error_def(LA_NAM) ;	/* Product           */
	error_def(LA_VER) ;	/* Version           */
	error_def(LA_X) ;	/* License value     */
	error_def(LA_PX) ;	/* Unlimited license */
	error_def(LA_T0) ;	/* Date available    */
	error_def(LA_T1) ;	/* Date expires      */
	error_def(LA_PT0) ;	/* Date available    */
	error_def(LA_PT1) ;	/* Date expires      */
	error_def(LA_LID) ;	/* License ID        */
	error_def(LA_L) ;	/* Number of systems */
	error_def(LA_SIDX) ;	/* Hw mdl, with hex  */
	error_def(LA_PSID) ;	/* Hw mdl. unlimited */
	error_def(LA_CS) ;	/* Check sum         */

	error_def(LA_PAKHD) ;	/* PAK header        */
	error_def(LA_ISSUR) ;	/* Issuer name, addr */
	error_def(LA_ISSUE) ;	/* headings	     */
	error_def(LA_ISSDT) ;	/* Issue date	     */
	error_def(LA_ISADR) ;	/* Customer name,adr */
	error_def(LA_DELIM) ;	/* page delimiter    */
	error_def(LA_EMPTY) ;	/* empty line	     */

	psid= (char *)p + p->ph.l[3] ;
	pnid= (char *)p + p->ph.l[4] ;
	padr= (char *)p + p->ph.l[5] ;

	la_fputmsgu(f,LA_PAKHD,0,0) ;
	la_fputmsgu(f,LA_ISSUR,0,0) ;
	la_fputmsgu(f,LA_ISSUE,0,0) ;
	fao[0]= 0 ;				/* outputs current time */
	fao[0] = &(p->pf.std);
	la_fputmsgu(f,LA_ISSDT,fao,1) ;

	for (k= 0;k!=5;k++)
	{
		fao[0]= strlen(padr) ; fao[1]= padr ;
		la_fputmsgu(f,LA_ISADR,fao,3) ;
		padr= padr + strlen(padr) + 1 ;
	}
	la_fputmsgu(f,LA_DELIM,0,0) ;

	fao[0]= strlen(p->pd.nam) ; fao[1]= (p->pd.nam) ;
	la_fputmsgu(f,LA_NAM,fao,2) ;
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	fao[0]= strlen(p->pd.ver) ; fao[1]= (p->pd.ver) ;
	la_fputmsgu(f,LA_VER,fao,2) ;
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	if (p->pd.x!=0)			/* job limited license 	*/
	{
		la_fputmsgu(f,LA_X,&(p->pd.x),1) ;
	}
	else				/* unlimited license  	*/
	{
		la_fputmsgu(f,LA_PX,0,0) ;
	}
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	if (p->pd.t0[1]!=0)		/* available date given */
	{
	        fao[0]= &(p->pd.t0) ;
		la_fputmsgu(f,LA_T0,fao,1) ;
	}
	else				/* date not given 	*/
	{
		la_fputmsgu(f,LA_PT0,0,0) ;
	}
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	if (p->pd.t1[1]!=0)		/* available date given */
	{
	        fao[0]= &(p->pd.t1) ;
		la_fputmsgu(f,LA_T1,fao,1) ;
	}
	else				/* date not given 	*/
	{
		la_fputmsgu(f,LA_PT1,0,0) ;
	}
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	la_fputmsgu(f,LA_LID,&(p->pd.lid),1) ;
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	la_fputmsgu(f,LA_L,&(p->pd.L),1) ;
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	for (k= 0;k!=(p->pd.L);k++)
	{
		if (psid[k]==0)
		{
			fao[0] = k ;
			la_fputmsgu(f,LA_PSID,fao,1) ;
		}
		else
		{	mdl = psid[k] ;
			w = la_mdl2nam(buf,mdl) ;
			j = 8 ;
			while (mdl!=0)
			{
				hex[--j] = x[mdl%16] ;
				mdl >>=4 ;
			}
			fao[0] = k ;
			fao[1] = w ; fao[2] = buf ;
			fao[3] = 8 - j ; fao[4] = hex+j ;
			la_fputmsgu(f,LA_SIDX,fao,5) ;
		}
	}
	la_fputmsgu(f,LA_EMPTY,0,0) ;
	fao[0]= p->ph.n ;
	fao[1]= fao[3]= fao[5]= fao[7]= 4 ;
        fao[2]= p->ph.cs ;
        fao[4]= p->ph.cs +  4 ;
        fao[6]= p->ph.cs +  8 ;
        fao[8]= p->ph.cs + 12 ;
	la_fputmsgu(f,LA_CS,fao,9) ;
}
