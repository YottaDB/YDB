/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsdbver.h"
#include "gtm_multi_thread.h"

/* #GTM_THREAD_SAFE : The below function (bml_newmap) is thread-safe */
void bml_newmap(blk_hdr_ptr_t ptr, uint4 size, trans_num curr_tn)
{
	sm_uc_ptr_t bptr;

	ptr->bver = GDSVCURR;
	ptr->bsiz = size;
	ptr->levl = LCL_MAP_LEVL;
	ptr->tn = curr_tn;
	bptr = (sm_uc_ptr_t)ptr + SIZEOF(blk_hdr);
	size -= SIZEOF(blk_hdr);
	*bptr++ = THREE_BLKS_FREE;
	memset(bptr, FOUR_BLKS_FREE, size - 1);
}
