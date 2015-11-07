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

#include "gtm_socket.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_inet.h"

#include <errno.h>

#include "io.h"
#include "iotcpdef.h"
#include "iotcpdefsp.h"
#include "iotcproutine.h"

GBLREF io_pair			io_curr_device;
GBLREF tcp_library_struct	tcp_routines;

error_def(ERR_SOCKWRITE);
error_def(ERR_TEXT);

void	iotcp_write(mstr *v)
{
	io_desc		*iod;
	char		*out;
	int		inlen, outlen, size;
	d_tcp_struct	*tcpptr;
	char		*errptr;
	int4		errlen;

#ifdef DEBUG_TCP
	PRINTF("%s >>>\n", __FILE__);
#endif
	iod = io_curr_device.out;
	tcpptr = (d_tcp_struct *)iod->dev_sp;
	tcpptr->lastop = TCP_WRITE;
	memcpy(iod->dollar.device, LITZERO, SIZEOF(LITZERO));
	inlen = v->len;
	outlen = iod->width - iod->dollar.x;

	if (!iod->wrap && inlen > outlen)
		inlen = outlen;
	if (!inlen)
		return;
	for (out = v->addr;  ;  out += size)
	{
		if (outlen > inlen)
			outlen = inlen;
		if ((size = tcp_routines.aa_send(tcpptr->socket, out, outlen, (tcpptr->urgent ? MSG_OOB : 0))) == -1)
		{
			iod->dollar.za = 9;
			memcpy(iod->dollar.device, LITONE_COMMA, SIZEOF(LITONE_COMMA));
			errptr = (char *)STRERROR(errno);
			errlen = STRLEN(errptr);
			memcpy(&iod->dollar.device[SIZEOF(LITONE_COMMA) - 1], errptr, errlen);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKWRITE, 0, ERR_TEXT, 2, errlen, errptr);
		}
		assert(size == outlen);
		iod->dollar.x += size;
		if ((inlen -= size) <= 0)
			break;

		iod->dollar.x = 0;	/* don't use wteol to terminate wrapped records for fixed. */
		iod->dollar.y++;	/* \n is reserved as an end-of-rec delimiter for variable format */
		if (iod->length)	/* and fixed format requires no padding for wrapped records */
			iod->dollar.y %= iod->length;

		outlen = iod->width;
	}
	iod->dollar.za = 0;
#ifdef DEBUG_TCP
	PRINTF("%s <<<\n", __FILE__);
#endif
	return;
}
