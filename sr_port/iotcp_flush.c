/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_inet.h"

#include "io.h"
#include "iotcpdef.h"

void iotcp_flush(io_desc *iod)
{
#ifdef C9A06001531
	/* pending change request C9A06001531 */

	d_tcp_struct	*tcpptr;

#ifdef DEBUG_TCP
	PRINTF("%s >>>\n", __FILE__);
#endif
	tcpptr = (d_tcp_struct *)iod->dev_sp;
	if ((TCP_WRITE == iod->dollar.x && tcpptr->lastop) && !iod->dollar.za)
		iotcp_wteol(1, iod);

#ifdef DEBUG_TCP
	PRINTF("%s <<<\n", __FILE__);
#endif

#endif
	return;
}
