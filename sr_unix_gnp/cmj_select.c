/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "cmidef.h"
#include "gtm_time.h"
#include <sys/time.h>
#include <errno.h>

GBLREF struct NTD *ntd_root;

void cmj_select(int signo)
{
	int count, local_errno;
	int rsfd;
	int wsfd;
	int esfd;
	struct timeval t;
	int n = ntd_root->max_fd + 1;
	struct CLB *lnk;

	fd_set myrs = ntd_root->rs;
	fd_set myws = ntd_root->ws;
	fd_set myes = ntd_root->es;

	t.tv_usec = 0;
	t.tv_sec = 0;
	do
	{
		count = select(n, &myrs, &myws, &myes, &t);
	} while (0 > count && (EINTR == errno || EAGAIN == errno));
	if (0 > count)
	{
		local_errno = errno;
	}

	while (0 < count) {
		/* decode */
		esfd = cmj_firstone(&myes, n);
		while (0 < esfd)
		{
			lnk = cmj_unit2clb(ntd_root, esfd);
			if (lnk)
				cmj_exception_interrupt(lnk, signo);
			esfd = cmj_firstone(&myes, n);
		}

		rsfd = cmj_firstone(&myrs, n);
		while (0 < rsfd)
		{
			if (rsfd == ntd_root->listen_fd)
				cmj_incoming_call(ntd_root);
			else
			{
				lnk = cmj_unit2clb(ntd_root, rsfd);
				if (lnk)
					cmj_read_interrupt(lnk, signo);
			}
			rsfd = cmj_firstone(&myrs, n);
		}

		wsfd = cmj_firstone(&myws, n);
		while (0 < wsfd)
		{
			lnk = cmj_unit2clb(ntd_root, wsfd);
			if (lnk)
				cmj_write_interrupt(lnk, signo);
			wsfd = cmj_firstone(&myws, n);
		}
		myrs = ntd_root->rs;
		myws = ntd_root->ws;
		t.tv_usec = 0;
		t.tv_sec = 0;
		/*
		 * don't look at exceptions again since urgent exception
		 * is not cleared until the first byte of non-urgent data
		 */
		do
		{
			count = select(n, &myrs, &myws, NULL, &t);
		} while (0 > count && (EINTR == errno || EAGAIN == errno));
		if (0 > count)
		{
			local_errno = errno;
		}
	}
}
