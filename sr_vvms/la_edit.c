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

/* la_edit.c: new license entered, inteactively with fields editting
   used in  : la_create.c
*/

#include "mdef.h"
#include <ssdef.h>
#include <descrip.h>
#include "ladef.h"
#define MINT 0x0FFFFFFF
#define MUNS 0xFFFFFFFF
#define MSHO 32767
#define B 	 (e==SS$_NORMAL)
#define cop(a,b) { char *y= a ;while (*b!=0) *(y++)= *(b++) ; *y= 0 ;}

void la_edit   (uint4 kid,char *h,pak *p)
/*
uint4 	kid ;				virt. keyboard ID
char 		*h ;			data base
pak		*p ;            	buf. for new pak rec
*/
{
	int	str$upcase() ;
	int 	k,e,u,w ;
	int4 	n  ;
	bool	valid ;
	char	buf[9*ADDR] ;			/* temporary		*/
	int4	sid[NSYS] ;			/* temp hardw. model	*/
	int4	mdl ;				/* hardware model	*/

	int4	*psid ;				/* pak SIDs             */
	char	*padr ;				/* pak address		*/
	char	*pcom ;				/* pak comment 		*/
	char	*badr ;				/* pointer to buf	*/
	char	*top  ;

	error_def(LA_PNAM) ;
	error_def(LA_PVER) ;
	error_def(LA_PX)   ;
	error_def(LA_PT0)  ;
	error_def(LA_PT1)  ;
	error_def(LA_PLID) ;
	error_def(LA_PL)   ;
	error_def(LA_PSID) ;
	error_def(LA_PCUST) ;
	error_def(LA_PADR1) ;
	error_def(LA_PADR2) ;
	error_def(LA_PADR3) ;
	error_def(LA_PADR4) ;
	error_def(LA_PCOM) ;
	error_def(LA_PENC) ;
	error_def(LA_VALNAM) ;
	error_def(LA_VALVER) ;
	error_def(LA_INVAL) ;
	uint4 ladr[5]= {LA_PCUST,LA_PADR1,LA_PADR2,LA_PADR3,LA_PADR4} ;

	$DESCRIPTOR(dnam,p->pd.nam) ;
	$DESCRIPTOR(dver,p->pd.ver) ;
	$DESCRIPTOR(dbuf,buf) ;

	valid= FALSE ;
	while (!valid)
	{
		w= la_getstr(kid,LA_PNAM,p->pd.nam,1,PROD)   ;
		dnam.dsc$w_length= w ;
		e= str$upcase(&dnam,&dnam) ; if (!B) lib$signal(e) ;
		valid= la_validate(LA_VALNAM,p->pd.nam) ;
	}
	valid= FALSE ;
	while (!valid)
	{
		w= la_getstr(kid,LA_PVER,p->pd.ver,0,VERS)   ;
		dver.dsc$w_length= w ;
		e= str$upcase(&dver,&dver) ; if (!B) lib$signal(e) ;
		valid= la_validate(LA_VALVER,p->pd.ver) ;
	}
	n= p->pd.x ;
	la_getnum(kid,LA_PX,&n,0,MSHO) ; p->pd.x= n ;
	la_getdat(kid,LA_PT0,&(p->pd.t0),0,MUNS)  ;
	la_getdat(kid,LA_PT1,&(p->pd.t1),p->pd.t0[1],MUNS)  ;
	valid= FALSE ;
	while (!valid)
	{
	 	la_getnum(kid,LA_PLID,&(p->pd.lid),1,MINT) ;
		valid= la_uniqlid(h,p->pd.lid) ;
	}
	n= p->pd.L ;
	psid= (char *)p + SIZEOF(pak) ;
	padr= psid + 2*n ;
	for (k= 0;k!=n;k++)
	{
		sid[k]= psid[k] ;
	}
	for (k= n;k!=NSYS;k++)
	{
		sid[k]= 0 ;
	}
	la_getnum(kid,LA_PL,&n,1,NSYS) ; p->pd.L= n ;
	for (k= 0;k!=n;k++)
	{
		w= la_mdl2nam(buf,sid[k]) ;
		buf[w]= 0 ;
		valid= FALSE ;
		while (!valid)
		{
			w= la_getstr(kid,LA_PSID,buf,0,HWLEN) ;
			dbuf.dsc$w_length= w ;
			e= str$upcase(&dbuf,&dbuf) ; if (!B) lib$signal(e) ;
			valid= la_nam2mdl(&mdl,w,buf) ;
			if (!valid)
			{
				la_putmsgu(LA_INVAL,0,0) ;
			}
		}
		sid[k]= mdl ;
	}
	badr= buf ;
	for (k= 0;k!=5;k++)
	{
		cop(badr,padr) ;
		w= la_getstr(kid,ladr[k],badr,0,ADDR) ;
		padr++ ;
		badr += w + 1 ;
	}
	cop(badr,padr) ;
	w= la_getstr(kid,LA_PCOM,badr,0,4*ADDR) ;
	top= badr + w + 1 ;
	for (k= 0;k!=n;k++)
	{
		psid[k]= sid[k] ;
	}
	padr= psid + 2*n ;
	badr= buf ;
	while (badr!=top)
	{
		*(padr++)= *(badr++) ;
	}
	n= p->ph.n ;
	la_getnum(kid,LA_PENC,&n,0,NCRY) ; p->ph.n= n ;
}
