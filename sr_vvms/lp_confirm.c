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

/* lp_confirm.c : lp_confirm == SS$_NORMAL when the product acquired license
		  units
   used in      : Licensed products
 */
#include "mdef.h"
#include <ssdef.h>
#include <lkidef.h>
#include <efndef.h>
#include "ladef.h"

#define PROPER(stat) { if (stat==SS$_IVLOCKID) return LP_NOTACQ ;\
		       else if ((stat & 1)==0) return stat ;}

GBLREF int4	process_id ;

int4 lp_confirm (int4 lid, uint4 lkid)
/*
int4	 lid  ;			        license ID
uint4	 lkid ;				lock ID
*/
{
	error_def	(LP_NOTACQ) ;
	struct { short bln ; short cod ; char *buf ; int4 *rln ;} itm[3] ;
	int4 		status ;
	int4 		iosb[2] ;
	int4 		parid,len,len1 ;
	int4		res ;			/* parent lock resource name */

	itm[0].bln= 4 ; itm[0].cod= LKI$_PARENT ; itm[0].buf= &parid ; itm[0].rln= &len ;
	itm[1].bln= 4 ; itm[1].cod= LKI$_RESNAM ; itm[1].buf= &res   ; itm[1].rln= &len1 ;
	itm[2].bln = itm[2].cod = 0; itm[2].buf = itm[2].rln = 0;

	if (lkid!=0 && lid!=0)
	{
		status= gtm_getlkiw(EFN$C_ENF,&lkid,itm,iosb,0,0,0) ;
		PROPER(status) ;
		if (parid!=0 && process_id==res)
		{
			status= gtm_getlkiw(EFN$C_ENF,&parid,&itm[1],iosb,0,0,0) ;
			PROPER(status) ;
			status= ( lid==res ? SS$_NORMAL : LP_NOTACQ ) ;
		}
		else
		{
			status= LP_NOTACQ ;
		}
	}
	else
	{
		status= LP_NOTACQ ;
	}
	return status ;
}
