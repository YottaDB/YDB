/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_multi_thread.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "error.h"
#include "util.h"
#include "io.h"
#include "gtm_string.h"
#include "gvcmy_close.h"
#include "gvcmz.h"
#include "copy.h"

GBLREF boolean_t 	created_core;
GBLREF boolean_t	dont_want_core;

error_def(ERR_ASSERT);
error_def(ERR_BADSRVRNETMSG);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_MEMORY);

void gvcmz_errmsg(struct CLB *c, bool close)
{
	unsigned char	*bufptr, msgnum;
	short		msglen;
	boolean_t	cont;
	uint4		status;
	cmi_descriptor	*desc;
	link_info 	*li;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	li = (link_info *)c->usr;
	util_out_print(NULL, RESET);
	flush_pio();
	bufptr = c->mbf;
	assert(CMMS_E_ERROR == *bufptr);
	++bufptr;
	cont = *bufptr++;
	msgnum = *bufptr++;
	if (1 != msgnum)	/* Need to start with msg 1 for severity and signal values */
	{
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_BADSRVRNETMSG);
	}
	CM_GET_SHORT(msglen, bufptr, li->convert_byteorder);
	bufptr += SIZEOF(short);
	CM_GET_LONG(SIGNAL, bufptr, li->convert_byteorder);
	bufptr += SIZEOF(SIGNAL);
	CM_GET_LONG(SEVERITY, bufptr, li->convert_byteorder);
	bufptr += SIZEOF(SEVERITY);
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	memcpy(TREF(util_outptr), bufptr, msglen);
	TREF(util_outptr) += msglen;
	while(cont)
	{
		status = cmi_read(c);
		if (CMI_ERROR(status))
			break;
		bufptr = c->mbf;
		if (CMMS_E_ERROR != *bufptr++)
			break;
		cont = *bufptr++;
		if (++msgnum != *bufptr++)
		{
			assert(FALSE);
			break;
		}
		CM_GET_SHORT(msglen, bufptr, li->convert_byteorder);
		bufptr += SIZEOF(short);
		memcpy(TREF(util_outptr), bufptr, msglen);
		TREF(util_outptr) += msglen;
	}

	if (close)
		gvcmy_close(c);

	if (DUMPABLE)
		created_core = dont_want_core = FALSE;
	DRIVECH(NULL);
}
