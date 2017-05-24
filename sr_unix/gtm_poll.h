/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_POLL_H_INCLUDED
#define GTM_POLL_H_INCLUDED

#include <poll.h>

#define EVENTFD_NOTIFIED(fds, n) (0 != (fds[n].revents & POLLIN))

#define INIT_POLLFD(pollfd, efd)		\
{						\
	pollfd.fd = efd;			\
	pollfd.revents = 0;			\
	pollfd.events = POLLIN;			\
}

#endif
