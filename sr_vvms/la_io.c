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

/* License Adiministration I/O system */
#include "mdef.h"

#include "gtm_string.h"
#include <ssdef.h>
#include <rms.h>
#include "ladef.h"
#include "la_io.h"
#define T  RMS$_NORMAL

 void bclose (struct FAB *f)
{
	sys$close(f) ;
}

 void bdelete (struct FAB *f,struct XABPRO *x)
{
	int	e;

	*x = cc$rms_xabpro;
	x->xab$w_pro = 0x0000;				/* (sy:rwed,ow:rwed,gr:rwed,wo:rwed) */
	f->fab$l_xab = x;

	e = sys$close(f);

	f->fab$l_fop = f->fab$l_fop | FAB$M_NAM;	/* Allow name block input	*/
	f->fab$l_fna = 0;
	f->fab$b_fns = 0;		/* Disable hard file name	*/

	e = sys$erase(f);
}

 int bcreat (
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 struct NAM *nam,
 struct XABPRO *xab,
 int n )                  /* file allocation */
{
	int e ;

	*f= cc$rms_fab ;
	f->fab$l_alq= n ;
	f->fab$w_deq= n/4 ;
	f->fab$b_fac= FAB$M_PUT ;
	f->fab$l_fna= fn ;
	f->fab$b_fns= strlen(fn) ;
	f->fab$l_mrn= 0 ;
	f->fab$l_fop= FAB$M_CBT ;
	f->fab$w_mrs= BLKS ;
	f->fab$b_org= FAB$C_SEQ ;
	f->fab$b_rfm= FAB$C_FIX ;
	f->fab$b_shr= FAB$M_NIL ;

	*xab = cc$rms_xabpro;
	xab->xab$w_pro = 0xEE88;	/*  /protection=(sy:rwe,ow:rwe,gr:r,wo:r)  */
	f->fab$l_xab = xab;

	f->fab$l_nam = nam;

	e= sys$create(f) ;
	if (e==T)
	{
		*r= cc$rms_rab ;
		r->rab$l_fab= f ;
		r->rab$b_mbc= 4;
		e= sys$connect(r) ;
	}
	if (e==T) e= SS$_NORMAL ;
        return (e) ;
}

 int vcreat (
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 int n )                  /* file allocation */
{
	int e ;
	*f= cc$rms_fab ;
	f->fab$b_fac= FAB$M_PUT ;
	f->fab$l_fna= fn ;
	f->fab$b_fns= strlen(fn) ;
	f->fab$l_mrn= 0 ;
	f->fab$b_org= FAB$C_SEQ ;
	f->fab$b_rfm= FAB$C_VAR ;
	f->fab$b_rat= FAB$M_CR ;
	f->fab$b_shr= FAB$M_NIL ;

	e= sys$create(f) ;
	if (e==T)
	{
		*r= cc$rms_rab ;
		r->rab$l_fab= f ;
		r->rab$b_mbc= 4;
		e= sys$connect(r) ;
	}
	if (e==T) e= SS$_NORMAL ;
        return (e) ;
}

 int bopen(
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 struct XABFHC *x ,
 struct NAM *nam)
{
	int e ;
	*f= cc$rms_fab ;
	f->fab$b_fac= FAB$M_GET;
	f->fab$l_fna= fn ;
	f->fab$b_fns= strlen(fn) ;
	f->fab$b_shr= FAB$M_SHRGET ;
	f->fab$l_xab= x ;

	*x= cc$rms_xabfhc ;
	f->fab$l_nam = nam;

	e= sys$open(f) ;
	if (e==T)
	{
		*r= cc$rms_rab ;
		r->rab$l_fab= f ;
		r->rab$b_mbc= 4 ;
		r->rab$b_rac= RAB$C_SEQ ;
		r->rab$l_rop= RAB$M_RAH ;
		e= sys$connect(r) ;
	}
	if (e==T) e= SS$_NORMAL ;
        return (e) ;
}

 int breopen(
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 struct XABFHC *x ,
 struct NAM *nam)
{
	int e ;
	*f= cc$rms_fab ;
	f->fab$b_fac= FAB$M_GET | FAB$M_DEL ;	/*  Allow also delete access	*/
	f->fab$l_fna= fn ;
	f->fab$b_fns= strlen(fn) ;
/*	f->fab$b_shr= FAB$M_SHRGET ;	*/	/*  No shared get		*/
	f->fab$l_xab= x ;

	*x= cc$rms_xabfhc ;
	f->fab$l_nam = nam;

	e= sys$open(f) ;
	if (e==T)
	{
		*r= cc$rms_rab ;
		r->rab$l_fab= f ;
		r->rab$b_mbc= 4 ;
		r->rab$b_rac= RAB$C_SEQ ;
		r->rab$l_rop= RAB$M_RAH ;
		e= sys$connect(r) ;
	}
	if (e==T) e= SS$_NORMAL ;
        return (e) ;
}

 int bread (
 struct RAB *r ,
 char *p ,			/* user buffer address         */
 unsigned short w )		/* user buffer length in bytes */
{
	int e ;
	r->rab$l_ubf= p ;
	r->rab$w_usz= w ;
	e= sys$get(r) ;
	if (e==T) e= SS$_NORMAL ;
	return(e) ;
}

 int bwrite (
 struct RAB *r ,
 char *p ,
 unsigned short w )
{
	int e ;
	r->rab$l_rbf= p ;
	r->rab$w_rsz= w ;
	if ((e= sys$put(r))==T) e= SS$_NORMAL ;
	return(e) ;
}
