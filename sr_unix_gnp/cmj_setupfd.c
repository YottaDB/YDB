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
#include "mdef.h"
#include "cmidef.h"
#include "gtm_socket.h"
#include "gtm_fcntl.h"
#ifdef __sparc
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include <netinet/in.h>
#ifndef __MVS__
#include <netinet/tcp.h>
#endif
#include <errno.h>
#include "eintr_wrappers.h"

GBLREF	uint4	process_id;

cmi_status_t cmj_setupfd(int fd)
{
	int rval;
	long lpid = (long)process_id;
	int on = 1;

	FCNTL3(fd, F_SETFL, O_NONBLOCK, rval);
	if (-1 == rval)
		return (cmi_status_t)errno;
	FCNTL3(fd, F_SETOWN, lpid, rval);
	if (-1 == rval)
		return (cmi_status_t)errno;
#ifdef F_SETSIG
	FCNTL3(fd, F_SETSIG, (long)SIGIO, rval);
	if (-1 == rval)
		return (cmi_status_t)errno;
#endif
	rval = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
			(const void *)&on, sizeof(on));
	if (rval < 0)
		return (cmi_status_t)errno;
#ifdef TCP_NODELAY
	/* z/OS only does setsockop on SOL and IP */
	rval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			(const void *)&on, sizeof(on));
	if (rval < 0)
		return (cmi_status_t)errno;
#endif
	return SS_NORMAL;
}
