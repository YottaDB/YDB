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

/* lm_maint.c : License Man. function completing maintenance commands */

#include "mdef.h"
#include <climsgdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <rms.h>
#include "ladef.h"
#include "lmdef.h"
#include "la_io.h"
#define DELETE (rp[0]=='D' || rp[0]=='Y' )
#define MODIFY (rp[0]=='M' || rp[0]=='Y' || rp[0]==0 )
#define QUIT   (rp[0]=='Q')
#define PROPER(status) { if ((status & 1)==0) lib$signal(status) ;}

int lm_maint (void)
{
        int 	cli$get_getvalue();

	error_def	(LA_NOCNFDB);
	error_def	(LA_DESTR)  ;
	error_def	(LA_DELETE) ;
	error_def	(LA_EMPTY)  ;
	error_def	(LA_MOD)    ;
	error_def	(LA_MODCNF) ;
	int4 		status ;
 	unsigned short 	w ;
	char 		com[64] ;			/* command 	    */
	char 		rp[32] ;			/* reply	    */
        $DESCRIPTOR 	(dentv,"$VERB");
        $DESCRIPTOR 	(dcom,com);
	uint4 	kid ;				/* virt. keyb. ID    */
	la_prolog 	*prol;				/* db prolog	     */
	char 		*h ;				/* data base 	     */
	pak  		*p ;				/* pak record	     */
	pak 		q[PBUF] ;			/* pak pattern       */
	int 		v[32] ;				/* qualif. variables */
	int 		n,k ;				/* pak rec count     */
	char 		*x,*y,*z ;			/* temp pointers for */
	bool 		update,next ;
	int4		fao[1] ;
	struct FAB 	f ;
	struct RAB 	r ;

	status= smg$create_virtual_keyboard(&kid) ;
        PROPER(status) ;
        status= cli$get_value(&dentv,&dcom,&w)    ;
	PROPER(status) ;
	com[0]= (com[0]>='a' ? com[0]-32 : com[0]) ; com[w]= 0 ;

	la_getcli(v,q) ;
	la_puthead(q)  ;			/* pak pattern filled in  */
	h= la_getdb(LMDB) ;
	if (h==NULL)
	{
		lib$signal(LA_NOCNFDB) ;
	}
	prol= h ;
	p= h + SIZEOF(la_prolog) ;
	n= 0 ;
	update= FALSE ;
	rp[0]= 0 ;
	while (n != prol->N && !QUIT)
	{
		next= TRUE ;
		if (la_match(p,q,v))
		{
			switch (com[0])
			{
			case 'L' : lm_listpak(p) ;
				   break ;
			case 'D' : lm_listpak(p) ;
				   la_putmsgu(LA_EMPTY, 0, 0);
				   rp[0]= 0 ;
				   la_getstr(kid,LA_DESTR,rp,0,1) ;
				   rp[0]= (rp[0]>='a' ? rp[0]-32 : rp[0]) ;
				   if DELETE
				   {
					prol->N-- ;
					prol->len -= p->ph.l[0] ;
					x= p ; y= (char *)p + p->ph.l[0] ; z= h + prol->len ;
					while (x!=z)
					{
						*(x++) = *(y++) ;
					}
				        lm_putmsgu(LA_DELETE,0,0) ;
					next= FALSE;
					update= TRUE ;
				   }
				   break ;
			case 'M' : lm_listpak(p) ;
				   la_putmsgu(LA_EMPTY, 0, 0);
				   rp[0]= 0 ;
				   la_getstr(kid,LA_MOD,rp,0,1) ;
				   rp[0]= (rp[0]>='a' ? rp[0]-32 : rp[0]) ;
			  	   if MODIFY
				   {
					la_putmsgu(LA_EMPTY, 0, 0);
					lm_edit(kid,h,p,p->pd.L,p->pd.L+1) ;
					la_putfldr(&(p->pf)) ;
					update= TRUE ;
					lm_putmsgu (LA_MODCNF,0,0) ;
				   }
				   break ;
#ifdef VERIFY
			case 'V' : status= lm_verify(p) ;
				   fao[0]= p->pd.lid ;
				   lm_putmsgu(status,fao,1) ;
				   break ;
#endif
			otherwise: break ;
			}
		}
		if (next)
		{
			n++ ;
			p = (char *)p + p->ph.l[0] ;
		}
	}
        la_putmsgu(LA_EMPTY, 0, 0);
	if (update)
	{
		la_putdb(LMDB,h) ;
	}
	la_freedb(h) ;
	return status ;
}
