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

/* This is the routine used by both db_init() and mu_rndwn_file() for initializing appropriate structures. */

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

void	db_common_init(gd_region *reg, sgmnt_addrs *csa, sgmnt_data_ptr_t csd)
{
	csa->bmm = csd->master_map;
	csa->reorg_last_dest = 0;	/* For mupip reorg swap operation */
	csa->region = reg;		/* initialize the back-link */
	reg->max_rec_size = csd->max_rec_size;
	reg->max_key_size = csd->max_key_size;
 	reg->null_subs = csd->null_subs;
	reg->jnl_state = csd->jnl_state;
	reg->jnl_file_len = csd->jnl_file_len;		/* journal file name length */
	memcpy(reg->jnl_file_name, csd->jnl_file_name, reg->jnl_file_len);	/* journal file name */
	reg->jnl_alq = csd->jnl_alq;
	reg->jnl_deq = csd->jnl_deq;
	reg->jnl_buffer_size = csd->jnl_buffer_size;
	reg->jnl_before_image = csd->jnl_before_image;
	bt_init(csa);
}
