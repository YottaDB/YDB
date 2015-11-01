/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

#include <unistd.h>
#include "gtm_socket.h"
#include "gtm_inet.h"

#include "io.h"
#include "io_params.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "op.h"

GBLREF  d_socket_struct         *socket_pool;

static char socketpoolv[] = "socketpool";
static char socketpoolp = '\0';
static char socketpoolm[] = "socket";

void iosocket_poolinit(void)
{
	mval 		sockv, sockp, sockm;
	int   		t;
	io_log_name	*nl;

	memset(&sockv, 0, sizeof(mval));
	memset(&sockp, 0, sizeof(mval));
	memset(&sockm, 0, sizeof(mval));

	sockv.mvtype = MV_STR;
	sockv.str.len = sizeof(socketpoolv) - 1;
	sockv.str.addr = &socketpoolv[0];

	sockp.mvtype = MV_STR;
	sockp.str.len = sizeof(socketpoolp);
	sockp.str.addr = &socketpoolp;

	sockm.mvtype = MV_STR;
	sockm.str.len = sizeof(socketpoolm) - 1;
	sockm.str.addr = &socketpoolm[0];

	t = NO_M_TIMEOUT;

	op_open(&sockv, &sockp, t, &sockm);

	nl = get_log_name(&sockv.str, NO_INSERT);
	assert(NULL != nl);
	socket_pool =  (d_socket_struct *)(nl->iod->dev_sp);
}
