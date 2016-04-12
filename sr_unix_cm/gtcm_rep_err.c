/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_rep_err.c ---
 *
 *	Error logging facility.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "send_msg.h"
#include "iosp.h"
#include "util.h"
#include "trans_log_name.h"
#include "gtm_syslog.h"
#include "gtm_time.h"

#include "gtcm.h"
#include "fao_parm.h"
#include "eintr_wrappers.h"
#include "sgtm_putmsg.h"
#include "gtm_limits.h"
#ifdef __MVS__
#include "gtm_stat.h"
#include "gtm_zos_io.h"
#endif

GBLREF char		*omi_service;

error_def(ERR_TEXT);
void gtcm_rep_err(char *msg, int errcode)
{
	FILE	*fp;
	char	outbuf[OUT_BUFF_SIZE];
	time_t	now;
	int	status, retval, gtm_dist_len;
	char 	*filebuf, *tag_emsg, *tmp_time;
	mstr	tn;

	if ('\0' == msg[0])
		sgtm_putmsg(outbuf, VARLSTCNT(2) errcode, 0);
	else
		sgtm_putmsg(outbuf, VARLSTCNT(6) errcode, 0, ERR_TEXT, 2, LEN_AND_STR(msg));
	util_out_print(outbuf, OPER);	/* Same message goes out to operator log */
	return;
}
