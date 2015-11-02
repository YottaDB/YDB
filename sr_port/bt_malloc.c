/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

void bt_malloc(sgmnt_addrs *csa)
{
	unsigned int	n;
	sgmnt_data_ptr_t	csd;

	csd = csa->hdr;
	/* check that the file header is quad word aligned */
	if ((-(SIZEOF(uint4) * 2) & (sm_long_t)csd) != (sm_long_t)csd)
		GTMASSERT;
	if ((-(SIZEOF(uint4) * 2) & SIZEOF_FILE_HDR(csd)) != SIZEOF_FILE_HDR(csd))
		GTMASSERT;
	csa->nl->bt_header_off = (n = (uint4)(SIZEOF_FILE_HDR(csd)));
	csa->nl->th_base_off = (n += csd->bt_buckets * SIZEOF(bt_rec));	/* hash table */
	csa->nl->th_base_off += SIZEOF(que_ent);				/* tnque comes after fl and bl of blkque */
	csa->nl->bt_base_off = (n += SIZEOF(bt_rec));			/* th_queue anchor referenced above */
	assert((n += (csd->n_bts * SIZEOF(bt_rec))) == (SIZEOF_FILE_HDR(csd)) + (BT_SIZE(csd)));	/* DON'T use n after this */
	bt_init(csa);
	bt_refresh(csa, TRUE);
	return;
}
