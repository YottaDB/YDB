/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc *
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
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_unistd.h"		/* for close() used by CLOSEFILE_RESET */
#include "gtm_time.h"		/* for ctime() and time() */

#include "gtcm.h"
#include "gtmio.h"

GBLREF char	*omi_oprlist[];
GBLREF char	*omi_errlist[];


void
gtcm_end_net(cll)
    omi_conn_ll	*cll;
{
    extern int4	 omi_nxact, omi_nerrs, omi_brecv, omi_bsent;

    omi_conn	*cptr, *tptr;
    int		i, nxact, nerrs;
    int		rc;

/*  Close all existing connections */
    cptr = cll->head;
    while (cptr) {
	cptr = (tptr=cptr)->next;
/*	gtcm_cn_disc() releases the omi_conn structure, hence the back ptr */
	gtcm_cn_disc(tptr, cll);
    }

#ifdef BSD_TCP
	if (!INV_FD_P(cll->nve))
		CLOSEFILE_RESET(cll->nve, rc);	/* resets "cll->nve" to FD_INVALID */
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
    NON_IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of transactions: %ld\n",
	     omi_nxact)));
    IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of transactions: %d\n",
	     omi_nxact)));
    for (i = 0, nxact = 0; i < OMI_OP_MAX; i++) {
	nxact += cll->st_cn.xact[i];
	if (cll->st_cn.xact[i])
	    OMI_DBG((omi_debug, "gtcm_server: %8d %s\n", cll->st_cn.xact[i],
		     ((omi_oprlist[i]) ? omi_oprlist[i] : "unknown")));
    }
    if (nxact != omi_nxact)
	OMI_DBG((omi_debug,"gtcm_server:\tNumber of transactions (sum): %d\n",
		 nxact));
    NON_IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of errors: %ld\n",
	     omi_nerrs)));
    IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of errors: %d\n",
	     omi_nerrs)));
    for (i = 0, nerrs = 0; i < OMI_ER_MAX; i++) {
	nerrs += cll->st_cn.errs[i];
	if (cll->st_cn.errs[i])
	    OMI_DBG((omi_debug, "gtcm_server: %8d %s\n", cll->st_cn.errs[i],
		     ((omi_errlist[i]) ? omi_errlist[i] : "unknown")));
    }
    if (nerrs != omi_nerrs)
	NON_IA64_ONLY(OMI_DBG((omi_debug,"gtcm_server:\tNumber of errors (sum): %ld\n",
		 nerrs)));
	IA64_ONLY(OMI_DBG((omi_debug,"gtcm_server:\tNumber of errors (sum): %d\n",
		 nerrs)));
    NON_IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of bytes received: %ld\n",
	     omi_brecv)));
    IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of bytes received: %d\n",
	     omi_brecv)));
    NON_IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of bytes sent: %ld\n",
	     omi_bsent)));
    IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server:\tNumber of bytes sent: %d\n",
	     omi_bsent)));

    return;

}
