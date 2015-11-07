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

/* iorm_jbc.c
   used in: iorm_close.c
*/
#include <rms.h>
#include "mdef.h"
#include "io.h"
#include <ssdef.h>
#include <sjcdef.h>
#include <iodef.h>
#include <efndef.h>

#include "io_params.h"
#define	IDENDIM	 28
LITREF unsigned char io_params_size[];

int4 iorm_jbc(struct NAM *nam, mval *pp, mstr *que, bool delete)
/* *p   :		 parameter list  */
/* *que :		 default que	   */
/* delete :		 delete file	   */
{
	int4 	sys$sndjbc(), sys$parse(), sys$search() ;
	int4 	status  ;
	int4	iosb[2] ;
	int4	temp	;
	int 	p_offset ;
	int 	k ;
	unsigned char c ;
	struct  { short	bln ; short cod ; char *buf ; int4 rln ;}  itm[64] ;

	itm[0].bln= IDENDIM ;
	itm[0].cod= SJC$_FILE_IDENTIFICATION  ;
	itm[0].buf= &(nam->nam$t_dvi) ;
	itm[0].rln= 0 ;

	itm[1].bln= que->len  ;
	itm[1].cod= SJC$_QUEUE;
	itm[1].buf= que->addr ;
	itm[1].rln= 0 ;

	p_offset = 0 ; k= 2 ; c = *(pp->str.addr + p_offset++) ;
	while (c != iop_eol)
	{
		if (io_params_size[c]==IOP_VAR_SIZE)
		{
			itm[k].bln= (short) *(pp->str.addr + p_offset) ;
			itm[k].buf= (pp->str.addr + p_offset + 1) ;
			p_offset += (*(pp->str.addr + p_offset) + 1);
		}
		else
		{
			itm[k].bln= io_params_size[c] ;
			itm[k].buf= pp->str.addr + p_offset;
			p_offset += io_params_size[c];
		}
		itm[k].rln= 0 ;
		switch (c)
		{
		case iop_after:	itm[k].cod= SJC$_AFTER_TIME ;
				break ;
		case iop_burst:	itm[k].cod= SJC$_FILE_BURST ;
				break ;
		case iop_characteristic:
				itm[k].cod= SJC$_CHARACTERISTIC_NUMBER  ;
				break ;
		case iop_cli: itm[k].cod= SJC$_CLI ;
				break ;
		case iop_copies: itm[k].cod= SJC$_FILE_COPIES ;
				break ;
		case iop_cpulimit: itm[k].cod= SJC$_CPU_LIMIT ;
				break ;
		case iop_doublespace:	itm[k].cod= SJC$_DOUBLE_SPACE ;
				break ;
		case iop_firstpage: itm[k].cod= SJC$_FIRST_PAGE ;
				break ;
		case iop_flag:	itm[k].cod= SJC$_FILE_FLAG ;
				break ;
		case iop_form:	itm[k].cod= SJC$_FORM_NUMBER ;
				break ;
		case iop_header: itm[k].cod= SJC$_PAGE_HEADER ;
				break ;
		case iop_hold:	itm[k].cod= SJC$_HOLD ;
				break ;
		case iop_lastpage: itm[k].cod= SJC$_LAST_PAGE ;
				break ;
		case iop_logfile: itm[k].cod= SJC$_LOG_SPECIFICATION ;
				break ;
		case iop_logqueue: itm[k].cod= SJC$_LOG_QUEUE ;
				break ;
		case iop_lowercase: itm[k].cod= SJC$_LOWERCASE ;
				break ;
		case iop_name:	itm[k].cod= SJC$_JOB_NAME ;
				break ;
		case iop_noburst: itm[k].cod= SJC$_NO_FILE_BURST ;
				break ;
		case iop_nodoublespace: itm[k].cod= SJC$_NO_DOUBLE_SPACE ;
				break ;
		case iop_noflag: itm[k].cod= SJC$_NO_FILE_FLAG ;
				break ;
		case iop_noheader: itm[k].cod= SJC$_NO_PAGE_HEADER ;
				break ;
		case iop_nohold: itm[k].cod= SJC$_NO_HOLD ;
				break ;
		case iop_nolowercase: itm[k].cod= SJC$_NO_LOWERCASE  ;
				break ;
		case iop_nonotify: itm[k].cod= SJC$_NO_NOTIFY ;
				break ;
		case iop_nopage: itm[k].cod= SJC$_NO_PAGINATE ;
				break ;
		case iop_nopassall: itm[k].cod= SJC$_NO_PASSALL ;
				break ;
		case iop_noprint: itm[k].cod= SJC$_NO_LOG_SPOOL ;
				k++ ;
				itm[k].bln= 0 ;
				itm[k].cod= SJC$_LOG_DELETE ;
				itm[k].buf= 0 ;
				itm[k].rln= 0 ;
				break ;
		case iop_norestart: itm[k].cod= SJC$_NO_RESTART ;
				break ;
		case iop_note:	itm[k].cod= SJC$_NOTE ;
				break ;
		case iop_notify: itm[k].cod= SJC$_NOTIFY ;
				break ;
		case iop_notrailer: itm[k].cod= SJC$_NO_FILE_TRAILER ;
				break ;
		case iop_operator: itm[k].cod= SJC$_OPERATOR_REQUEST ;
				break ;
		case iop_p1: itm[k].cod=SJC$_PARAMETER_1 ;
				break ;
		case iop_p2: itm[k].cod=SJC$_PARAMETER_2 ;
				break ;
		case iop_p3: itm[k].cod=SJC$_PARAMETER_3 ;
				break ;
		case iop_p4: itm[k].cod=SJC$_PARAMETER_4 ;
				break ;
		case iop_p5: itm[k].cod=SJC$_PARAMETER_5 ;
				break ;
		case iop_p6: itm[k].cod=SJC$_PARAMETER_6 ;
				break ;
		case iop_p7: itm[k].cod=SJC$_PARAMETER_7 ;
				break ;
		case iop_p8: itm[k].cod=SJC$_PARAMETER_8 ;
				break ;
		case iop_page: itm[k].cod= SJC$_PAGINATE ;
				break ;
		case iop_passall: itm[k].cod= SJC$_PASSALL ;
				break ;
		case iop_print: itm[k].cod= SJC$_LOG_SPOOL ;
				k++ ;
				itm[k].bln= 0 ;
				itm[k].cod= SJC$_NO_LOG_DELETE ;
				itm[k].buf= 0 ;
				itm[k].rln= 0 ;
				break ;
		case iop_priority: itm[k].cod= SJC$_PRIORITY ;
				break ;
		case iop_queue: itm[1].bln= itm[k].bln ;
				itm[1].buf= itm[k].buf ;
				--k ;
				break ;
		case iop_remote: itm[k].cod= SJC$_SCSNODE_NAME ;
				break ;
		case iop_restart: itm[k].cod= SJC$_RESTART ;
				break ;
		case iop_setup:	itm[k].cod= SJC$_FILE_SETUP_MODULES ;
				break ;
		case iop_trailer: itm[k].cod= SJC$_FILE_TRAILER ;
				break ;
		case iop_uic:	itm[k].cod= SJC$_UIC ;
				break ;
		case iop_user:	itm[k].cod= SJC$_USERNAME ;
				break ;
		default: 	--k ;
				break ;
		}
		k++ ;
		c= *(pp->str.addr + p_offset++) ;
	}
	if (delete)
	{
		itm[k].bln= 0 ;
		itm[k].cod= SJC$_DELETE_FILE ;
		itm[k].buf= 0 ;
		itm[k].rln= 0 ;
		k++ ;
	}
	itm[k].bln = itm[k].cod = 0;
	itm[k].buf = itm[k].rln = 0 ;
	status= sys$sndjbcw(EFN$C_ENF,SJC$_ENTER_FILE,0,itm,iosb,0,0) ;
	if ((status & 1)==1)
	{
		status= iosb[0] ;
	}
	return status ;
}
