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

/* lp_licensed.c = SS$_NORMAL : product is licensed
	         = LP_LEXPIR  : license has expired
		 = LP_NOAVAL  : license not yet available
		 = LP_INVCSM  : invalid checksum
   used in       : licensed products
*/
#include "mdef.h"
#include "ladef.h"
#include <ssdef.h>
#include <descrip.h>
#include "la_encrypt.h"
#define MAXSHO 32767

int4 lp_licensed(char *h, struct dsc$descriptor *prd, struct dsc$descriptor *ver, int4 mdl, int4 nid,
		int4 *lid, int4 *x, int4 *days, pak *p)
/* h is a pointer to the data base */
/* prd is a pointer to a descriptor that in turn points to the product name */
/* ver is a pointer to a descriptor that in turn points to the version	*/
/* mdl is a hardware model id */
/* nid is a cluster node ID */
/* lid is a pointer to a license ID (returned) */
/* x is a pointer to a license value (returned) */
/* days is a pointer to a number of days until expiration (returned) */
/* p is a pointer to a pak record (returned) */
{
	int4	memcmp() ;
	void	lm_convert() ;
	error_def(LP_LEXPIR) ;
	error_def(LP_NOAVAL) ;
	error_def(LP_NOCONF) ;
	error_def(LP_INVCSM) ;
	la_prolog *prol;				/* db prolog	     */
	int4 	*psid ;
	int4 	*pnid ;
	int	k,j ;
	int4 	status;
	short 	offset  ;
	int4 	d0,dd,d1;
	bool 	match,pmat,vmat,nmat,valid ;
	int4	bcs[3]= {0,0,0} ;
	int4	pcs[3]= {0,0,0} ;

	*lid= *x= *days= 0 ;
	prol= h ;
	p= h ;
	offset= SIZEOF(la_prolog) ;
	match= FALSE ;
	j= 0 ;
	while (!match && j != prol->N)
	{
		p = (char *)p + offset ;
		pmat = (p->pd.nam[prd->dsc$w_length]==0) && (memcmp(prd->dsc$a_pointer,p->pd.nam,prd->dsc$w_length)==0) ;
		vmat = (p->pd.ver[0]==0) || (memcmp(ver->dsc$a_pointer,p->pd.ver,ver->dsc$w_length)==0) ;
		if  (pmat && vmat)
		{
			psid= (char *)p + p->ph.l[3] ;
			pnid= (char *)p + p->ph.l[4] ;
			pcs[0]= pcs[1]= 0 ;
			lm_convert(p->ph.cs,pcs) ;
			k= 0 ;
			nmat= FALSE ;
			while (!nmat && k!=p->pd.L)
			{
				nmat= (nid==pnid[k] && (mdl==psid[k] || psid[k]==0));
				k++ ;
			}
		}
		match= pmat && vmat && nmat ;
		offset= p->ph.l[0] ;
		j++ ;
	}
	if (match)
	{
		*lid= p->pd.lid ;
		*x= (p->pd.x==0 ? MAXSHO : p->pd.x) ;
		valid= la_encrypt(p->ph.n,&(p->pd),(p->ph.l[4] - p->ph.l[2]),bcs) ;
		valid= valid && (bcs[0]==pcs[0]) && (bcs[1]==pcs[1]) ;
		if (valid)
		{
			if (p->pd.t1[1]==0)			/* Unlimited License */
			{
				*days= MAXSHO ;
				status= SS$_NORMAL ;
			}
			else
			{
				lib$day(&dd,0)	;		/* Current days    */
				lib$day(&d1,p->pd.t1) ;		/* Expiration days */
				if (p->pd.t0[1])		/* Available days  */
					lib$day(&d0, p->pd.t0) ;
				else
					d0 = 0;
				*days= d1 - dd ;
				status = ( dd<d0 ? LP_NOAVAL : ( dd<d1 ? SS$_NORMAL : LP_LEXPIR )) ;
			}
		}
		else
		{
			status= LP_INVCSM ;
		}
	}
	else
	{
		status= LP_NOCONF ;
	}
	return status;
}
