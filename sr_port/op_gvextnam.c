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

#include "gtm_string.h"
#include <stdarg.h>

#include "error.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gbldirnam.h"
#include "op.h"
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "dpgbldir.h"
#include "sgnl.h"
#include "mvalconv.h"
#include <rtnhdr.h>
#include "mv_stent.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */

/*the header files below are for environment translation*/
#ifdef UNIX
#include "fgncalsp.h"
#endif
#include "gtm_env_xlate_init.h"

GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF mstr             extnam_str;
GBLREF mval		dollar_zgbldir;
GBLREF gd_addr		*gd_header;
GBLREF gd_region	*gv_cur_region;

STATICFNDCL void op_gvextnam_common(int count, int hash_code, mval *val1, va_list var);

void op_gvextnam(UNIX_ONLY_COMMA(int count_arg) mval *val1, ...)
{
	int	 hash_code;
	mval	*val2, *gblname_mval;
	va_list	var, var_dup;
	VMS_ONLY(int	count;)

	VAR_START(var, val1);
	VAR_COPY(var_dup, var);
	val2 = va_arg(var_dup, mval *);	/* skip env translate mval */
	gblname_mval = va_arg(var_dup, mval *);	/* get at the gblname mval */
	COMPUTE_HASH_MSTR(gblname_mval->str, hash_code);
	VMS_ONLY(va_count(count);)
	op_gvextnam_common(UNIX_ONLY_COMMA(count_arg+1) VMS_ONLY_COMMA(count+1) hash_code, val1, var);
	va_end(var);
	va_end(var_dup);
}

void op_gvextnam_fast(UNIX_ONLY_COMMA(int count_arg) int hash_code, mval *val1, ...)
{
	va_list		var;
	VMS_ONLY(int	count;)

	VAR_START(var, val1);
	VMS_ONLY(va_count(count);)
	op_gvextnam_common(UNIX_ONLY_COMMA(count_arg) VMS_ONLY_COMMA(count) hash_code, val1, var);
	va_end(var);
}

STATICFNDEF void op_gvextnam_common(int count, int hash_code, mval *val1, va_list var)
{
	boolean_t	was_null, is_null;
	mstr		*tmp_mstr_ptr;
	mval		*val, *val2, val_xlated;
	mname_entry	gvname;
	uint4		max_key;
	gd_addr		*tmpgd;
	gvnh_reg_t	*gvnh_reg;
	gd_region	*reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	val2 = va_arg(var, mval *);
	MV_FORCE_STR(val1);
	val1 = gtm_env_translate(val1, val2, &val_xlated);
	assert(!TREF(gv_extname_size) || (NULL != extnam_str.addr));
	if (val1->str.len)
	{
		tmp_mstr_ptr = &val1->str;
		tmpgd = zgbldir(val1);
	} else
	{
		/* Null external reference, ensure that gd_header is not NULL */
		if (!gd_header)
			gvinit();
		tmp_mstr_ptr = &dollar_zgbldir.str;
		tmpgd = gd_header;
	}
	extnam_str.len = tmp_mstr_ptr->len;
	if (extnam_str.len > TREF(gv_extname_size))
	{
		if (NULL != extnam_str.addr)
			free(extnam_str.addr);
		TREF(gv_extname_size) = extnam_str.len;
		extnam_str.addr = (char *)malloc(TREF(gv_extname_size));
	}
	memcpy(extnam_str.addr, tmp_mstr_ptr->addr, tmp_mstr_ptr->len);
	assert((NULL == gv_target) || (INVALID_GV_TARGET != gv_target));
	val = va_arg(var, mval *);
	assertpro(MV_IS_STRING(val));
	gvname.var_name = val->str;
	gvname.hash_code = hash_code;
	TREF(gd_targ_addr) = tmpgd;		/* needed by name-level $order/$zprevious and various other functions */
	GV_BIND_NAME_AND_ROOT_SEARCH(tmpgd, &gvname, gvnh_reg);
	/* cs_addrs is not initialized in case gvnh_reg->gvspan is non-NULL. Assert accordingly */
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC((NULL == gvnh_reg->gvspan) ? CHECK_CSA_TRUE : CHECK_CSA_FALSE);
	was_null = is_null = FALSE;
	/* gv_cur_region will not be set in case gvnh_reg->gvspan is non-NULL. So use region from gvnh_reg */
	reg = gvnh_reg->gd_reg;
	for (count -= 4;  count > 0;  count--)
	{
		val = va_arg(var, mval *);
		COPY_SUBS_TO_GVCURRKEY(val, reg, gv_currkey, was_null, is_null);
			/* updates gv_currkey, was_null, is_null */
	}
	/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH (e.g. setting gv_cur_region for spanning globals) */
	GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, tmpgd, gv_currkey, reg);
	/* Now that "gv_cur_region" is setup correctly for both spanning and non-spanning globals, do GVSUBOFLOW check */
	max_key = gv_cur_region->max_key_size;
	if (gv_currkey->end >= max_key)
		ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
	TREF(gv_some_subsc_null) = was_null; /* if true, it indicates there is a null subscript (other than the last subscript)
						in current key */
	TREF(gv_last_subsc_null) = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && (NEVER == reg->null_subs))
		sgnl_gvnulsubsc();
	return;
}
