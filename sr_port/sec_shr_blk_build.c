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
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "probe.h"
#if 	defined(__alpha) && defined(__VMS)
#include "gtmsecshr.h"
#endif
#include "sec_shr_blk_build.h"

int sec_shr_blk_build(cw_set_element *cse, sm_uc_ptr_t base_addr, trans_num ctn)
{
	blk_segment	*seg, *stop_ptr, *array;
	unsigned char	*ptr;

	array = (blk_segment *)cse->upd_addr;
	if (!(GTM_PROBE(sizeof(blk_segment), array, READ)) || !(GTM_PROBE(sizeof(blk_hdr), base_addr, WRITE)))
		return FALSE;

	/* block transaction number needs to be modified first. see comment in gvcst_blk_build as to why */
	((blk_hdr*)base_addr)->tn = ctn;
	((blk_hdr*)base_addr)->bsiz = array->len;
	((blk_hdr*)base_addr)->levl = cse->level;

	if (cse->forward_process)
	{
		ptr = base_addr + sizeof(blk_hdr);
		for (seg = array + 1, stop_ptr = (blk_segment *)array->addr;  seg <= stop_ptr;  seg++)
		{
			if (!(GTM_PROBE(sizeof(blk_segment), seg, READ)))
				return FALSE;
			if (!(GTM_PROBE(seg->len, seg->addr, READ)))
				return FALSE;
			if (!(GTM_PROBE(seg->len, ptr, WRITE)))
				return FALSE;
			memmove(ptr, seg->addr, seg->len);
			ptr += seg->len;
		}
	} else
	{
		ptr = base_addr + array->len;
		for  (seg = (blk_segment*)array->addr, stop_ptr = array;  seg > stop_ptr;  seg--)
		{
			if (!(GTM_PROBE(sizeof(blk_segment), seg, READ)))
				return FALSE;
			ptr -= seg->len;
			if (!(GTM_PROBE(seg->len, seg->addr, READ)))
				return FALSE;
			if (!(GTM_PROBE(seg->len, ptr, WRITE)))
				return FALSE;
			memmove(ptr, seg->addr, seg->len);
		}
	}
	return TRUE;
}
