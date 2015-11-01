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
 *  omi_dump_pkt.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header: /cvsroot/sanchez-gtm/gtm/sr_unix_cm/omi_dmp_pkt.c,v 1.1.1.1 2001/05/16 14:01:54 marcinim Exp $";
#endif

#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "mdef.h"
#include "omi.h"

GBLREF char	*omi_pklog;
GBLREF int	 omi_pkdbg;
GBLREF char	*omi_oprlist[];


void omi_dump_pkt(omi_conn *cptr)
{
#ifndef __linux__

#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

    extern char	*sys_errlist[];

#ifdef __osf__
#pragma pointer_size (restore)
#endif

#endif
    extern int	 errno;

    char	*ptr, *end, *chr;
    omi_vi	 vi, mlen, xlen;
    omi_li	 li, nx;
    omi_si	 si;
    int		 i, j, len, buf[5];

    if (!omi_pklog || INV_FD_P(cptr->pklog))
	return;

    if (write(cptr->pklog, cptr->bptr, cptr->blen) < 0)
	OMI_DBG((omi_debug, "gtcm_server: write(): %s\n", sys_errlist[errno]));

    if (!omi_pkdbg)
	return;

    OMI_DBG((omi_debug, "gtcm_server: connection %d:", cptr->stats.id));

    ptr = cptr->bptr;
    OMI_VI_READ(&mlen, ptr);
    OMI_DBG((omi_debug, " %d bytes", mlen.value));

    if (cptr->exts & OMI_XTF_BUNCH) {
	OMI_LI_READ(&nx, ptr);
	OMI_DBG((omi_debug, " %d transactions in bunch", nx.value));
    }
    else {
	nx.value = 1;
	xlen     = mlen;
    }
    OMI_DBG((omi_debug, "\n"));

    for (i = 0; i < nx.value; i++) {
	if (cptr->exts & OMI_XTF_BUNCH)
	    OMI_VI_READ(&xlen, ptr);
	end  = ptr + xlen.value;
	ptr += 3;
	OMI_SI_READ(&si, ptr);
	ptr += 8;
	OMI_DBG((omi_debug, "    "));
	if (cptr->exts & OMI_XTF_BUNCH)
	    OMI_DBG((omi_debug, "bunch %d, ", i));
	OMI_DBG((omi_debug, "%s (%d bytes)\n", (omi_oprlist[si.value])
		 ? omi_oprlist[si.value] : "unknown", xlen.value));
	chr  = (char *)buf;
	while (ptr < end) {
	    OMI_DBG((omi_debug, "\t"));
	    if ((len = end - ptr) > 20)
		len = 20;
	    memcpy(chr, ptr, len);
	    ptr += len;
	    for (j = len; j < 20; j++)
		chr[j] = '\0';
	    for (j = 0; j < 5; j++)
		OMI_DBG((omi_debug, "%08x ", buf[j]));
	    for (j = 0; j < 20; j++)
		if (j >= len)
		    chr[j] = ' ';
		else if (chr[j] < 32 || chr[j] > 126)
		    chr[j] = '.';
	    OMI_DBG((omi_debug, "%20s\n", chr));
	}
    }

    return;

}
