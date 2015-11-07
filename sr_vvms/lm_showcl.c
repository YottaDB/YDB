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

/* lm_showcl : displays node name, node number and hw. model for all systems
	       in the cluster
   used in   : lmu
*/
#include "mdef.h"
#include <ssdef.h>
#include <syidef.h>
#include <efndef.h>

#include "ladef.h"
#include "lmdef.h"
#define NAML	15

int4	lm_showcl(void)
{
	int 	sys$getsyiw() ;
	error_def(LA_SHOWCL) ;
	error_def(LA_HEADCL) ;
	error_def(LA_DELICL) ;
	char	nam[32] ;
	char	buf[15] ;
	short	mdl = 0 ;
	int4	nid ;
	int4	csid ;
	char	snam[] = "Standalone" ;
	struct
	{
		short blen ;			/* buffer length 	*/
		short code ;			/* item code		*/
		char  *buf ;			/* return buffer	*/
		short *len ;			/* return length	*/
	} itm[4] ;

	int4 		iosb[2],status ;
	int4		fao[5] ;
	short		w0,w1,w2 ;
	unsigned char	cmem = 0 ;

	la_putmsgu(LA_HEADCL,0,0) ;
	la_putmsgu(LA_DELICL,0,0) ;

	itm[0].blen= 2 		   ; itm[1].blen= 1          	   ;  itm[2].blen= 0 ;
	itm[0].code= SYI$_HW_MODEL ; itm[1].code= SYI$_CLUSTER_MEMBER;itm[2].code= 0 ;
	itm[0].buf = &mdl          ; itm[1].buf= &cmem		   ;  itm[2].buf = 0 ;
	itm[0].len = &w0           ; itm[1].len=  &w1       	   ;  itm[2].len = 0 ;

	status= sys$getsyiw(EFN$C_ENF,0,0,itm,iosb,0,0) ;
	if (status==SS$_NORMAL && (cmem & 1)==0)
	{
		itm[0].blen= 4        		; itm[1].blen= 0 ;
		itm[0].code= SYI$_NODE_NUMBER   ; itm[1].code= 0 ;
		itm[0].buf= &nid		; itm[1].buf=  0 ;
		itm[0].len=  &w1       		; itm[1].len=  0 ;
		status= sys$getsyiw(EFN$C_ENF,0,0,itm,iosb,0,0) ;
		if (status!=SS$_NORMAL)
		{
			nid= 0 ;
			status= SS$_NORMAL ;
		}
		w0= la_mdl2nam(buf,(int4)mdl) ;
		fao[0]= SIZEOF(snam)-1 ; fao[1]= snam ; fao[2]= nid ; fao[3]= w0 ; fao[4]= buf ;
		la_putmsgu(LA_SHOWCL,fao,5) ;
	}
	else if ((cmem & 1)==1)
	{
		itm[1].blen= 4        		; itm[2].blen= 15  	     ; itm[3].blen= 0 ;
		itm[1].code= SYI$_NODE_NUMBER   ; itm[2].code= SYI$_NODENAME ; itm[3].code= 0 ;
		itm[1].buf= &nid		; itm[2].buf=  nam	     ; itm[3].buf = 0 ;
		itm[1].len=  &w1       		; itm[2].len=  &w2           ; itm[3].len = 0 ;
		csid= -1 ;
		status= sys$getsyiw(EFN$C_ENF,&csid,0,itm,iosb,0,0) ;
		while (status==SS$_NORMAL)
		{
			w0= la_mdl2nam(buf,(int4)mdl) ;
			fao[0]= w2 ; fao[1]= nam ; fao[2]= nid ; fao[3]= w0 ; fao[4]= buf ;
			la_putmsgu(LA_SHOWCL,fao,5) ;
			status= sys$getsyiw(EFN$C_ENF,&csid,0,itm,iosb,0,0) ;
		}
	}
	return ( status==SS$_NOMORENODE ? SS$_NORMAL : status ) ;
}
