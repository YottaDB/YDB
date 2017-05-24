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
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "sgnl.h"
#include "mvalconv.h"
#include "tp_set_sgm.h"
#include "min_max.h"

GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgm_info		*first_sgm_info;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF uint4		dollar_tlevel;
GBLREF mstr		extnam_str;
GBLREF gd_region	*gv_cur_region;

STATICFNDCL void op_gvname_common(int count, int hash_code, mval *val_arg, va_list var);

void op_gvname(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...)
{
	int	 	hash_code;
	mval		tmpval;
	va_list		var;
	VMS_ONLY(int	count;)

	tmpval = *val_arg;
	tmpval.str.len = MIN(tmpval.str.len, MAX_MIDENT_LEN);
	COMPUTE_HASH_MSTR(tmpval.str, hash_code);
	VAR_START(var, val_arg);
	VMS_ONLY(va_count(count);)
	op_gvname_common(UNIX_ONLY_COMMA(count_arg+1) VMS_ONLY_COMMA(count+1) hash_code, &tmpval, var);
	va_end(var);
}

void op_gvname_fast(UNIX_ONLY_COMMA(int count_arg) int hash_code, mval *val_arg, ...)
{
	va_list		var;
	VMS_ONLY(int	count;)

	VAR_START(var, val_arg);
	VMS_ONLY(va_count(count);)
	op_gvname_common(UNIX_ONLY_COMMA(count_arg) VMS_ONLY_COMMA(count) hash_code, val_arg, var);
	va_end(var);
}

STATICFNDEF void op_gvname_common(int count, int hash_code, mval *val_arg, va_list var)
{
	boolean_t	was_null, is_null;
	boolean_t	bgormm;
	mval		*val;
	mname_entry	gvname;
	int		max_key;
	gvnh_reg_t	*gvnh_reg;
	char		varstr[MAX_MIDENT_LEN + 1];
	gd_region	*reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	extnam_str.len = 0;
	if (!gd_header)
		gvinit();
	val = val_arg;
	count -= 2;
	gvname.hash_code = hash_code;
	/* GV_BIND_NAME_AND_ROOT_SEARCH invokes "gv_bind_name" which does a "gv_init_reg". That could in turn invoke
	 * "gvcst_init_statsDB" which does a "gvcst_get" and that could invoke "stp_gcol" (through the ENSURE_STP_FREE_SPACE
	 * macro). This garbage collection could move mvals around. "val->str" would be correctly repointed since "val" is
	 * known to "stp_gcol" but if gvname.var_name is a copy of val->str, it would not be repointed and so later usages
	 * of "gvname" in "gv_bind_name" could point to garbage contents. Hence take a copy of the variable in the C-stack
	 * and point to it instead of val->str.addr.
	 */
	gvname.var_name.len = val->str.len;
	memcpy(varstr, val->str.addr, gvname.var_name.len);
	gvname.var_name.addr = varstr;
	TREF(gd_targ_addr) = gd_header;		/* needed by name-level $order/$zprevious and various other functions */
	/* Bind the unsubscripted global name to corresponding region in the global directory map */
	GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &gvname, gvnh_reg);
	/* gv_cur_region will not be set in case gvnh_reg->gvspan is non-NULL. So use region from gvnh_reg */
	reg = gvnh_reg->gd_reg;
	DEBUG_ONLY(bgormm = IS_REG_BG_OR_MM(reg);)
	assert(bgormm || !dollar_tlevel);
	assert(NULL != gv_target);
	assert(gv_currkey->end);
	assert('\0' != gv_currkey->base[0]);	/* ensure the below DBG_CHECK_... assert does a proper check (which it wont
						 * if it finds gv_currkey->base[0] is '\0'
						 */
	/* cs_addrs is not initialized in case gvnh_reg->gvspan is non-NULL. Assert accordingly */
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC((NULL == gvnh_reg->gvspan) ? CHECK_CSA_TRUE : CHECK_CSA_FALSE);
	was_null = is_null = FALSE;
	for ( ; count > 0; count--)
	{
		val = va_arg(var, mval *);
		COPY_SUBS_TO_GVCURRKEY(val, reg, gv_currkey, was_null, is_null);
			/* updates gv_currkey, was_null, is_null */
	}
	/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH (e.g. setting gv_cur_region for spanning globals) */
	GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey, reg);
	/* Now that "gv_cur_region" is setup correctly for both spanning and non-spanning globals, do GVSUBOFLOW check */
	max_key = gv_cur_region->max_key_size;
	if (gv_currkey->end >= max_key)
		ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
	assert(!dollar_tlevel || sgm_info_ptr && (sgm_info_ptr->tp_csa == cs_addrs));
	TREF(gv_some_subsc_null) = was_null; /* if true, it indicates there is a null subscript (other than the last subscript)
						in current key */
	TREF(gv_last_subsc_null) = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && (NEVER == reg->null_subs))
		sgnl_gvnulsubsc();
	TREF(prev_gv_target) = gv_target;	/* note down gv_target in another global for debugging purposes (C9I09-003039) */
	return;
}
