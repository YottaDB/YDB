/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsdbver.h"
#include "gds_blk_upgrade.h"

#include "send_msg.h"

error_def(ERR_GTMCURUNSUPP);

int4 gds_blk_upgrade(sm_uc_ptr_t gds_blk_src, sm_uc_ptr_t gds_blk_trg, int4 blksize, enum db_ver *ondsk_blkver)
{
	/* this was for V4 upgrade - delete it */
	assert(FALSE);
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMCURUNSUPP);
	return SS_NORMAL;
}
