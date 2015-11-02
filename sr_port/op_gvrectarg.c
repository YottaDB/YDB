/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
GBLREF	sgm_info	*sgm_info_ptr;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data	*cs_data;
GBLREF	uint4		dollar_tlevel;

void op_gvrectarg(mval *v)
{
	int			len;
	unsigned char		*c;
	short			end;
	gd_region		*reg;
	gvsavtarg_t		*gvsavtarg, gvst_tmp;
	DEBUG_ONLY(
		int		n;
		unsigned char	*tmpc;
	)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* You might be somewhat apprehensive at the seemingly cavalier use of GTMASSERT in this routine.
	 * First, let me explain myself.  The mvals passed to RECTARG are supposed to come only from SAVTARG,
	 * and are to represent the state of gv_currkey when SAVTARG was done.  Consequently, there are
	 * certain preconditions that one can expect.  Namely,
	 *	1) SAVTARG makes all mvals MV_STR's.  If this one isn't, it should be.
	 *	2) If gv_currkey existed for SAVTARG (len is > 0), it had better exist for RECTARG.
	 *	3) All gv_keys end with 00.  When reading this mval, if you run out of characters
	 *		before you see a 0, something is amiss.
	 */
	if (!MV_IS_STRING(v))
		GTMASSERT;
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
	if (NULL == gv_currkey)
		GTMASSERT;
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
	if (dollar_tlevel)
		sgm_info_ptr = gvsavtarg->sgm_info_ptr;
	assert(dollar_tlevel || (NULL == sgm_info_ptr));
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
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC;
	DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
	return;
}
