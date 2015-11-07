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

/* la_showpak.c: shows pak summary to stdout
   js / 20-sep-1989
 */
#include "mdef.h"
#include <ssdef.h>
#include <descrip.h>
#include "gtm_string.h"
#include "ladef.h"

void la_showpak (q)
char *q ;                               /* buffer with the pak record 	*/
{
	pak	*p ;			/* pak record 		*/
	char	*padr ;			/* pak address		*/
	$DESCRIPTOR(faoctl, "!5SL!AS  !8AD !11%D !12AD  !37AD");
	char	line[256];
	int4	length;
	char	status[2] = " ";
	$DESCRIPTOR(linedesc, line);
	$DESCRIPTOR(dstatus, status);
	int4	today, exday;

	p= q ;
	padr= q + p->ph.l[5] ;

	if (p->pd.t1[1]!=0) {
	  lib$day (&today, 0);
	  lib$day (&exday, p->pd.t1);
	  if (today > exday) status[0] = '+';
	}

	if (p->ph.cs[0]=='0')
	  if (status[0]=='+')
	    status[0] = '%';
	  else
	    status[0] = '*';

	sys$fao (&faoctl, &length, &linedesc, p->pd.lid, &dstatus, LEN_AND_STR(p->pd.nam), &(p->pf.std),
		 LEN_AND_STR(p->pf.oid), LEN_AND_STR(padr));
	linedesc.dsc$w_length = length;
	lib$put_output (&linedesc);

	return;
}
