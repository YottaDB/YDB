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
 *  gtcm_end_net.c ---
 *
 *	Close out the network layer.
 *
 */

#ifndef lint
static char rcsid[] = "$Header: /cvsroot/sanchez-gtm/gtm/sr_unix_cm/gtcm_end_net.c,v 1.1.1.1 2001/05/16 14:01:54 marcinim Exp $";
#endif

#ifdef DEBUG
#include "gtm_stdio.h"
#endif /* defined(DEBUG) */

#include "mdef.h"
#include "gtcm.h"

GBLREF char	*omi_oprlist[];
GBLREF char	*omi_errlist[];


void
gtcm_end_net(cll)
    omi_conn_ll	*cll;
{
    extern int4	 omi_nxact, omi_nerrs, omi_brecv, omi_bsent;

    omi_conn	*cptr, *tptr;
    int		 i, nxact, nerrs;

/*  Close all existing connections */
    cptr = cll->head;
    while (cptr) {
	cptr = (tptr=cptr)->next;
/*	gtcm_cn_disc() releases the omi_conn structure, hence the back ptr */
	gtcm_cn_disc(tptr, cll);
    }

#ifdef BSD_TCP
    if (!INV_FD_P(cll->nve))
	(void) close(cll->nve);
#endif /* defined(BSD_TCP) */

    OMI_DBG_STMP;
    OMI_DBG((omi_debug, "gtcm_server: shutdown completed\n"));
    OMI_DBG((omi_debug, "gtcm_server:\tGood connections: %d\n",
	     cll->stats.conn));
    OMI_DBG((omi_debug, "gtcm_server:\tBad connections: 0\n"));
    OMI_DBG((omi_debug, "gtcm_server:\tGood disconnects: %d\n",
	     cll->stats.disc - cll->stats.clos));
    OMI_DBG((omi_debug, "gtcm_server:\tBad disconnects: %d\n",
	     cll->stats.clos));
    OMI_DBG((omi_debug, "gtcm_server:\tNumber of seconds (conn time): %ld\n",
	     cll->st_cn.start));
    OMI_DBG((omi_debug, "gtcm_server:\tNumber of transactions: %ld\n",
	     omi_nxact));
    for (i = 0, nxact = 0; i < OMI_OP_MAX; i++) {
	nxact += cll->st_cn.xact[i];
	if (cll->st_cn.xact[i])
	    OMI_DBG((omi_debug, "gtcm_server: %8d %s\n", cll->st_cn.xact[i],
		     ((omi_oprlist[i]) ? omi_oprlist[i] : "unknown")));
    }
    if (nxact != omi_nxact)
	OMI_DBG((omi_debug,"gtcm_server:\tNumber of transactions (sum): %ld\n",
		 nxact));
    OMI_DBG((omi_debug, "gtcm_server:\tNumber of errors: %ld\n",
	     omi_nerrs));
    for (i = 0, nerrs = 0; i < OMI_ER_MAX; i++) {
	nerrs += cll->st_cn.errs[i];
	if (cll->st_cn.errs[i])
	    OMI_DBG((omi_debug, "gtcm_server: %8d %s\n", cll->st_cn.errs[i],
		     ((omi_errlist[i]) ? omi_errlist[i] : "unknown")));
    }
    if (nerrs != omi_nerrs)
	OMI_DBG((omi_debug,"gtcm_server:\tNumber of errors (sum): %ld\n",
		 nerrs));
    OMI_DBG((omi_debug, "gtcm_server:\tNumber of bytes received: %ld\n",
	     omi_brecv));
    OMI_DBG((omi_debug, "gtcm_server:\tNumber of bytes sent: %ld\n",
	     omi_bsent));

    return;

}
