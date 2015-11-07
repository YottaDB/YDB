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

/* lm_getnid.c: assigns a unique node IDs for each hardware model in the list.
		The nodes are selected to match the hardware models.
		The computed value can be overwritten interactively.
		For standalone machines nid[k]==0
   used in    : lm_edit.c
*/
#include "mdef.h"
#include <ssdef.h>
#include <syidef.h>
#include <efndef.h>

#include "ladef.h"
#include "lmdef.h"
#define MINT	0x0FFFFFFF
#define LAST	(status==SS$_NOMORENODE)
#define PROPER(e) if (e!=SS$_NORMAL && !LAST) { la_putmsgs(e) ; }

void lm_getnid (uint4 kid,int4 nid[],int4 sid[],int4 n)
/*
uint4	kid ;					virt. keyb. ID
int4		nid[] ;				array of node IDs
int4		sid[] ;				array of hw. models
int4		n ; 				array size
*/
{
	int4	sys$getsyiw() ;
	error_def(LA_PNID) ;
	error_def(LA_NOSYS) ;
	int4	mdl= 0 ;				/* hw. model		*/
	int4	nd= 0 ;					/* node ID 		*/
	int4	inid ;					/* initial node id	*/
	int 	i,k ;
	int4 	iosb[2],status ;
	bool	valid ;
	char	buf[32] ;
	int4	fao[2] ;
	int4	tmp[16] ;
	short	w ;
	unsigned char cmem ;
	struct
	{	short blen ;			/* buffer length 	*/
		short code ;			/* item code		*/
		char  *buf ;			/* return buffer	*/
		short *len ;			/* return length	*/
	} itm[4] ;

	itm[0].blen= 1        	   	 ; itm[1].blen= 0 ;
	itm[0].code= SYI$_CLUSTER_MEMBER ; itm[1].code= 0 ;
	itm[0].buf = &cmem		 ; itm[1].buf = 0 ;
	itm[0].len = &w    		 ; itm[1].len = 0 ;

	status= sys$getsyiw(EFN$C_ENF,0,0,itm,iosb,0,0) ;
	if (( cmem & 1 )==0)
	{
		inid = 0 ;
		status= lm_mdl_nid(&mdl,&nd,&inid) ;
		inid = -1 ;
	}
	else
	{
		inid= -1 ;
		status= lm_mdl_nid(&mdl,&nd,&inid) ;
	}
	for ( k = 0 ; k!=n ; k++ )
	{
		tmp[k] = nid[k] ; nid[k] = 0 ;
	}
	k= 0 ;
	while ( k!=n && status!=SS$_NOMORENODE )
	{
		i = 0 ;
		while (((sid[i]!=mdl && sid[i]!=0) || nid[i]!=0) && i!=n) i++ ;
		if (i!=n)
		{
			nid[i] = nd ;
		}
		status= lm_mdl_nid(&mdl,&nd,&inid) ;
		PROPER(status) ;
		k++ ;
	}
	k = 0 ;
	while (k!=n)
	{
		if (nid[k]==0 && tmp[k]==0)
		{
			fao[0]= la_mdl2nam(buf,sid[k]) ; fao[1]= buf ;
			lm_putmsgu(LA_NOSYS,fao,2) ;
		}
		else if (nid[k]==0)
		{
			nid[k] = tmp[k] ;
		}
		la_getnum(kid,LA_PNID,(nid+k),0,MINT) ;
		k++ ;
	}
}
