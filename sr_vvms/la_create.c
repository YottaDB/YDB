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

/* la_create.c : License Administration function for creating new licenses
   used in     : license_adm.c
*/

#include <ssdef.h>
#include "mdef.h"
#include <climsgdef.h>
#include <descrip.h>
#include "ladef.h"
#include "lmdef.h"
#define SAVE (rp[0]=='S' || rp[0]=='Y' )
#define DISP (rp[0]=='D' || rp[0]==0 )
#define QUIT (rp[0]=='Q')
#define EDIT (rp[0]=='E')

int la_create (void)
{
        int 	smg$create_virtual_keyboard(), toupper();

	error_def(LA_NOCNFDB); 			/* No license created	*/
	error_def(LA_NOCNF)  ;			/* No license created	*/
	error_def(LA_NEWCNF) ;			/* New license created	*/
	error_def(LA_SAVE) ;			/* Save Y/N ?		*/
	error_def(LA_EMPTY) ;
	error_def(LA_BADENCR) ;

	char *h ;				/* db in main store 	*/
	la_prolog *prol ;			/* db file prolog 	*/
	pak  *p ;                       	/* pak record          	*/

	char 	rp[32] ;			/* operator reply      	*/
	int4	status ;
	unsigned char 	recall= 16 ;
 	unsigned short 	w ;
	uint4  	kid ;			/* virt. keyboard ID    */
	uint4	bcs[3]= {0,0,0} ;

	if ((h= la_getdb(LADB))==NULL)		/* db in main storage	*/
	{
		lib$signal(LA_NOCNFDB) ;
	}
	prol= h	;
	p= (char *)h + prol->len ;		/* place for new pak	*/

	status= smg$create_virtual_keyboard(&kid,0,0,0,&recall)
	;if (status!=SS$_NORMAL)
	{
		lib$signal(status) ;
	}
	la_initpak(prol->lastid,p) ;		/* pak initialized	*/
	rp[0]= 'E' ;
	while (!SAVE && !QUIT)
	{
		if EDIT
		{
			la_edit(kid,h,p) ;
			la_puthead(p) ;
			la_putfldr(&(p->pf)) ;
			if(!la_encrypt(p->ph.n,&(p->pd),(p->ph.l[4] - p->ph.l[2]),bcs))
			{
				lib$signal(LA_BADENCR) ;
			}
			else
			{
				la_convert(p->ph.cs,bcs) ;
			}
		}
		else if DISP
		{
			la_listpak(p) ;
		}
		la_putmsgu(LA_EMPTY,0,0) ;
		rp[0]= 0 ;
		la_getstr(kid,LA_SAVE,rp,0,1) ;
		la_putmsgu(LA_EMPTY,0,0) ;
		rp[0]= ( rp[0]>='a' ? rp[0]-32 : rp[0] ) ;
	}
	if (SAVE)
	{
		(prol->N)++ ;			/* count of paks ++	*/
		prol->len += p->ph.l[0]	;	/* db file size ++	*/
		prol->lastid = p->pd.lid ;	/* new last license ID  */
		la_putdb (LADB,h) ;	 	/* db back to file	*/
		lm_putmsgu (LA_NEWCNF,0,0) ;
	}
	else if (QUIT)				/* abort without saving	*/
	{
		lm_putmsgu(LA_NOCNF,0,0) ;
	}
	la_freedb(h) ;
	return(status) ;
}
