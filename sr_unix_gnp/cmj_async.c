/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
||||||| parent of 07a11a26bb (GT.M V7.1-004)
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
=======
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 07a11a26bb (GT.M V7.1-004)
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
<<<<<<< HEAD

	ASSERT_IS_LIBCMISOCKETTCP;
	/* I/O readiness is delivered via epoll, not SIGIO; only O_NONBLOCK is required here. */
	rval = fcntl(fd, F_SETFL, O_NONBLOCK);
||||||| parent of 07a11a26bb (GT.M V7.1-004)
#if defined(FIOASYNC)
	int val = 1;
	rval = ioctl(fd, FIOASYNC, &val);
#elif defined(O_ASYNC)
	rval = fcntl(fd, F_SETFL, O_ASYNC|O_NONBLOCK);
#elif defined(FASYNC)
	rval = fcntl(fd, F_SETFL, FASYNC|O_NONBLOCK);
#else
#error Can not set async state on platform
#endif
=======
#if defined(O_ASYNC)
	rval = fcntl(fd, F_SETFL, O_ASYNC|O_NONBLOCK);
#elif defined(FASYNC)
	rval = fcntl(fd, F_SETFL, FASYNC|O_NONBLOCK);
#else
#error Can not set async state on platform
#endif
>>>>>>> 07a11a26bb (GT.M V7.1-004)
	if (rval < 0)
		status = errno;
	return status;
}

int cmj_reset_async(int fd)
{
	cmi_status_t rval;
<<<<<<< HEAD
#if defined(FIOASYNC)
	int val = 0;
	ASSERT_IS_LIBCMISOCKETTCP;
	rval = (cmi_status_t)ioctl(fd, FIOASYNC, &val);
#elif defined(O_ASYNC) || defined(FASYNC)
	ASSERT_IS_LIBCMISOCKETTCP;
||||||| parent of 07a11a26bb (GT.M V7.1-004)
#if defined(FIOASYNC)
	int val = 0;
	rval = (cmi_status_t)ioctl(fd, FIOASYNC, &val);
#elif defined(O_ASYNC) || defined(FASYNC)
=======
#if defined(O_ASYNC) || defined(FASYNC)
>>>>>>> 07a11a26bb (GT.M V7.1-004)
	rval = (cmi_status_t)fcntl(fd, F_SETFL, O_NONBLOCK);
#else
#error Can not set async state on platform
#endif
	return rval;
}
