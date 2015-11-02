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
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"
#include "gdscc.h"		/* needed for tp.h */
#include "filestruct.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "op.h"

#define DIR_ROOT 1

GBLREF	gd_binding	*gd_map;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	sgm_info	*first_sgm_info, *sgm_info_ptr;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data	*cs_data;
GBLREF	uint4		dollar_tlevel;

void op_gvrectarg(mval *v)
{
	int			len;
	unsigned char		*c;
	short			end;
	sgm_info		*si;
	gd_region		*reg;
	gvsavtarg_t		*gvsavtarg, gvst_tmp;
	DEBUG_ONLY(
		int		n;
		unsigned char	*tmpc;
	)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* You might be somewhat apprehensive at the seemingly cavalier use of assertpro() in this routine.
	 * First, let me explain myself.  The mvals passed to RECTARG are supposed to come only from SAVTARG,
	 * and are to represent the state of gv_currkey when SAVTARG was done.  Consequently, there are
	 * certain preconditions that one can expect.  Namely,
	 *	1) SAVTARG makes all mvals MV_STR's.  If this one isn't, it should be.
	 *	2) If gv_currkey existed for SAVTARG (len is > 0), it had better exist for RECTARG.
	 *	3) All gv_keys end with 00.  When reading this mval, if you run out of characters
	 *		before you see a 0, something is amiss.
	 */
	assertpro(MV_IS_STRING(v));
	len = v->str.len;
	if (0 == len)
	{
		if (NULL != gv_currkey)
		{	/* Reset gv_currkey.
			 * The following code (should be maintained in parallel with similar code in op_svput.c.
			 */
			gv_currkey->end = gv_currkey->prev = 0;
			gv_currkey->base[0] = KEY_DELIMITER;
		}
		return;
	}
	assertpro(NULL != gv_currkey);
	assert(GVSAVTARG_FIXED_SIZE <= len);
	c = (unsigned char *)v->str.addr;
	/* op_gvsavtarg had ensured 8-byte alignment of v->str.addr but it is possible a "stp_gcol" was invoked in between
	 * which could have repointed v->str.addr to a non-8-byte-aligned address. In this rare case, do special processing
	 * before dereferencing the fields of the structure.
	 */
	if ((UINTPTR_T)c == ROUND_UP2((UINTPTR_T)c, GVSAVTARG_ALIGN_BNDRY))
		gvsavtarg = (gvsavtarg_t *)c;
	else
	{
		gvsavtarg = &gvst_tmp;
		memcpy(gvsavtarg, c, GVSAVTARG_FIXED_SIZE);
	}
	TREF(gd_targ_addr) = gvsavtarg->gd_targ_addr;
	gd_map = gvsavtarg->gd_map;
	reg = gvsavtarg->gv_cur_region;
	TP_CHANGE_REG(reg);	/* sets gv_cur_region, cs_addrs, cs_data */
	gv_target = gvsavtarg->gv_target;
	assert(dollar_tlevel || ((NULL == first_sgm_info) && (NULL == sgm_info_ptr)));
	if (dollar_tlevel)
	{	/* Restore sgm_info_ptr if needed.
		 * a) If first_sgm_info is NULL (possible if op_gvrectarg is called as part of tp restart handling),
		 *	then no region is included in this TP so set sgm_info_ptr to NULL.
		 * b) If the region has not yet been opened in this TP transaction (si->tp_set_sgm_done is FALSE), then
		 *	again set sgm_info_ptr to NULL.
		 * c) If the region corresponding to the restored $reference has already been opened as part
		 * 	of this TP transaction, then set sgm_info_ptr to point to the corresponding tp structure.
		 */
		assert((NULL == gv_target) || (cs_addrs == gv_target->gd_csa));
		if ((NULL == first_sgm_info) || (NULL == cs_addrs) || (NULL == (si = cs_addrs->sgm_info_ptr))
			|| !si->tp_set_sgm_done)
		{	/* Note: Assignment in above test
			 * Case (a) OR (b).
			 * In case of (b), note that first_sgm_info could be non-NULL while sgm_info_ptr is NULL.
			 * We can fix the latter to be a non-NULL value by invoking "tp_set_sgm". But we dont want
			 * to do that here because it can cause restarts (due to the "insert_region" call) and this
			 * function can be invoked by "gtm_trigger_fini" which cannot handle tp restarts within.
			 * We therefore keep first_sgm_info and sgm_info_ptr temporarily out-of-sync until the NEXT
			 * global reference (op_gvname, op_gvnaked or op_gvextnam) which would invoke "tp_set_sgm".
			 * The two need to be in sync BEFORE any database functions (gvcst_search.c, gvcst_get.c etc.)
			 * are invoked as they assume sgm_info_ptr is non-NULL if dollar_tlevel is non-zero.
			 */
			sgm_info_ptr = NULL;
		} else
		{	/* Case (c) */
			sgm_info_ptr = si;
			DBG_CHECK_IN_FIRST_SGM_INFO_LIST(si);
		}
		assert((NULL != first_sgm_info) || (NULL == sgm_info_ptr));
	}
	TREF(gv_last_subsc_null) = gvsavtarg->gv_last_subsc_null;
	TREF(gv_some_subsc_null) = gvsavtarg->gv_some_subsc_null;
	gv_currkey->prev = gvsavtarg->prev;
	gv_currkey->end = end = gvsavtarg->end;
	assert(gv_currkey->end < gv_currkey->top);
	c += GVSAVTARG_FIXED_SIZE;
	assert(end == (len - GVSAVTARG_FIXED_SIZE));
	if (0 < end)
	{
		memcpy(gv_currkey->base, c, end);
		assert(KEY_DELIMITER == gv_currkey->base[end - 1]);
	}
	gv_currkey->base[end] = KEY_DELIMITER;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	return;
}
