/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_string.h"
#include "gvcmz.h"

GBLREF gd_region *gv_cur_region;
GBLREF gv_key *gv_currkey;

void gvcmz_error(char code, uint4 status)
{
	INTPTR_T	err[6], *ptr;
	bool		gv_error;

	error_def(ERR_GVDATAFAIL);
	error_def(ERR_GVGETFAIL);
	error_def(ERR_GVKILLFAIL);
	error_def(ERR_GVORDERFAIL);
	error_def(ERR_GVQUERYFAIL);
	error_def(ERR_GVPUTFAIL);
	error_def(ERR_GVZPREVFAIL);
	error_def(ERR_NETLCKFAIL);
	error_def(ERR_NETFAIL);

	switch(code)
	{
		case CMMS_L_LKCANALL:
		case CMMS_L_LKCANCEL:
		case CMMS_L_LKDELETE:
		case CMMS_L_LKREQIMMED:
		case CMMS_L_LKREQNODE:
		case CMMS_L_LKREQUEST:
		case CMMS_L_LKRESUME:
		case CMMS_L_LKACQUIRE:
		case CMMS_L_LKSUSPEND:
		case CMMS_U_LKEDELETE:
		case CMMS_U_LKESHOW:
			break;
		default:
			gv_cur_region->open = FALSE;
			gv_cur_region->dyn.addr->acc_meth = dba_bg;
			memset(gv_currkey->base,0,gv_currkey->end + 1);
			break;
	}
	ptr = &err[1];
	gv_error = TRUE;
	switch(code)
	{
	case CMMS_Q_DATA:
		*ptr++ = ERR_GVDATAFAIL;
		break;
	case CMMS_Q_GET:
		*ptr++ = ERR_GVGETFAIL;
		break;
	case CMMS_Q_ZWITHDRAW:
	case CMMS_Q_KILL:
		*ptr++ = ERR_GVKILLFAIL;
		break;
	case CMMS_Q_ORDER:
		*ptr++ = ERR_GVORDERFAIL;
		break;
	case CMMS_Q_PUT:
		*ptr++ = ERR_GVPUTFAIL;
		break;
	case CMMS_Q_QUERY:
		*ptr++ = ERR_GVQUERYFAIL;
		break;
	case CMMS_Q_PREV:
		*ptr++ = ERR_GVZPREVFAIL;
		break;
	case CMMS_L_LKCANALL:
	case CMMS_L_LKCANCEL:
	case CMMS_L_LKDELETE:
	case CMMS_L_LKREQIMMED:
	case CMMS_L_LKREQNODE:
	case CMMS_L_LKREQUEST:
	case CMMS_L_LKRESUME:
	case CMMS_L_LKACQUIRE:
	case CMMS_L_LKSUSPEND:
	case CMMS_U_LKEDELETE:
	case CMMS_U_LKESHOW:
		*ptr++ = ERR_NETLCKFAIL;
		*ptr++ = 0;
		gv_error = FALSE;
		break;
	default:
		*ptr++ = ERR_NETFAIL;
		*ptr++ = 0;
		gv_error = FALSE;
		break;
	}
	if (gv_error)
	{
		*ptr++ = 2;
		*ptr++ = 9;
		*ptr++ = (INTPTR_T)"Net error";
	}
	*ptr++  = status;
	err[0] = ptr - err - 1;
	gvcmz_neterr(&err[0]);
}
