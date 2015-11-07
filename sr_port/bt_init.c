/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

void bt_init(sgmnt_addrs *csa)
{
	sgmnt_data_ptr_t csd;

	csd = csa->hdr;
	csa->ti = &csd->trans_hist;
	if (dba_mm != csd->acc_meth)
	{	/* BT structures are NOT maintained for MM */
		csa->bt_header = (bt_rec_ptr_t)((sm_uc_ptr_t) csd + csa->nl->bt_header_off);
		csa->bt_base = (bt_rec_ptr_t)((sm_uc_ptr_t) csd + csa->nl->bt_base_off);
		csa->th_base = (th_rec_ptr_t)((sm_uc_ptr_t) csd + csa->nl->th_base_off);
	}
	return;
}
