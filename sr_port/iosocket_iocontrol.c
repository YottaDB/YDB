/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_iocontrol.c */

#include "mdef.h"
#include "mvalconv.h"

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
LITREF	mval		skiparg;

error_def(ERR_EXPR);
error_def(ERR_INVCTLMNE);

/* for iosocket_dlr_zkey */
#define LISTENING	"LISTENING|"
#define READ		"READ|"
#define MAXEVENTLITLEN	(SIZEOF(LISTENING)-1)
#define MAXZKEYITEMLEN	(MAX_HANDLE_LEN + SA_MAXLITLEN + MAXEVENTLITLEN + 2)	/* 1 pipe and a semicolon */

void	iosocket_iocontrol(mstr *mn, int4 argcnt, va_list args)
{
	char		action[MAX_DEVCTL_LENGTH];
	unsigned short 	depth;
	int		length, n, timeout;
	pid_t		pid;
	mval		*arg, *handlesvar = NULL;
#ifdef	GTM_TLS
	mval		*option, *tlsid, *password, *extraarg;
#endif

	if (0 == mn->len)
		return;
	assert(MAX_DEVCTL_LENGTH > mn->len);
	lower_to_upper((uchar_ptr_t)action, (uchar_ptr_t)mn->addr, mn->len);
	length = mn->len;
	if (0 == memcmp(action, "LISTEN", length))
	{
		if (1 > argcnt)
			depth = DEFAULT_LISTEN_DEPTH;
		else
		{
			arg = va_arg(args, mval *);
			if ((NULL == arg) || M_ARG_SKIPPED(arg) || !MV_DEFINED(arg))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
				return;
			}
			depth = MV_FORCE_INTD(arg);
		}
		iosocket_listen(io_curr_device.in, depth);
	} else if (0 == memcmp(action, "WAIT", length))
	{
		if (1 > argcnt)
			timeout = NO_M_TIMEOUT;
		else
		{
			arg = va_arg(args, mval *);
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				timeout = MV_FORCE_INTD(arg);
			else
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
				return;
			}
		}
		iosocket_wait(io_curr_device.in, timeout);
#	ifndef VMS
	} else if (0 == memcmp(action, "PASS", length))
	{
		n = argcnt;
		if (1 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				pid = MV_FORCE_INTD(arg);
			else
				pid = -1;
		} else
			pid = -1;
		if (2 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				timeout = MV_FORCE_INTD(arg);
			else
				timeout = NO_M_TIMEOUT;
		} else
			timeout = NO_M_TIMEOUT;
		iosocket_pass_local(io_curr_device.out, pid, timeout, n, args);
	} else if (0 == memcmp(action, "ACCEPT", length))
	{
		n = argcnt;
		if (1 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg))
				handlesvar = arg;
		}
		if (2 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				pid = MV_FORCE_INTD(arg);
			else
				pid = -1;
		} else
			pid = -1;
		if (3 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				timeout = MV_FORCE_INTD(arg);
			else
				timeout = NO_M_TIMEOUT;
		} else
			timeout = NO_M_TIMEOUT;
		iosocket_accept_local(io_curr_device.in, handlesvar, pid, timeout, n, args);
#ifdef	GTM_TLS
	} else if (0 == memcmp(action, "TLS", length))
	{	/*	WRITE /TLS(option[,[timeout][,tlsid[,password]]]) */
		if (1 <= argcnt)
		{
			option = va_arg(args, mval *);
			if ((NULL != option) && !M_ARG_SKIPPED(option) && MV_DEFINED(option))
				MV_FORCE_STRD(option);
			else
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
				return;
			}
		} else
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
			return;
		}
		if (2 <= argcnt)
		{
			arg = va_arg(args, mval *);
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				timeout = MV_FORCE_INTD(arg);
			else
				timeout = NO_M_TIMEOUT;
		} else
			timeout = NO_M_TIMEOUT;
		if (3 <= argcnt)
		{
			tlsid = va_arg(args, mval *);
			if ((NULL != tlsid) && !M_ARG_SKIPPED(tlsid) && MV_DEFINED(tlsid))
				MV_FORCE_STRD(tlsid);
			else
				tlsid = NULL;
		} else
			tlsid = NULL;
		if ((4 <= argcnt) && (NULL != tlsid))
		{	/* password only valid if tlsid provided */
			password = va_arg(args, mval *);
			if ((NULL != password) && !M_ARG_SKIPPED(password) && MV_DEFINED(password))
				MV_FORCE_STRD(password);
			else
				password = NULL;
		} else
			password = NULL;
		if (5 <= argcnt)
		{
			extraarg = va_arg(args, mval *);
			if ((NULL != extraarg) && !M_ARG_SKIPPED(extraarg) && MV_DEFINED(extraarg))
				MV_FORCE_STRD(extraarg);
			else
				extraarg = NULL;
		} else
			extraarg = NULL;
		iosocket_tls(option, timeout, tlsid, password, extraarg);
#	endif
#	endif
	} else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVCTLMNE);

	return;
}

void	iosocket_dlr_device(mstr *d)
{
	io_desc		*iod;
	int		len;

	iod = io_curr_device.in;
 	len = STRLEN(iod->dollar.device);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	memcpy(d->addr, iod->dollar.device, len);
	d->len = len;
	return;
}

void	iosocket_dlr_key(mstr *d)
{
	io_desc		*iod;
	int		len;

	iod = io_curr_device.in;
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
