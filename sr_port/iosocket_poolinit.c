/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_poolinit.c */

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_unistd.h"
#include "gtm_socket.h"
#include "gtm_inet.h"

#include "io.h"
#include "io_params.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "op.h"

GBLREF  d_socket_struct         *socket_pool;

LITREF	mval		literal_notimeout;

static char socketpoolv[] = "socketpool";
static char socketpoolp = '\0';
static char socketpoolm[] = "socket";

void iosocket_poolinit(void)
{
	mval 		sockv, sockp, sockm;
	io_log_name	*nl;

	memset(&sockv, 0, SIZEOF(mval));
	memset(&sockp, 0, SIZEOF(mval));
	memset(&sockm, 0, SIZEOF(mval));
	sockv.mvtype = MV_STR;
	sockv.str.len = SIZEOF(socketpoolv) - 1;
	sockv.str.addr = &socketpoolv[0];
	sockp.mvtype = MV_STR;
	sockp.str.len = SIZEOF(socketpoolp);
	sockp.str.addr = &socketpoolp;
	sockm.mvtype = MV_STR;
	sockm.str.len = SIZEOF(socketpoolm) - 1;
	sockm.str.addr = &socketpoolm[0];
	op_open(&sockv, &sockp, (mval *)&literal_notimeout, &sockm);
	nl = get_log_name(&sockv.str, NO_INSERT);
	assert(NULL != nl);
	socket_pool =  (d_socket_struct *)(nl->iod->dev_sp);
}
