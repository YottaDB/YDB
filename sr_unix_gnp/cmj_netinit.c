/****************************************************************
 *								*
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

#include "gtm_string.h"
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "cmidef.h"
#include "generic_signal_handler.h"
#include "sig_init.h"

GBLDEF struct NTD *ntd_root;

/* Mirror the desired event mask for "fd" (derived from rs/ws/es) into the NTD's epoll set.
 * Uses EPOLL_CTL_ADD on first registration and EPOLL_CTL_MOD thereafter; if no events are
 * desired the fd is removed via EPOLL_CTL_DEL.
 */
void cmj_epoll_update(struct NTD *tsk, int fd)
{
	struct epoll_event	ev;
	int			rc;

	ASSERT_IS_LIBCMISOCKETTCP;
	if (NULL == tsk || 0 > tsk->epoll_fd || 0 > fd)
		return;
	ev.events = 0;
	if (FD_ISSET(fd, &tsk->rs))
		ev.events |= EPOLLIN;
	if (FD_ISSET(fd, &tsk->ws))
		ev.events |= EPOLLOUT;
	if (FD_ISSET(fd, &tsk->es))
		ev.events |= EPOLLPRI;
	ev.data.fd = fd;
	if (0 == ev.events)
	{
		rc = epoll_ctl(tsk->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
		if ((-1 == rc) && (ENOENT != errno))
		{
			CMI_DPRINT(("cmj_epoll_update: EPOLL_CTL_DEL fd=%d errno=%d\n", fd, errno));
		}
		return;
	}
	rc = epoll_ctl(tsk->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
	if (-1 == rc && ENOENT == errno)
	{
		rc = epoll_ctl(tsk->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
		if (-1 == rc)
		{
			CMI_DPRINT(("cmj_epoll_update: EPOLL_CTL_ADD fd=%d errno=%d\n", fd, errno));
		}
	} else if (-1 == rc)
	{
		CMI_DPRINT(("cmj_epoll_update: EPOLL_CTL_MOD fd=%d errno=%d\n", fd, errno));
	}
}

/* Remove "fd" from the NTD's epoll set unconditionally (used on close/error). */
void cmj_epoll_remove(struct NTD *tsk, int fd)
{
	int rc;

	ASSERT_IS_LIBCMISOCKETTCP;
	if (NULL == tsk || 0 > tsk->epoll_fd || 0 > fd)
		return;
	rc = epoll_ctl(tsk->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	if ((-1 == rc) && (ENOENT != errno) && (EBADF != errno))
	{
		CMI_DPRINT(("cmj_epoll_remove: EPOLL_CTL_DEL fd=%d errno=%d\n", fd, errno));
	}
}

/* CLEAN-UP: 1. replace CMI_CMICHECK with new message
	2. find correct size and location of MBX_SIZE
*/

error_def(CMI_CMICHECK);

cmi_status_t cmj_netinit(void)
{
	struct NTD *tsk;

	ASSERT_IS_LIBCMISOCKETTCP;
	if (ntd_root)
		return CMI_CMICHECK;
	ntd_root = (struct NTD *)malloc(SIZEOF(*ntd_root));
	tsk = ntd_root;
	memset(tsk, 0, SIZEOF(*tsk));
	tsk->listen_fd = FD_INVALID;
	FD_ZERO(&tsk->rs);
	FD_ZERO(&tsk->ws);
	FD_ZERO(&tsk->es);
	/* I/O readiness is now driven by epoll instead of SIGIO/SIGURG. mutex_set is left empty
	 * so the existing SIGPROCMASK brackets become cheap (block-empty-set) and other signals
	 * (SIGTERM/HUP/USR/ALRM) keep their normal delivery. cmj_handler is no longer registered.
	 */
	sigemptyset(&tsk->mutex_set);
	tsk->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (-1 == tsk->epoll_fd)
		return errno;
	return SS_NORMAL;
}
