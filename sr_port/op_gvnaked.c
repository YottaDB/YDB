/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include <rtnhdr.h>
#include "mv_stent.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "subscript.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "sgnl.h"
#include "mvalconv.h"
#include "tp_set_sgm.h"
#include "dpgbldir.h"

GBLREF	bool		undef_inhibit;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	uint4		dollar_tlevel;

error_def(ERR_GVNAKED);
error_def(ERR_MAXNRSUBSCRIPTS);

STATICFNDCL void op_gvnaked_common(int count, int hash_code_dummy, mval *val_arg, va_list var);

void op_gvnaked(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...)
{
	va_list		var;
	VMS_ONLY(int	count;)

	VAR_START(var, val_arg);
	VMS_ONLY(va_count(count);)
	op_gvnaked_common(UNIX_ONLY_COMMA(count_arg+1) VMS_ONLY_COMMA(count+1) 0, val_arg, var);
	va_end(var);
}

/* See comment in "gvn.c" (search for OC_GVNAKED) for why hash_code_dummy is needed */
void op_gvnaked_fast(UNIX_ONLY_COMMA(int count_arg) int hash_code_dummy, mval *val_arg, ...)
{
	va_list		var;
	VMS_ONLY(int	count;)

	VAR_START(var, val_arg);
	VMS_ONLY(va_count(count);)
	op_gvnaked_common(UNIX_ONLY_COMMA(count_arg) VMS_ONLY_COMMA(count) hash_code_dummy, val_arg, var);
	va_end(var);
}

STATICFNDEF void op_gvnaked_common(int count, int hash_code_dummy, mval *val_arg, va_list var)
{
	boolean_t	was_null, is_null, sbs_cnt;
	mval		*val;
	int		max_key;
	unsigned char	*ptr, *end_ptr;
	gd_region	*reg, *reg_start, *reg_top;
	gd_addr		*addr_ptr;
	ht_ent_mname	*tabent;
	gv_namehead	*gvt;
	gvnh_reg_t	*gvnh_reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!gv_currkey || (0 == gv_currkey->prev) || (0 == gv_currkey->end))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVNAKED);
	assertpro(1 <= --count);	/* -- is to ignore "hash_code_dummy" parameter from count */
	sbs_cnt = 0;
	if (1 < count)
	{	/* Use of naked reference can cause increase in number of subscripts.   So count the subscripts */
		ptr = gv_currkey->base;
		end_ptr = ptr + gv_currkey->prev;
		while (ptr < end_ptr)
			if (KEY_DELIMITER == *ptr++)
				sbs_cnt++;
		if (MAX_GVSUBSCRIPTS < (count + sbs_cnt))
		{
			gv_currkey->end = 0;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
		}
	}
	/* else naked reference will not increase number of subscripts, so do not worry about exceeding the limit */
	reg = gv_cur_region;
	assert(NULL != reg);
	/* If no global that this process has referenced spans regions, we can be sure whatever region
	 * "gv_cur_region" points to is the region where the naked reference will map to as well.
	 */
	if (TREF(spangbl_seen))
	{	/* It is possible the unsubscripted global name corresponding to this naked reference spans multiple
		 * regions. In this case, we need to figure out what region the complete naked reference maps to
		 * But before that, we need to find out which global directory (of all the ones this process has opened
		 * in its lifetime) contains "gv_cur_region". And from there figure out the gvnh_reg_t structure
		 * corresponding to the unsubscripted global name.
		 */
		for (addr_ptr = get_next_gdr(NULL); NULL != addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			reg_start = addr_ptr->regions;
			reg_top = reg_start + addr_ptr->n_regions;
			if ((reg >= reg_start) && (reg < reg_top))
				break;
		}
		assert(NULL != addr_ptr);
		tabent = lookup_hashtab_mname((hash_table_mname *)addr_ptr->tab_ptr, &gv_target->gvname);
		assert(NULL != tabent);
		gvnh_reg = (gvnh_reg_t *)tabent->value;
		gvt = gvnh_reg->gvt;
		/* Assert that the unsubscripted gv_target has the same collation characteristics as the current "gv_target".
		 * This makes it safe to use the current "gv_target" for the COPY_SUBS_TO_GVCURRKEY macro.
		 */
		assert(gv_target->collseq == gvt->collseq);
		assert(gv_target->nct == gvt->nct);
	} else
		gvnh_reg = NULL;
	gv_currkey->end = gv_currkey->prev;
	val = val_arg;
	is_null = FALSE;
	was_null = TREF(gv_some_subsc_null);
	if (!val->mvtype && !undef_inhibit)
		gv_currkey->end = 0;
	for ( ; ; )
	{
		COPY_SUBS_TO_GVCURRKEY(val, reg, gv_currkey, was_null, is_null);
			/* updates gv_currkey, was_null, is_null */
		if (0 < --count)
			val = va_arg(var, mval *);
		else
			break;
	}
	if (TREF(spangbl_seen))
	{	/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH
		 * (e.g. setting gv_cur_region for spanning globals).
		 */
		GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, addr_ptr, gv_currkey, reg);
	} else
		TREF(gd_targ_gvnh_reg) = NULL;
	/* Now that "gv_cur_region" is setup correctly for both spanning and non-spanning globals, do GVSUBOFLOW check */
	max_key = gv_cur_region->max_key_size;
	if (gv_currkey->end >= max_key)
		ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	if (IS_REG_BG_OR_MM(reg))
	{
		assert(INVALID_GV_TARGET != gv_target);
                if (dollar_tlevel)
			tp_set_sgm();
		GVCST_ROOT_SEARCH;
		assert(gv_target->gd_csa == cs_addrs);
	}
	TREF(gv_some_subsc_null) = was_null; /* if true, it indicates there is a null subscript (other than the last subscript)
						in current key */
	TREF(gv_last_subsc_null) = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && (NEVER == reg->null_subs))
		sgnl_gvnulsubsc();
	return;
}
