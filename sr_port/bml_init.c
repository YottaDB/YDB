/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>		/* for DSK_WRITE_NOCACHE macro */

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

GBLREF jnl_gbls_t 	jgbl;

int4 bml_init(gd_region *reg, block_id bml, trans_num blktn)
{
	blk_hdr_ptr_t		ptr;
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	uint4			size;
	int4			status;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	size = BM_SIZE(csd->bplmap);
	/* Allocate full block. bml_newmap will set the write size. DSK_WRITE_NOCACHE will write part or all of it as appropriate */
	ptr = (blk_hdr_ptr_t)malloc(csd->blk_size);
	bml_newmap(ptr, size, blktn, csd->desired_db_format);
	/* status holds the status of any error return from DSK_WRITE_NOCACHE */
	DSK_WRITE_NOCACHE(reg, bml, (sm_uc_ptr_t)ptr, csd->desired_db_format, status);
	free(ptr);
	return status;
}
