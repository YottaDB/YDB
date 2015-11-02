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

#include "mdef.h"

#include <sys/types.h>
#include <errno.h>
#include <netinet/tcp.h>

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_inet.h"

#include "io.h"
#include "iotcpdef.h"
#include "iotcpdefsp.h"
#include "iotcproutine.h"

void	iotcp_wteol(int4 x, io_desc *iod)
{
	/* pending a change request C9A06-001531 */
	return;
}
