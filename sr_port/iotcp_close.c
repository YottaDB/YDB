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

/* iotcp_close.c - close a TCP/IP connection
 *  Parameters-
 *  iod - I/O descriptor for the currently open TCP/IP connection.
 *
 *  pp->str.addr   - device parameters
 *
 */
#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_inet.h"

#include "copy.h"
#include "io_params.h"
#include "io.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "stringpool.h"

GBLREF tcp_library_struct	tcp_routines;
LITREF unsigned char		io_params_size[];

void iotcp_close(io_desc *iod, mval *pp)
{
	bool		close_listen_socket = FALSE;
	unsigned char	c;
	d_tcp_struct	*tcpptr;
	int		p_offset;

#ifdef DEBUG_TCP
	PRINTF("%s >>>\n", __FILE__);
#endif
	assert(iod->type == tcp);
	tcpptr = (d_tcp_struct *)iod->dev_sp;
	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		switch (c = *(pp->str.addr + p_offset++))
		{
		case iop_listen:
			/* close the listening socket associated with this connection,
			 * rather than the actual connection socket.
			 */
			close_listen_socket = TRUE;
			break;
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[c]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[c]);
	}

	if (close_listen_socket)
	{
		iotcp_rmlsock(iod);
#ifdef DEBUG_TCP
		PRINTF("%s (listening socket for %d) <<<\n", __FILE__, tcpptr->socket);
#endif
	}
	else
	{
	        if (iod->state != dev_open)
		        return;
		tcp_routines.aa_close(tcpptr->socket);
		iod->state = dev_closed;
#ifdef DEBUG_TCP
		PRINTF("%s (%d) <<<\n", __FILE__, tcpptr->socket);
#endif
	}
}
