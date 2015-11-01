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
	if ((-(sizeof(uint4) * 2) & (sm_long_t)csd) != (sm_long_t)csd)
		GTMASSERT;
	if ((-(sizeof(uint4) * 2) & sizeof(sgmnt_data)) != sizeof(sgmnt_data))
		GTMASSERT;
	csa->nl->bt_header_off = (n = sizeof(sgmnt_data));
	csa->nl->th_base_off = (n += csd->bt_buckets * sizeof(bt_rec));	/* hash table */
	csa->nl->th_base_off += sizeof(que_ent);				/* tnque comes after fl and bl of blkque */
	csa->nl->bt_base_off = (n += sizeof(bt_rec));			/* th_queue anchor referenced above */
	assert((n += (csd->n_bts * sizeof(bt_rec))) == (sizeof(sgmnt_data)) + (BT_SIZE(csd)));	/* DON'T use n after this */
	bt_init(csa);
	bt_refresh(csa);
	return;
}
