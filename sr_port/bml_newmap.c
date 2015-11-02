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

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsdbver.h"

void bml_newmap(blk_hdr_ptr_t ptr, uint4 size, trans_num curr_tn)
{
	sm_uc_ptr_t bptr;

	/* --- similar logic exists in mupip_restore.c, which need to pick up any new updates here --- */
	ptr->bver = GDSVCURR;
	ptr->bsiz = size;
	ptr->levl = LCL_MAP_LEVL;
	ptr->tn = curr_tn;
	bptr = (sm_uc_ptr_t)ptr + SIZEOF(blk_hdr);
	size -= SIZEOF(blk_hdr);
	*bptr++ = THREE_BLKS_FREE;
	memset(bptr, FOUR_BLKS_FREE, size - 1);
}
