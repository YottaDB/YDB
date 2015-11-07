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

/* la_maint.c : License Adm. function completing maintenance commands */

#include "mdef.h"
#include <climsgdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <rms.h>
#include "ladef.h"
#include "la_io.h"
#include "la_writepak.h"

#define B    (e==CLI$_NORMAL||e==SS$_NORMAL)
#define OK   (rep[0]=='Y' || rep[0]=='T' || rep[0]=='1')
#define QUIT (rep[0]=='Q')

int la_maint (void)
{
        int4	cli$get_getvalue();

	error_def(LA_NOCNFDB) ;
	error_def(LA_VOID) ;
	error_def(LA_VOIDED) ;
	error_def(LA_EMPTY)  ;
	int e ;
 	unsigned short w ;
	bool voi,cli;					/* a license voided */
	char com[64] ;					/* command 	    */
	char io[64]= "PAK.LIS" ;			/* io file name     */
	char rep[64] ;					/* reply	    */

        $DESCRIPTOR (dentv,"$VERB");
        $DESCRIPTOR (dcom,com);
        $DESCRIPTOR (denti,"io");
        $DESCRIPTOR (dio,io);

	uint4 kid ;				/* virt. keyb. ID  */

	la_prolog *prol;				/* db prolog	     */
	char *h	;					/* data base 	     */
	pak  *p ;					/* pak record	     */
	pak q[PBUF] ;					/* pak pattern       */
	int v[32] ;					/* qualif. variables */
	int   n,k ;					/* pak rec count     */
	struct FAB f ;
	struct RAB r ;

	e= smg$create_virtual_keyboard(&kid) ; if (!B) lib$signal(e) ;
        e= cli$get_value(&dentv,&dcom,&w)    ; if (!B) lib$signal(e) ;
        com[w]= 0;

	cli = la_getcli(v,q) ;
	denti.dsc$w_length= 2 ;
	e= cli$present(&denti) ;
	if B
	{
		e= cli$get_value(&denti,&dio,&w);
	        io[w]= 0;
	}
	la_puthead(q)  ;			/* pak pattern filled in  */
	if ((h= la_getdb(LADB))==NULL)
	{
		lib$signal(LA_NOCNFDB) ;
	}
	prol= h ;
	p= h + SIZEOF(la_prolog) ;

	if ((com[0]=='S')||(com[0]=='s')) {
	  $DESCRIPTOR (head1, "PAK #   Product  Created on  Created by    Customer name");
	  $DESCRIPTOR (head2, "-----   -------  ----------- ------------  -------------------------------------");
	  lib$put_output (&head1);
	  lib$put_output (&head2);
	}

	n= 0 ;
	voi= FALSE ;
	rep[0]= 0 ;
	while (n != prol->N && !QUIT)
	{
		if ((!cli && n==prol->N-1) || (cli && la_match(p,q,v)))
		{
			switch ((com[0]>='a' ? com[0]-32 : com[0]))
			{
			case 'L' : la_listpak(p) ;
				   break ;
			case 'P' : e= vcreat(io,&f,&r,0) ; if (!B) lib$signal(e) ;
				   la_writepak(&r,p) ;
				   bclose(&f) ;
				   break ;
			case 'S' : la_showpak(p);
				   break;
			case 'V' : la_listpak(p) ;
				   la_putmsgu(LA_EMPTY, 0, 0);
				   rep[0]= 0 ;
				   la_getstr(kid,LA_VOID,rep,0,32) ;
				   rep[0]= (rep[0]>='a' ? rep[0]-32 : rep[0]) ;
				   if (OK)
				   {
					for (k= 0;k!=16;k++) p->ph.cs[k]= '0' ;
				        la_putmsgu(LA_VOIDED,0,0) ;
					voi= TRUE ;
				   }
				   break ;
			otherwise: ;
			}
		}
		n++ ;
		p = (char *)p + p->ph.l[0] ;
	}
	if (voi)
	{
		la_putdb(LADB,h) ;
	}
	la_freedb(h) ;
	return e ;
}
