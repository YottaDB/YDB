/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

int sec_shr_blk_build(sgmnt_addrs *csa, sgmnt_data_ptr_t csd, boolean_t is_bg,
			cw_set_element *cse, sm_uc_ptr_t base_addr, trans_num ctn)
{
	blk_segment	*seg, *stop_ptr, *array;
	unsigned char	*ptr;
	boolean_t	do_accounting;

	array = (blk_segment *)cse->upd_addr;
	assert(csa->read_write);
	if (csa->now_crit)	/* csa->now_crit can be FALSE if we are finishing bg_update_phase2 part of the commit */
		do_accounting = TRUE;	/* used by SECSHR_ACCOUNTING macro */
	if (!(GTM_PROBE(SIZEOF(blk_segment), array, READ)))
	{
		SECSHR_ACCOUNTING(4);
		SECSHR_ACCOUNTING(__LINE__);
		SECSHR_ACCOUNTING((INTPTR_T)cse->upd_addr);
		SECSHR_ACCOUNTING(SIZEOF(blk_segment));
		assert(FALSE);
		return FALSE;
	}
	if (!(GTM_PROBE(SIZEOF(blk_hdr), base_addr, WRITE)))
	{
		SECSHR_ACCOUNTING(4);
		SECSHR_ACCOUNTING(__LINE__);
		SECSHR_ACCOUNTING((INTPTR_T)base_addr);
		SECSHR_ACCOUNTING(SIZEOF(blk_hdr));
		assert(FALSE);
		return FALSE;
	}
	/* block transaction number needs to be modified first. see comment in gvcst_blk_build as to why */
	((blk_hdr_ptr_t)base_addr)->bver = GDSVCURR;
	assert(csa->now_crit || (ctn < csd->trans_hist.curr_tn));
	assert(!csa->now_crit || (ctn == csd->trans_hist.curr_tn));
	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	((blk_hdr_ptr_t)base_addr)->bsiz = UINTCAST(array->len);
	((blk_hdr_ptr_t)base_addr)->levl = cse->level;

	if (cse->forward_process)
	{
		ptr = base_addr + SIZEOF(blk_hdr);
		for (seg = array + 1, stop_ptr = (blk_segment *)array->addr;  seg <= stop_ptr;  seg++)
		{
			if (!(GTM_PROBE(SIZEOF(blk_segment), seg, READ)))
			{
				SECSHR_ACCOUNTING(4);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)seg);
				SECSHR_ACCOUNTING(SIZEOF(blk_segment));
				assert(FALSE);
				return FALSE;
			}
			if (!seg->len)
				continue;	/* GTM_PROBE on a zero length returns FALSE so check for it explicitly here */
			if (!(GTM_PROBE(seg->len, seg->addr, READ)))
			{
				SECSHR_ACCOUNTING(5);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)seg);
				SECSHR_ACCOUNTING((INTPTR_T)seg->addr);
				SECSHR_ACCOUNTING(seg->len);
				assert(FALSE);
				return FALSE;
			}
			if (!(GTM_PROBE(seg->len, ptr, WRITE)))
			{
				SECSHR_ACCOUNTING(6);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)seg);
				SECSHR_ACCOUNTING((INTPTR_T)ptr);
				SECSHR_ACCOUNTING((INTPTR_T)seg->addr);
				SECSHR_ACCOUNTING(seg->len);
				assert(FALSE);
				return FALSE;
			}
			DBG_BG_PHASE2_CHECK_CR_IS_PINNED(csa, seg);
			memmove(ptr, seg->addr, seg->len);
			ptr += seg->len;
		}
	} else
	{
		ptr = base_addr + array->len;
		for  (seg = (blk_segment*)array->addr, stop_ptr = array;  seg > stop_ptr;  seg--)
		{
			if (!(GTM_PROBE(SIZEOF(blk_segment), seg, READ)))
			{
				SECSHR_ACCOUNTING(4);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)seg);
				SECSHR_ACCOUNTING(SIZEOF(blk_segment));
				assert(FALSE);
				return FALSE;
			}
			if (!seg->len)
				continue;	/* GTM_PROBE on a zero length returns FALSE so check for it explicitly here */
			if (!(GTM_PROBE(seg->len, seg->addr, READ)))
			{
				SECSHR_ACCOUNTING(5);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)seg);
				SECSHR_ACCOUNTING((INTPTR_T)seg->addr);
				SECSHR_ACCOUNTING(seg->len);
				assert(FALSE);
				return FALSE;
			}
			ptr -= seg->len;
			if (!(GTM_PROBE(seg->len, ptr, WRITE)))
			{
				SECSHR_ACCOUNTING(6);
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING((INTPTR_T)seg);
				SECSHR_ACCOUNTING((INTPTR_T)ptr);
				SECSHR_ACCOUNTING((INTPTR_T)seg->addr);
				SECSHR_ACCOUNTING(seg->len);
				assert(FALSE);
				return FALSE;
			}
			DBG_BG_PHASE2_CHECK_CR_IS_PINNED(csa, seg);
			memmove(ptr, seg->addr, seg->len);
		}
	}
	return TRUE;
}
