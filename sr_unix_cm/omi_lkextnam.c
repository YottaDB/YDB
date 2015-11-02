/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  omi_lkextnam.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_string.h"

#include "omi.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "error.h"
#include "mlk_pvtblk_insert.h"
#include "dpgbldir.h"
#include "mlk_region_lookup.h"

GBLREF mlk_pvtblk	*mlk_pvt_root;

int
omi_lkextnam(
    omi_conn	*cptr,
    uns_short	 len,
    char	*ref,
    char	*data)
{
    mlk_pvtblk	*r;
    sgmnt_addrs	*sa;
    gd_region	*reg;
    mval	 ext, lck;
    char	*ptr, *end;
    int		 subcnt, elen;
    omi_li	 li;
    omi_si	 si;
    bool	 rv;


/*  Pointers into the global reference */
    ptr = ref;
    end = ref + len;

/*  Initialize part of the mval */
    ext.mvtype = MV_STR;

/*  Refine the gd_addr given this environment */
    OMI_LI_READ(&li, ptr);
    if (ptr + li.value > end)
	return -OMI_ER_PR_INVGLOBREF;
    ext.str.len  = li.value;
    ext.str.addr = ptr;
    cptr->ga     = zgbldir(&ext);
    ptr         += li.value;
    elen         = (int)(end - ptr);

/*  Refine the gd_addr given this name */
    OMI_SI_READ(&si, ptr);
    if (!si.value || ptr + si.value > end)
	return -OMI_ER_PR_INVGLOBREF;
    lck.str.len   = si.value;
    lck.str.addr  = ptr;
    ptr          += si.value;
    subcnt        = 1;

/*  Refine the gd_addr given these subscripts */
    while (ptr < end) {
	OMI_SI_READ(&si, ptr);
	if (!si.value || ptr + si.value > end)
	    return -OMI_ER_PR_INVGLOBREF;
	ptr += si.value;
	subcnt++;
    }
    lck.mvtype = ext.mvtype = MV_STR;
    reg        = mlk_region_lookup(&lck, cptr->ga);
    OMI_SI_READ(&si, data);
    r          = (mlk_pvtblk *)malloc(SIZEOF(mlk_pvtblk) + elen + si.value);
    memset(r, 0, SIZEOF(mlk_pvtblk) - 1);
    r->translev      = 1;
    r->subscript_cnt = subcnt;
    r->total_length  = elen;
    memcpy(&r->value[0], lck.str.addr - 1, elen);
    r->value[elen++] = si.value;
    memcpy(&r->value[elen], data, si.value);
    r->region  = reg;
    sa         = &FILE_INFO(r->region)->s_addrs;
    r->ctlptr  = (mlk_ctldata *)sa->lock_addrs[0];
    if (!mlk_pvtblk_insert(r))
    {
	if (r->value[r->total_length] == mlk_pvt_root->value[mlk_pvt_root->total_length]
		&& !memcmp(&r->value[elen],&mlk_pvt_root->value[elen],r->value[r->total_length]))
	{
	    free(r);
	    return TRUE;
	}
	else
	    return FALSE;
    }
    else if (r != mlk_pvt_root)
	return -OMI_ER_DB_UNRECOVER;

    return TRUE;

}
