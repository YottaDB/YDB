/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_socket.h"
#include "gtm_netdb.h"
#include "gtm_inet.h"
#include "gtm_string.h"
#include "gtm_select.h"

#include "cmidef.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "deferred_events_queue.h"
#include "relqop.h"
#include "gtm_ipv6.h"

GBLREF struct NTD *ntd_root;

GBLREF	volatile int4	outofband;

error_def(ERR_FDSIZELMT);

cmi_status_t cmi_open(struct CLB *lnk)
{
	cmi_status_t		status;
	int			rval;
	unsigned char		*cp;
	char			hn[MAX_HOST_NAME_LEN];
	struct addrinfo		*ai_ptr, *ai_head;
	sigset_t		oset;
	int			new_fd, rc, save_errno;
	int			sockerror;
	GTM_SOCKLEN_TYPE	sockerrorlen;
	fd_set			writefds;
	int			save_error;

	ASSERT_IS_LIBCMISOCKETTCP;
	if (!ntd_root)
	{
		status = cmj_netinit();
		if (CMI_ERROR(status))
			return status;
	}
	lnk->ntd = ntd_root;
	status = cmj_getsockaddr(&lnk->nod, &lnk->tnd, &ai_head);
	if (CMI_ERROR(status))
		return status;
	lnk->mun = -1;
	memset((void *)&lnk->cqe, 0, SIZEOF(lnk->cqe));
	memset((void *)&lnk->ios, 0, SIZEOF(lnk->ios));
	memset((void *)&lnk->stt, 0, SIZEOF(lnk->stt));
	lnk->cbl = 0;
	lnk->urgdata = 0;
	lnk->deferred_event = lnk->deferred_reason = lnk->deferred_status = 0;
	lnk->sta = CM_CLB_DISCONNECT;
	lnk->err = NULL;
	lnk->ast = NULL;
	save_errno = 0;		/* 4SCA: Garbage return value */
	for(ai_ptr = ai_head; NULL != ai_ptr; ai_ptr = ai_ptr->ai_next)
	{
		if (FD_INVALID != (new_fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol)))
			break;
		save_errno = errno;
	}
	if (NULL == ai_ptr)
	{
		FREEADDRINFO(ai_head);
		return save_errno;
	}
	rval = connect(new_fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);		/* BYPASSOK(connect) */
	if (FD_SETSIZE <= new_fd)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_FDSIZELMT, 1, new_fd);
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
	if ((-1 == rval) && ((EINTR == errno) || (EINPROGRESS == errno)
#	if (defined(__osf__) && defined(__alpha)) || defined(__sun) || defined(__vms)
			     || (EWOULDBLOCK == errno)
#	endif
			     ))
	{	/* connection attempt will continue so wait for completion */
		do
		{
			if (EINTR == errno)
				eintr_handling_check();
			if ((EINTR == errno) && outofband && (jobinterrupt != outofband))
				break;		/* abort unless job interrupt */
			FD_ZERO(&writefds);
			FD_SET(new_fd, &writefds);
			rval = select(new_fd + 1, NULL, &writefds, NULL, NULL);
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			if (-1 == rval && EINTR == errno)
			{
				eintr_handling_check();
				continue;
			}
			if (0 < rval)
			{	/* check for socket error */
				sockerrorlen = SIZEOF(sockerror);
				rval = getsockopt(new_fd, SOL_SOCKET, SO_ERROR, &sockerror, &sockerrorlen);
				if (0 == rval && 0 != sockerror)
				{	/* return socket error */
					rval = -1;
					errno = sockerror;
				}
			}
			break;
		} while (TRUE);
	}
	if (-1 == rval && EISCONN != errno)
	{
		save_errno = errno;
		CLOSEFILE_RESET(new_fd, rc);	/* resets "new_fd" to FD_INVALID */
		FREEADDRINFO(ai_head);
		return save_errno;
	}
	SIGPROCMASK(SIG_BLOCK, &ntd_root->mutex_set, &oset, rc);
	status = cmj_setupfd(new_fd);
	if (CMI_ERROR(status))
		CLOSEFILE_RESET(new_fd, rc);	/* resets "new_fd" to FD_INVALID */
	else
	{
		status = cmj_set_async(new_fd);
		if (!CMI_ERROR(status))
		{
			insqh(&lnk->cqe, &ntd_root->cqh);
			if (new_fd > ntd_root->max_fd)
				ntd_root->max_fd = new_fd;
			lnk->mun = new_fd;
			memcpy((struct sockaddr *)(&lnk->peer_sas), ai_ptr->ai_addr, ai_ptr->ai_addrlen);
			memcpy(&lnk->peer_ai, ai_ptr, SIZEOF(struct addrinfo));
			lnk->peer_ai.ai_addr = (struct sockaddr *)(&lnk->peer_sas);
			FD_SET(new_fd, &ntd_root->es);
			lnk->sta = CM_CLB_IDLE;
		} else
			CLOSEFILE_RESET(new_fd, rc);	/* resets "new_fd" to FD_INVALID */
	}
	SIGPROCMASK(SIG_SETMASK, &oset, NULL, rc);
	FREEADDRINFO(ai_head);			/* prevent mem-leak */
	return status;
}
