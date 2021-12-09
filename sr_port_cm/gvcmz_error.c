/****************************************************************
 *								*
 * Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "callg.h"

GBLREF gd_region *gv_cur_region;
GBLREF gv_key *gv_currkey;

void gvcmz_error(char code, uint4 status)
{
	void		**ptr;
	gparam_list	err_plist;
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

	ASSERT_IS_LIBGNPCLIENT;
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
	ptr = &err_plist.arg[0];
	gv_error = TRUE;
	switch(code)
	{
	case CMMS_Q_DATA:
		*ptr++ = (void *)ERR_GVDATAFAIL;
		break;
	case CMMS_Q_GET:
		*ptr++ = (void *)ERR_GVGETFAIL;
		break;
	case CMMS_Q_ZWITHDRAW:
	case CMMS_Q_KILL:
		*ptr++ = (void *)ERR_GVKILLFAIL;
		break;
	case CMMS_Q_ORDER:
		*ptr++ = (void *)ERR_GVORDERFAIL;
		break;
	case CMMS_Q_PUT:
		*ptr++ = (void *)ERR_GVPUTFAIL;
		break;
	case CMMS_Q_QUERY:
		*ptr++ = (void *)ERR_GVQUERYFAIL;
		break;
	case CMMS_Q_PREV:
		*ptr++ = (void *)ERR_GVZPREVFAIL;
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
		*ptr++ = (void *)ERR_NETLCKFAIL;
		*ptr++ = (void *)0;
		gv_error = FALSE;
		break;
	default:
		*ptr++ = (void *)ERR_NETFAIL;
		*ptr++ = (void *)0;
		gv_error = FALSE;
		break;
	}
	if (gv_error)
	{
		*ptr++ = (void *)2;
		*ptr++ = (void *)9;
		*ptr++ = (void *)"Net error";
	}
	*ptr++ = (void *)(INTPTR_T)status;
	err_plist.n = ptr - &err_plist.arg[0];
	gvcmz_neterr(&err_plist);
}
