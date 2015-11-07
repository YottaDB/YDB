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

/* iosocket_iocontrol.c */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "gtm_caseconv.h"
#include "stringpool.h"
#include "min_max.h"

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

error_def(ERR_INVCTLMNE);

/* for iosocket_dlr_zkey */
#define LISTENING	"LISTENING|"
#define READ		"READ|"
#define MAXEVENTLITLEN	(SIZEOF(LISTENING)-1)
#define MAXZKEYITEMLEN	(MAX_HANDLE_LEN + SA_MAXLITLEN + MAXEVENTLITLEN + 2)	/* 1 pipe and a semicolon */

void	iosocket_iocontrol(mstr *d)
{
	char 		action[MAX_DEVCTL_LENGTH];
	unsigned short 	depth; /* serve as depth for LISTEN and timeout for WAIT */
	int		length, n, timeout;

	if (0 == d->len)
		return;
	/* The contents of d->addr are passed to us from op_iocontrol. That routine sets up these parms
	 * but does not zero terminate the string we are passing to SSCANF. If this is not a complex parm
	 * with parens (as is the case with WRITE /WAIT type statements), SSCANF() will get into trouble
	 * if the stringpool contains extra junk. For that reason, we now add a null terminator and make
	 * sure the argument is not too big to parse into our buffer above.
	 */
	assert(d->addr == (char *)stringpool.free); 	/* Verify string is where we think it is so we don't corrupt something */
	assert(MAX_DEVCTL_LENGTH > d->len);
	assert(IS_IN_STRINGPOOL(d->addr, d->len));
	*(d->addr + d->len) = '\0';
	if (0 == (n = SSCANF(d->addr, "%[^(](%hu)", &action[0], &depth)))
		memcpy(&action[0], d->addr, d->len);
	if (0 == (length = STRLEN(&action[0])))
		return;
	lower_to_upper((uchar_ptr_t)&action[0], (uchar_ptr_t)&action[0], length);
	if (0 == memcmp(&action[0], "LISTEN", length))
	{
		if (2 > n)
			depth = DEFAULT_LISTEN_DEPTH;
		iosocket_listen(io_curr_device.out, depth);
	} else if (0 == memcmp(&action[0], "WAIT", length))
	{
		timeout = depth;
		if (2 > n)
			timeout = NO_M_TIMEOUT;
		iosocket_wait(io_curr_device.out, timeout); /* depth really means timeout here. */
	} else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVCTLMNE);

	return;
}

void	iosocket_dlr_device(mstr *d)
{
	io_desc		*iod;
	int		len;

	iod = io_curr_device.out;
 	len = STRLEN(iod->dollar.device);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	memcpy(d->addr, iod->dollar.device, len);
	d->len = len;
	return;
}

void	iosocket_dlr_key(mstr *d)
{
	io_desc         *iod;
        int             len;

        iod = io_curr_device.out;
        len = STRLEN(iod->dollar.key);
        /* verify internal buffer has enough space for $DEVICE string value */
        assert((int)d->len > len);
	if (len > 0)
	        memcpy(d->addr, iod->dollar.key, len);
        d->len = len;
        return;
}

void iosocket_dlr_zkey(mstr *d)
{
	int4		ii;
	int		len, thislen, totlen;
	char		*zkeyptr, *charptr;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;

	iod = io_curr_device.out;
	if (gtmsocket != iod->type)
		iod = io_curr_device.in;
	assertpro(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	zkeyptr = (char *)stringpool.free;
	totlen = thislen = len = 0;
	for (ii = 0; ii < dsocketptr->n_socket; ii++)
	{
		socketptr = dsocketptr->socket[ii];
		if ((socket_listening != socketptr->state) && (socket_connected != socketptr->state))
			continue;
		if ((socket_connected == socketptr->state) && (0 < socketptr->buffered_length))
		{	/* data to be read in buffer */
			if (!socketptr->pendingevent)
			{	/* may have been cleared by partial READ */
				socketptr->pendingevent = TRUE;
				socketptr->readycycle = dsocketptr->waitcycle;
			}
		}
		if (socketptr->pendingevent)
		{
			thislen = len = 0;
			if (!IS_STP_SPACE_AVAILABLE(totlen + MAXZKEYITEMLEN))
			{	/* d must be mstr part of mval known to stp_gcol */
				if (totlen)
				{
					d->len = totlen;
					d->addr = (char *)stringpool.free;
					stringpool.free += totlen;
				}
				INVOKE_STP_GCOL(totlen + MAXZKEYITEMLEN);
				if (totlen)
				{
					if (!IS_AT_END_OF_STRINGPOOL(d->addr, totlen))
					{	/* need to move to top */
						memcpy(stringpool.free, d->addr, totlen);
					} else
						stringpool.free -= totlen;	/* backup over prior items */
					d->len = 0;	/* so ignored by stp_gcol */
				}
				zkeyptr = (char *)stringpool.free + totlen;
			}
			if (totlen)
			{	/* at least one item already */
				*zkeyptr++ = ';';
				totlen++;
			}
			/* add READ/LISTENING|handle|remoteinfo;... */
			if (socket_listening == socketptr->state)
			{
				thislen = len = SIZEOF(LISTENING) - 1;
				memcpy(zkeyptr, LISTENING, len);
			} else
			{
				thislen = len = SIZEOF(READ) - 1;
				memcpy(zkeyptr, READ, len);
			}
			zkeyptr += len;
			thislen += len;
			totlen += len;
			memcpy(zkeyptr, socketptr->handle, socketptr->handle_len);
			zkeyptr += socketptr->handle_len;
			*zkeyptr++ = '|';
			thislen += (socketptr->handle_len + 1);
			totlen += (socketptr->handle_len + 1);
			if (socket_local != socketptr->protocol)
			{
				if (socket_listening == socketptr->state)
					len = SPRINTF(zkeyptr, "%d", socketptr->local.port);
				else
				{
					if (NULL != socketptr->remote.saddr_ip)
					{
						len = STRLEN(socketptr->remote.saddr_ip);
						memcpy(zkeyptr, socketptr->remote.saddr_ip, len);
					} else
						len = 0;
				}
#			ifndef VMS
			} else
			{
				if (NULL != socketptr->local.sa)
					charptr = ((struct sockaddr_un *)(socketptr->local.sa))->sun_path;
				else if (NULL != socketptr->remote.sa)
					charptr = ((struct sockaddr_un *)(socketptr->remote.sa))->sun_path;
				else
					charptr = "";
				len = STRLEN(charptr);
				len = MIN(len, (MAXZKEYITEMLEN - thislen));
				memcpy(zkeyptr, charptr, len);
#			endif
			}
			zkeyptr += len;
			totlen += len;
		}
	}
	d->addr = (char *)stringpool.free;
	d->len = totlen;
	stringpool.free += totlen;
	return;
}
