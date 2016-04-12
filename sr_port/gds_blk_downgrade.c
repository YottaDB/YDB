/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "gds_blk_downgrade.h"
#ifdef VMS
#include "copy.h"
#endif

#define SPACE_NEEDED (SIZEOF(blk_hdr) - SIZEOF(v15_blk_hdr))

GBLREF	boolean_t	dse_running;

void gds_blk_downgrade(v15_blk_hdr_ptr_t gds_blk_trg, blk_hdr_ptr_t gds_blk_src)
{
	sm_uc_ptr_t	trg_p, src_p;
	v15_trans_num	v15tn;
	trans_num	tn;
	uint4		bsiz, levl;
	int		movesize;

	/* Note that this routine is written in such a fashion that it is possible for the
	   source and target blocks to point to the same area.
	*/
	assert(gds_blk_trg);
	assert(gds_blk_src);
	assert(SIZEOF(v15_blk_hdr) > gds_blk_src->bver);	/* Check it is a GDSVCURR blk to begin with */
	assert(0 == ((long)gds_blk_trg & 0x7));			/* Buffer alignment checks (8 byte) */
	assert(0 == ((long)gds_blk_src & 0x7));
	trg_p = (sm_uc_ptr_t)gds_blk_trg + SIZEOF(v15_blk_hdr);
	src_p = (sm_uc_ptr_t)gds_blk_src + SIZEOF(blk_hdr);
	bsiz = gds_blk_src->bsiz;
	assert(MAX_BLK_SZ >= bsiz);
	assert(SIZEOF(blk_hdr) <= bsiz);
	tn = gds_blk_src->tn;
	assert((MAX_TN_V4 >= tn) || dse_running);
	levl = gds_blk_src->levl;
	movesize = bsiz - SIZEOF(blk_hdr);
	if ((sm_uc_ptr_t)gds_blk_trg != (sm_uc_ptr_t)gds_blk_src)
	{	/* Normal case, downgrade is to a new buffer. Our simple check is quicker
		   than just always doing memmove() would be. But assert they are at least
		   one block away just in case...
		 */
		DEBUG_ONLY(
			if ((sm_uc_ptr_t)gds_blk_trg > (sm_uc_ptr_t)gds_blk_src)
				assert((sm_uc_ptr_t)gds_blk_trg >= ((sm_uc_ptr_t)gds_blk_src + bsiz));
			else
				assert((sm_uc_ptr_t)gds_blk_src >= ((sm_uc_ptr_t)gds_blk_trg + bsiz));
		)
		memcpy(trg_p, src_p, movesize);
	} else
		memmove(trg_p, src_p, movesize);

	gds_blk_trg->bsiz = bsiz - SPACE_NEEDED;
	gds_blk_trg->levl = levl;
	v15tn = (v15_trans_num) tn;
	UNIX_ONLY(gds_blk_trg->tn = v15tn);
	VMS_ONLY(PUT_ULONG(&gds_blk_trg->tn, v15tn));
}
