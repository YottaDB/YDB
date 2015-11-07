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

/* lm_edit.c: new license entered interactively, with editing
   used in  : lm_register.c ,lm_maint.c
*/

#include "mdef.h"
#include <ssdef.h>
#include <descrip.h>
#include "ladef.h"
#include "lmdef.h"
#define MINT 0x0FFFFFFF
#define MUNS 0xFFFFFFFF
#define MSHO 32767
#define B (e==SS$_NORMAL)

void lm_edit (int4 kid,char *h,pak *p,int4 lo,int4 hi)
/*
int4	kid ;				virt. keyboard ID
char 	*h ;				data base
pak	*p ;    			returns - pak record
int4 	lo ;				min num. of systems
int4	hi ;				max num. of systems
*/
{
	int4 	str$upcase() ;
	int 	k,e,w ;
	char 	buf[32] ;
	int4 	n ;
	bool	valid ;

	int4 	*psid ;				/* pak SIDs             */
	int4	*pnid ;				/* pak NIDs		*/
	char	mbuf[HWLEN+1] ;			/* buf for hardw. model */
	int4	mdl ;				/* hardware model	*/

	error_def(LA_PNAM) ;
	error_def(LA_PVER) ;
	error_def(LA_PX)   ;
	error_def(LA_PT0)  ;
	error_def(LA_PT1)  ;
	error_def(LA_PLID) ;
	error_def(LA_PL)   ;
	error_def(LA_PSID) ;
	error_def(LA_PCS)  ;
	error_def(LA_INVAL);

	$DESCRIPTOR(dnam,p->pd.nam) ;
	$DESCRIPTOR(dver,p->pd.ver) ;
	$DESCRIPTOR(dcsm,p->ph.cs) ;
	$DESCRIPTOR(dbuf,mbuf) ;

	dnam.dsc$w_length= la_getstr(kid,LA_PNAM,p->pd.nam,1,PROD) ;
	dver.dsc$w_length= la_getstr(kid,LA_PVER,p->pd.ver,0,VERS) ;
	e= str$upcase(&dnam,&dnam) ; if (!B) lib$signal(e) ;
	e= str$upcase(&dver,&dver) ; if (!B) lib$signal(e) ;

	n= p->pd.x ;
	la_getnum(kid,LA_PX,&n,0,MSHO) ; p->pd.x= n ;
	la_getdat(kid,LA_PT0,&(p->pd.t0),0,MUNS)  ;
	la_getdat(kid,LA_PT1,&(p->pd.t1),p->pd.t0[1],MUNS)  ;
	la_getnum(kid,LA_PLID,&(p->pd.lid),1,MINT) ;

	n= p->pd.L ;
	la_getnum(kid,LA_PL,&n,lo,hi) ; p->pd.L= n ;
	psid= (char *)p + SIZEOF(pak) ;
	for (k= 0;k!=n;k++)
	{
		w= la_mdl2nam(mbuf,psid[k]) ;
		mbuf[w]= 0 ;
		valid= FALSE ;
		while (!valid)
		{
			w= la_getstr(kid,LA_PSID,mbuf,0,HWLEN) ;
			dbuf.dsc$w_length= w ;
			e= str$upcase(&dbuf,&dbuf) ; if (!B) lib$signal(e) ;
			valid= la_nam2mdl(&mdl,w,mbuf) ;
			if (!valid)
			{
				la_putmsgu(LA_INVAL,0,0) ;
			}
		}
		psid[k]= mdl ;
	}
	pnid= psid + n ;
	lm_getnid(kid,pnid,psid,n) ;
	buf[0]= p->ph.n+'0' ; buf[1]= '-' ;
	for (k=0;k!=4;k++)
	{
		buf[k+2]=  p->ph.cs[k]   ; buf[6]= '-' ;
		buf[k+7]=  p->ph.cs[k+4] ; buf[11]= '-';
		buf[k+12]= p->ph.cs[k+8] ; buf[16]= '-';
		buf[k+17]= p->ph.cs[k+12];
	}
	buf[CSLN]= 0 ;
	la_getstr(kid,LA_PCS,buf,CSLN,CSLN+1) ;
	p->ph.n= buf[0]-'0' ;
	for (k=0;k!=4;k++)
	{
		p->ph.cs[k]=    buf[k+2] ;
		p->ph.cs[k+4]=  buf[k+7] ;
		p->ph.cs[k+8]=  buf[k+12] ;
		p->ph.cs[k+12]= buf[k+17] ;
	}
	e= str$upcase(&dcsm,&dcsm) ; if (!B) lib$signal(e) ;
}
