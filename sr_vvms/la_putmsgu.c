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

/* la_putmsgu.c : outputs and formats message for user message code
		  and user FAO arguments
 */
#include "mdef.h"

#include <ssdef.h>

void la_putmsgu (c, fao, n)
int4	c ;				/* message code		    */
int4	fao[] ;				/* fao arguments	    */
short	n ;				/* number of fao args	    */
{
	int	k, local_n;
	struct { short	argc;		/* structure longword count */
                 short	opt;		/* message display options  */
                 int4	code;		/* message code 	    */
	         short	count;		/* FAO count    	    */
	         short	newopt;		/* new options  	    */
	         int4	fao[16];	/* fao arguments	    */
               } msgvec ;

	local_n = n;
	if (local_n < 0)
		local_n = 0;
	if (local_n > 15)
		local_n = 15;

	msgvec.argc   = local_n + 2;	/* number of longwords (excluding argc, opt) */
	msgvec.opt    = 0x0001;	/* include message text; do not include mnemonic name, severity level, or facility prefix */
	msgvec.code   = c;
	msgvec.count  = local_n;
	msgvec.newopt = msgvec.opt;	/* no change */

	for (k = 0;  k < local_n;  k++)
		msgvec.fao[k] = fao[k];
	sys$putmsg(&msgvec) ;
}
