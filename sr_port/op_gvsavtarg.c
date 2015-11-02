/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gdskill.h"		/* needed for tp.h */
#include "gdscc.h"		/* needed for tp.h */
#include "filestruct.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "process_gvt_pending_list.h"

GBLREF	gd_binding	*gd_map;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	spdesc		stringpool;
GBLREF  uint4           dollar_tlevel;
GBLREF	sgm_info	*first_sgm_info, *sgm_info_ptr;

void op_gvsavtarg(mval *v)
{
	int			len, align_len;
	unsigned char		*c;
	DEBUG_ONLY(
		unsigned char	*tmpc;
	)
	short			end;
	gvsavtarg_t		*gvsavtarg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	v->mvtype = 0;	/* BYPASSOK */ /* so stp_gcol (if invoked below) can free up space currently
					* occupied by this to-be-overwritten mval */
	if (NULL == gv_currkey)
	{	/* Simplest case, finish it off */
		v->str.len = 0;
		v->mvtype = MV_STR;
		return;
	}
	assert(NULL != TREF(gd_targ_addr));
	assert((NULL != gv_target) || (0 == gv_currkey->end));
	/* The way savtarg/rectarg works is by saving and restoring a copy of "gv_target". This assumes that once gv_target
	 * has been allocated and used in a savtarg, the memory is never freed at least until the rectarg is completed.
	 * gvtargets in the pending list could be freed and reallocated so such gvtargets should never be the current
	 * "gv_target" global in case we are in savtarg (or else at rectarg time the saved gv_target could have been freed).
	 * Assert accordingly.
	 */
	assert((NULL == gv_target) || !is_gvt_in_pending_list(gv_target));
	end = gv_currkey->end;
	len = (int)(end + GVSAVTARG_FIXED_SIZE);
	align_len = len + (GVSAVTARG_ALIGN_BNDRY - 1); /* is for 8-byte alignment of v->str.addr */
	ENSURE_STP_FREE_SPACE(align_len);
	v->str.len = len;
	c = (unsigned char *)(ROUND_UP2((UINTPTR_T)stringpool.free, GVSAVTARG_ALIGN_BNDRY));
	assert((c + len) <= (stringpool.free + align_len));
	v->str.addr = (char *)c;
	v->mvtype = MV_STR; /* initialize mvtype now that mval has been otherwise completely set up */
	stringpool.free += align_len;
	gvsavtarg = (gvsavtarg_t *)c;
	/* Now we are going to fill in the structure fields most of which are pointer fields (size upto 8-byte).
	 * This is why we ensure 8-byte alignment of c before typecasting it into gvsavtarg.
	 */
	DEBUG_ONLY(
		assert(!dollar_tlevel || (NULL != first_sgm_info) || (NULL == sgm_info_ptr));
		if (dollar_tlevel && (NULL != sgm_info_ptr))
			DBG_CHECK_IN_FIRST_SGM_INFO_LIST(sgm_info_ptr);
	)
	gvsavtarg->gd_targ_addr = TREF(gd_targ_addr);
	gvsavtarg->gd_map = gd_map;
	gvsavtarg->gv_cur_region = gv_cur_region;
	gvsavtarg->gv_target = gv_target;
	gvsavtarg->gv_last_subsc_null = TREF(gv_last_subsc_null);
	gvsavtarg->gv_some_subsc_null = TREF(gv_some_subsc_null);
	gvsavtarg->prev = gv_currkey->prev;
	gvsavtarg->end = end;
	c += GVSAVTARG_FIXED_SIZE;
	if (0 < end)
	{
		assert(KEY_DELIMITER != gv_currkey->base[0]);
		assert(KEY_DELIMITER == gv_currkey->base[end - 1]);
		assert(KEY_DELIMITER == gv_currkey->base[end]);
		memcpy(c, gv_currkey->base, end);
	}
	return;
}
