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

/* la_getdb  : entire license data base in memory
		or file read error
		or not enough memory allocated
		or bad db file identifier
   la_freedb : frees the store allocated for the data base
 */
#include "mdef.h"
#include <ssdef.h>
#include <rms.h>
#include "ladef.h"
#include "la_io.h"

GBLREF bool 	licensed;
GBLREF int4 	lkid, lid;

 char * la_getdb (char *fn)
/*
 char *fn ;                 			data base file name
*/
{
 	struct 	FAB f ;				/* file access block    */
 	struct 	RAB r ;				/* record access block  */
	struct 	XABFHC x ;			/* file header 		*/
	struct	NAM n;				/*  added nam block to capture file id  (js)  */

	int4		status ;
	int4 		size   ;
	int 		k ;
	char 		*h= NULL ;		/* db in main store 	*/
	char 		*p ;			/* current read buffer  */
	la_prolog	*prol ;			/* db file prolog	*/

	n = cc$rms_nam;
	status= bopen(fn,&f,&r,&x,&n);
	if (status==SS$_NORMAL)
	{
		size= 512 * x.xab$l_ebk + PBUF ;
		if ((h= (char *)malloc(size))!=NULL)
		{
			p= h ;
			while (status==SS$_NORMAL && p<h+size)
		        {
				status= bread(&r,p,BLKS) ; p += BLKS  ;
			}
			if (status!=RMS$_EOF)
			{
				h= NULL ;
			}
			else
			{
				prol= h ;
				if (prol->id!=DBID)	/* invalid file ID */
				{
					h= NULL ;
				}
			}
		}
		bclose(&f) ;
	}
	else
	{
		h= NULL ;
	}
	return(h) ;
}

 void la_freedb (h)
 char *h ;                 		/* data base in memory  */
{
	if (h!=NULL)
	{
		free(h) ;
	}
}
