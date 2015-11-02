/****************************************************************
 *                                                              *
 *      Copyright 2007 Fidelity Information Services, Inc       *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "fnpc.h"
#include "gtm_stdio.h"

#ifdef DEBUG
GBLREF  uint4   process_id;
GBLREF  int     c_miss;                         /* cache misses (debug) */
GBLREF  int     c_hit;                          /* cache hits (debug) */
GBLREF  int     c_small;                        /* scanned small string brute force */
GBLREF  int     c_small_pcs;                    /* chars scanned by small scan */
GBLREF  int     c_pskip;                        /* number of pieces "skipped" */
GBLREF  int     c_pscan;                        /* number of pieces "scanned" */
GBLREF  int     c_parscan;                      /* number of partial scans (partial cache hits) */
GBLREF  int     cs_miss;                        /* cache misses (debug) */
GBLREF  int     cs_hit;                         /* cache hits (debug) */
GBLREF  int     cs_small;                       /* scanned small string brute force */
GBLREF  int     cs_small_pcs;                   /* chars scanned by small scan */
GBLREF  int     cs_pskip;                       /* number of pieces "skipped" */
GBLREF  int     cs_pscan;                       /* number of pieces "scanned" */
GBLREF  int     cs_parscan;                     /* number of partial scans (partial cache hits) */
GBLREF  int     c_clear;                        /* cleared due to (possible) value change */

void fnpc_stats(void)
{
        FPRINTF(stderr, "process id:          %d\n", process_id);
        FPRINTF(stderr, "fnpc cache clears:   %d\n", c_clear);
        FPRINTF(stderr, "Reference Piece:\n");
        FPRINTF(stderr, "  fnpc cache miss:     %d\n", c_miss);
        FPRINTF(stderr, "  fnpc cache hit:      %d\n", c_hit);
        FPRINTF(stderr, "  fnpc pieces skipped: %d\n", c_pskip);
        FPRINTF(stderr, "  fnpc pieces scanned: %d\n", c_pscan);
        FPRINTF(stderr, "  fnpc partial scans:  %d\n", c_parscan);
        FPRINTF(stderr, "  small string scans:  %d\n", c_small);
        FPRINTF(stderr, "  small str pcs scnd:  %d\n", c_small_pcs);
        FPRINTF(stderr, "Set Piece:\n");
        FPRINTF(stderr, "  fnpc cache miss:     %d\n", cs_miss);
        FPRINTF(stderr, "  fnpc cache hit:      %d\n", cs_hit);
        FPRINTF(stderr, "  fnpc pieces skipped: %d\n", cs_pskip);
        FPRINTF(stderr, "  fnpc pieces scanned: %d\n", cs_pscan);
        FPRINTF(stderr, "  fnpc partial scans:  %d\n", cs_parscan);
        FPRINTF(stderr, "  small string scans:  %d\n", cs_small);
        FPRINTF(stderr, "  small str pcs scnd:  %d\n", cs_small_pcs);
}
#endif
