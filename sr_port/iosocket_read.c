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

/* iosocket_read.c */

#include "mdef.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "io.h"
#include "gt_timer.h"
#include "iosocketdef.h"

int	iosocket_read(mval *v, int4 msec_timeout)
{
	return iosocket_readfl(v, 0, msec_timeout); /* 0 means not fixed length */
}
