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

/* la_putdb : stores entire license data base from the main store back in db file */

#include "mdef.h"
#include <ssdef.h>
#include <descrip.h>
#include <rms.h>
#include "ladef.h"
#include "la_io.h"

#define B (e==SS$_NORMAL || e==RMS$_NORMAL)

 void la_putdb (char *fn,char *h)
/* fn - data base file name  */
/* h  - data base	*/
{
 	struct FAB f,fd ;			/* file access block    */
 	struct RAB r,rd ;			/* record access block  */
	struct XABFHC x ;			/* file header 		*/
	struct NAM nam, nam2;
	struct XABPRO xab, xab2;

	int sys$erase() ;

	int e,len ;
	la_prolog *prol ;			/* data base prolog 	*/
	char *p ;

	nam2 = cc$rms_nam;
	breopen(fn,&fd,&rd,&x,&nam2);		/* this should actually be "reopen" */
	nam = cc$rms_nam;
	e= bcreat(fn,&f,&r,&nam,&xab,ALOC) ; if (!B) lib$signal(e) ;
	if B
	{
		p= prol= h ;
		len= prol->len ;		/* file length in bytes */
		while (B && len>0 )
		{
			e   =  bwrite(&r,p,BLKS) ;
			p   += BLKS ;
			len -= BLKS ;
		}
		bclose(&f) ;
		if (!B)
		{
			lib$signal(e) ;
		}
		else
		{
#if 0
			bdelete(&fd,&xab2,&nam2);	/* delete old config file */
#else
			bdelete(&fd,&xab2);	/* delete old config file */
#endif
		}
	}
	else
	{
		lib$signal(e) ;
	}
}
