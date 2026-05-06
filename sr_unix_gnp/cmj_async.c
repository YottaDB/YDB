/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include <errno.h>

cmi_status_t cmj_set_async(int fd)
{
	cmi_status_t status = SS_NORMAL;
	int rval;

	ASSERT_IS_LIBCMISOCKETTCP;
	/* I/O readiness is delivered via epoll, not SIGIO; only O_NONBLOCK is required here. */
	rval = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (rval < 0)
		status = errno;
	return status;
}

int cmj_reset_async(int fd)
{
	cmi_status_t rval;
#if defined(FIOASYNC)
	int val = 0;
	ASSERT_IS_LIBCMISOCKETTCP;
	rval = (cmi_status_t)ioctl(fd, FIOASYNC, &val);
#elif defined(O_ASYNC) || defined(FASYNC)
	ASSERT_IS_LIBCMISOCKETTCP;
	rval = (cmi_status_t)fcntl(fd, F_SETFL, O_NONBLOCK);
#else
#error Can not set async state on platform
#endif
	return rval;
}
