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

void bt_init(sgmnt_addrs *cs)
{
	sgmnt_data_ptr_t base;

	base = cs->hdr;
	cs->ti = &base->trans_hist;
	cs->bt_header = (bt_rec_ptr_t)((sm_uc_ptr_t) base + cs->nl->bt_header_off);
	cs->bt_base = (bt_rec_ptr_t)((sm_uc_ptr_t) base + cs->nl->bt_base_off);
	cs->th_base = (th_rec_ptr_t)((sm_uc_ptr_t) base + cs->nl->th_base_off);
	return;
}
