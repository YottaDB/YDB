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

/*
 *  gtcm_cn_disc.c ---
 *
 *	Close out a connection.
 *
 */

#ifndef lint
static char rcsid[] = "$Header: /cvsroot/sanchez-gtm/gtm/sr_unix_cm/gtcm_cn_disc.c,v 1.1.1.1 2001/05/16 14:01:54 marcinim Exp $";
#endif

#ifdef DEBUG
#include "gtm_stdio.h"
#endif /* defined(DEBUG) */

#include "mdef.h"
#include "gtcm.h"

#ifdef GTCM_RC
#include "rc.h"
#endif /* defined(GTCM_RC) */

void gtcm_cn_disc(omi_conn *cptr, omi_conn_ll *cll)
{
	time_t	end;
	int	i, nxact, nerrs;
	char	*gtcm_hname();

	/*  Cumulative statistics */
	end = time((time_t *)0);
	cll->st_cn.start      += end - cptr->stats.start;
	cll->st_cn.bytes_recv += cptr->stats.bytes_recv;
	cll->st_cn.bytes_send += cptr->stats.bytes_send;
	for (i = 0, nxact = 0; i < OMI_OP_MAX; i++)
	{
		cll->st_cn.xact[i] += cptr->stats.xact[i];
		nxact              += cptr->stats.xact[i];
	}
	for (i = 0, nerrs = 0; i < OMI_ER_MAX; i++)
	{
		cll->st_cn.errs[i] += cptr->stats.errs[i];
		nerrs              += cptr->stats.errs[i];
	}
	cll->stats.disc++;
	if (cptr->state != OMI_ST_DISC)
		cll->stats.clos++;
	OMI_DBG_STMP;
	OMI_DBG((omi_debug, "%s: connection %d to %s closed\n",
	SRVR_NAME, cptr->stats.id, gtcm_hname(&cptr->stats.sin)));
	OMI_DBG((omi_debug, "%s:\t%d seconds connect time\n", SRVR_NAME, end - cptr->stats.start));
	OMI_DBG((omi_debug, "%s:\t%d transactions\n", SRVR_NAME, nxact));
	OMI_DBG((omi_debug, "%s:\t%d errors\n", SRVR_NAME, nerrs));
	OMI_DBG((omi_debug, "%s:\t%d bytes recv'd\n", SRVR_NAME, cptr->stats.bytes_recv));
	OMI_DBG((omi_debug, "%s:\t%d bytes sent\n", SRVR_NAME, cptr->stats.bytes_send));
#ifdef BSD_TCP
	/*  Close out the network connection */
	(void) close(cptr->fd);
#endif /* defined(BSD_TCP) */
	if (cptr->pklog)
		close(cptr->pklog);
#ifdef GTCM_RC
	if (cptr->of)
		rc_oflow_fin(cptr->of);
#endif /* defined(GTCM_RC) */
	/*  Release locks held on this connection */
	omi_prc_unla(cptr, cptr->buff, cptr->buff, cptr->buff);
	/*  Release the buffer and connection structure */
	free(cptr->buff);
	free(cptr);
	return;
}
