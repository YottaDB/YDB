/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*  gtcm_cn_acpt.c ---
 *	Accept a client connection.
 */

#include "mdef.h"

#include <errno.h>

#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"		/* for close() used by CLOSEFILE_RESET */
#include "gtm_time.h"		/* for ctime and time */

#include "gtcm.h"
#include "rc_oflow.h"
#include "eintr_wrappers.h"
#include "gtm_socket.h"
#include "gtmio.h"
#include "have_crit.h"

#ifdef BSD_TCP
#include "gtm_inet.h"
#endif /* defined(BSD_TCP) */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF	char	*omi_pklog;
GBLREF	int	one_conn_per_inaddr;
GBLREF	int	conn_timeout;

int gtcm_cn_acpt(omi_conn_ll *cll, int now)		/* now --> current time in seconds */
{
	int		i;
	omi_conn	*cptr;
	omi_fd		fd;
	int		rc;
	char 		*tmp_time;

#ifdef BSD_TCP
	GTM_SOCKLEN_TYPE	sln;
	struct sockaddr_storage	sas;
	int			optsize;
	const boolean_t		keepalive = TRUE;

	/*  Accept the connection from the network layer */
	sln = SIZEOF(sas);
	if ((fd = accept(cll->nve, (struct sockaddr *)&sas, (GTM_SOCKLEN_TYPE *)&sln)) < 0)
		return -1;
#endif				/* defined(BSD_TCP) */

	/*  Build the client data structure */
	if (!(cptr = (omi_conn *)malloc(SIZEOF(omi_conn))) || !(cptr->buff = (char *)malloc(OMI_BUFSIZ)))
	{
		if (cptr)
			free(cptr);
		CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
		return -1;
	}
	/*  Initialize the connection structure */
	cptr->next  = (omi_conn *)0;
	cptr->fd    = fd;
	cptr->ping_cnt = 0;
	cptr->timeout = now + conn_timeout;
	cptr->bsiz  = OMI_BUFSIZ;
	cptr->bptr  = cptr->buff;
	cptr->xptr  = (char *)0;
	cptr->blen  = 0;
	cptr->exts  = 0;
	cptr->state = OMI_ST_DISC;
	cptr->ga    = (ga_struct *)0; /* struct gd_addr_struct */
	cptr->of = (oof_struct *) malloc(SIZEOF(struct rc_oflow));
	memset(cptr->of, 0, SIZEOF(struct rc_oflow));
	cptr->pklog = FD_INVALID;
	/*  Initialize the statistics */
	memcpy(&cptr->stats.sas, &sas, sln);
	cptr->stats.ai.ai_addr = (struct sockaddr *)&cptr->stats.sas;
	cptr->stats.ai.ai_addrlen = sln;
	cptr->stats.bytes_recv = 0;
	cptr->stats.bytes_send = 0;
	cptr->stats.start      = time((time_t *)0);
	for (i = 0; i < OMI_OP_MAX; i++)
		cptr->stats.xact[i] = 0;
	for (i = 0; i < OMI_ER_MAX; i++)
		cptr->stats.errs[i] = 0;

	/* if we only allowing one connection per internet address, close any existing ones with the same addr. */
	if (one_conn_per_inaddr)
	{
		omi_conn	*this, *prev;

		for (prev = NULL, this = cll->head; this; prev = this, this = this->next)
		{
			if (0 == memcmp((sockaddr_ptr)(&this->stats.sas), (sockaddr_ptr)&sas, sln))
			{
				if (cll->tail == this)
				    cll->tail = cptr;
				if (prev)
				    prev->next = cptr;
				else
				    cll->head = cptr;
				cptr->next = this->next;
				OMI_DBG_STMP;
				OMI_DBG((omi_debug, "%s: dropping old connection to %s\n",
					SRVR_NAME, gtcm_hname(&cptr->stats.ai)));
				gtcm_cn_disc(this, cll);
				break;
			}
		}
		/* not found - add to the end of the list */
		if (!this)
		{
			if (cll->tail)
			{
				cll->tail->next = cptr;
				cll->tail       = cptr;
			} else
				cll->head = cll->tail = cptr;
		}
	} else
	{
		/*  Insert the client into the list of connections */
		if (cll->tail)
		{
			cll->tail->next = cptr;
			cll->tail       = cptr;
		} else
			cll->head = cll->tail = cptr;
	}
	cptr->stats.id = ++cll->stats.conn;

	DEBUG_ONLY(
		if (omi_pklog)
		{
			int		errno_save;
			char		pklog[1024];

			(void)SPRINTF(pklog, "%s.%04d", omi_pklog, cptr->stats.id);
			if (INV_FD_P((cptr->pklog = OPEN3(pklog, O_WRONLY|O_CREAT|O_APPEND|O_TRUNC, 0644))))
			{
				errno_save = errno;
				OMI_DBG_STMP;
				OMI_DBG((omi_debug, "%s: unable to open packet log \"%s\"\n\t%s\n",
					SRVR_NAME, pklog, STRERROR(errno_save)));
			}
		}
	)
	GTM_CTIME(tmp_time, &cptr->stats.start);
	OMI_DBG((omi_debug, "%s: connection %d from %s by user <%s> at %s", SRVR_NAME,
		cptr->stats.id, gtcm_hname(&cptr->stats.ai), cptr->ag_name, tmp_time));
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, SIZEOF(keepalive)) < 0)
	{
		PERROR("setsockopt:");
		return -1;
	}
	return 0;
}
