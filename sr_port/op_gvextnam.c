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
GBLREF gd_region	*gv_cur_region;
GBLREF mstr             extnam_str;
GBLREF mval		dollar_zgbldir;
GBLREF gd_addr		*gd_header;

void op_gvextnam(UNIX_ONLY_COMMA(int4 count) mval *val1, ...)
{
	va_list		var;
	VMS_ONLY(int4	count;)
	bool		was_null, is_null;
	mstr		*tmp_mstr_ptr;
	mval		*val, *val2, val_xlated;
	short		max_key;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, val1);
	VMS_ONLY(va_count(count));
	val2 = va_arg(var, mval *);
	MV_FORCE_STR(val1);
	val1 = gtm_env_translate(val1, val2, &val_xlated);
	assert(!TREF(gv_extname_size) || (NULL != extnam_str.addr));
	if (!gd_header)
		gvinit();
	if (val1->str.len)
	{
		tmp_mstr_ptr = &val1->str;
		TREF(gd_targ_addr) = zgbldir(val1);
	} else
	{
		tmp_mstr_ptr = &dollar_zgbldir.str;
		TREF(gd_targ_addr) = gd_header;
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
	if (!MV_IS_STRING(val))
		GTMASSERT;
	GV_BIND_NAME_AND_ROOT_SEARCH(TREF(gd_targ_addr), &(val->str));
	was_null = is_null = FALSE;
	max_key = gv_cur_region->max_key_size;
	for (count -= 3;  count > 0;  count--)
	{
		val = va_arg(var, mval *);
		COPY_SUBS_TO_GVCURRKEY(val, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey, was_null, is_null */
	}
	va_end(var);
	TREF(gv_some_subsc_null) = was_null; /* if true, it indicates there is a null subscript (other than the last subscript)
						in current key */
	TREF(gv_last_subsc_null) = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && (NEVER == gv_cur_region->null_subs))
		sgnl_gvnulsubsc();
	return;
}
