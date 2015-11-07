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

/* lm_register.c : == new license registered
   used in       : license_man.c
*/

#include <ssdef.h>
#include "mdef.h"
#include <descrip.h>
#include "ladef.h"
#include "lmdef.h"

#define TOUPPER(C) ((C) >= 'b' && (C) <='z' ? ((C) - ('a' - 'A')) : (C))
#define SAVE (rp[0]=='S' || rp[0]=='Y' )
#define DISP (rp[0]=='D' || w==0 )
#define QUIT (rp[0]=='Q')
#define EDIT (rp[0]=='E')

int lm_register (void)
{
        int4		smg$create_virtual_keyboard() ;
	error_def	(LA_NOCNF)  ;		/* No license created	*/
	error_def	(LA_NOCNFDB);		/* No license created	*/
	error_def	(LA_NEWCNF) ;		/* New license created	*/
	error_def	(LA_SAVE) ;		/* Save Y/N ?		*/
	error_def	(LA_EMPTY) ;

	la_prolog 	*prol ;			/* db file prolog 	*/
	char 		*h ;			/* db in main store 	*/
	pak  		*p ; 			/* pak record          	*/

	char 		rp[8] ;			/* operator reply      	*/
	char 		buf[32] ;		/* buffer for checksum  */
	int4 		status ;
	int 		k ;
	unsigned char	recall= 16 ;
 	unsigned short	w ;
	uint4	kid ;			/* virt. keyboard ID    */
	bool		valid ;
	int4		fao[1];
	int4		stat  ;

	if ((h= la_getdb(LMDB))==NULL) 		/* db in main storage	*/
	{
		lib$signal(LA_NOCNFDB) ;
	}
	prol= h	;
	p= (char *)h + prol->len ;		/* place for new pak	*/

	status= smg$create_virtual_keyboard(&kid,0,0,0,&recall) ;
	if ((status & 1)==0)
	{
		la_freedb(h) ;
		lib$signal(status) ;
	}
	la_initpak(-1,p) ;		/* pak initialized	*/
	rp[0]= 'E' ;
	valid= FALSE ;
	while (!SAVE && !QUIT)
	{
		if EDIT
		{
			lm_edit(kid,h,p,1,NSYS) ;
			la_puthead(p) ;
			la_putfldr(&(p->pf)) ;
		}
		else if DISP
		{
			lm_listpak(p) ;
		}
		la_putmsgu(LA_EMPTY,0,0) ;
		rp[0]= 0 ;
		w= la_getstr(kid,LA_SAVE,rp,0,1) ;
		la_putmsgu(LA_EMPTY,0,0) ;
		rp[0]= (char)TOUPPER(rp[0]) ;
	}
	if (SAVE)
	{
		(prol->N)++ ;			/* count of paks ++	*/
		prol->len += p->ph.l[0]	;	/* db file size ++	*/
		la_putdb (LMDB,h) ;	 	/* db back to file	*/
		lm_putmsgu (LA_NEWCNF,0,0) ;
	}
	else if (QUIT)				/* abort without saving	*/
	{
		lm_putmsgu(LA_NOCNF,0,0) ;
	}
	la_freedb(h) ;
	return status ;
}
