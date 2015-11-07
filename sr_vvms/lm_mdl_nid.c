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

/* lm_mdl_nid.c: given is csid.
		 csid>0    mdl,nid are returned for specific csid.
		 csid==-1, mdl,nid are returned for all systems
		 in the cluster - one pair per call
		 csid==0   mdl,nid are returned for local system
		 when the system is standalone nid==0
   used in     : lm_getnid.c
*/
#include "mdef.h"
#include <ssdef.h>
#include <syidef.h>
#include <efndef.h>

#include "ladef.h"

int4	lm_mdl_nid (mdl,nid,csid)
int4	*mdl ;					/* returns - hardware model */
int4 	*nid ;					/* returns - node number    */
int4 	*csid ;					/* cluster system ID	    */
{
	int4	sys$getsyiw() ;
	struct
	{
		short blen ;			/* buffer length 	*/
		short code ;			/* item code		*/
		char  *buf ;			/* return buffer	*/
		short *len ;			/* return length	*/
	} itm[4] ;

	int4 		iosb[2],status ;
	short		w,u ;

	itm[0].blen= 2            ; itm[1].blen= 4        	   ; itm[2].blen= 0 ;
	itm[0].code= SYI$_HW_MODEL; itm[1].code= SYI$_NODE_NUMBER  ; itm[2].code= 0 ;
	itm[0].buf = mdl          ; itm[1].buf=  nid	  	   ; itm[2].buf = 0 ;
	itm[0].len = &w           ; itm[1].len=  &u       	   ; itm[2].len = 0 ;

	status= sys$getsyiw(EFN$C_ENF,csid,0,itm,iosb,0,0) ;
	return status ;
}
