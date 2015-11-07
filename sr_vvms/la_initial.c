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

/* la_initial.c: == data base file initialized
 */

#include "mdef.h"
#include <climsgdef.h>
#include <ssdef.h>
#include <psldef.h>
#include <rms.h>
#include <lnmdef.h>
#include <descrip.h>
#include "gtm_string.h"
#include "ladef.h"
#include "lmdef.h"
#include "la_io.h"

int la_initial (char *ln[])
/*
char	*ln[] ;					db logical name
*/
{
        int4		cli$get_getvalue(),sys$trnlnm();
	error_def	(LA_DBINIT) ; 		/* License db init. 	*/
	error_def	(LA_DBEXIS) ; 		/* Lic. db exists allready */
	la_prolog 	*h ;			/* data base prolog 	*/
	int4		status ;		/* status           	*/
	int4		fao[2] ;
	char		res[128] ;		/* resulting file name 	*/
	char		exp[255];		/* expanded file name	*/
	int4		expl;			/*  ... length		*/
	char 		buf[BLKS] ;
	$DESCRIPTOR	(dtbp,"LNM$JOB") ;
	$DESCRIPTOR	(dtbs,"LNM$FILE_DEV") ;
	$DESCRIPTOR	(dlnm,ln[0]) ;
	$DESCRIPTOR	(dres,res) ;
	struct FAB 	fab ;
	struct RAB 	rab ;
	struct NAM	nam;
	struct XABPRO 	xab;
	int4		ctx= 0 ;
	int4		iosb ;
	char 		acmo= PSL$C_USER ;
	short		w ;
	struct
	{
		short bln;
		short cod;
		int4  bdr;
		int4  rln;
	} itm[3] ;

	dlnm.dsc$w_length= strlen(ln[0]) ;
	status= sys$trnlnm(0,&dtbs,&dlnm,0,0) ;
	if (status==SS$_NOLOGNAM)
	{
		itm[0].bln= strlen(ln[1])	; itm[1].bln= 0 ;
		itm[0].cod= LNM$_STRING		; itm[1].cod= 0 ;
		itm[0].bdr= ln[1] 		; itm[1].bdr= 0 ;
		itm[0].rln= &w 			; itm[1].rln= 0 ;
		status= sys$crelnm(0,&dtbp,&dlnm,&acmo,itm) ;
	}
	if (status==SS$_NORMAL)
	{
		status= lib$find_file(&dlnm,&dres,&ctx,0,0,&iosb,0) ;
		if (status==RMS$_NMF || status==RMS$_FNF)
		{
			status= SS$_NORMAL ;
		}
		else if (status==SS$_NORMAL || status==RMS$_NORMAL)
		{
			status= LA_DBEXIS ;
		}
	}
	if (status==SS$_NORMAL)			/* file name received in f  */
	{
		h= buf ;			/* prolog positioned in buf */
	        h->id= DBID ;			/* db file assigned its ID  */
		h->N= 0 ;
		h->len= SIZEOF(la_prolog) ;	/* prolog data filled in    */
		nam = cc$rms_nam;
		nam.nam$l_esa = &exp;
		nam.nam$b_ess = SIZEOF(exp);
		status= bcreat(ln[0],&fab,&rab,&nam,&xab,ALOC) ;
		if (status==SS$_NORMAL)
		{
			status= bwrite(&rab,buf,BLKS) ;
		}
		bclose(&fab) ;
	}
	if (status==SS$_NORMAL)
	{
/*		fao[0]= strlen(ln[0]) ; fao[1]= ln[0] ;		*/
		fao[0]= nam.nam$b_esl; fao[1]= &exp;
		lm_putmsgu(LA_DBINIT,fao,2) ;
	}
        else
	{
		lib$signal(status) ;
	}
	return status ;
}
