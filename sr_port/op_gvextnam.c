/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#ifdef VMS
#include <descrip.h>	/* for GTM_ENV_TRANSLATE */
#endif

#include "gtm_limits.h"	/* for GTM_ENV_TRANSLATE */

#ifdef EARLY_VARARGS
#include <varargs.h>
#endif

#include "error.h"	/* for GTM_ENV_TRANSLATE */
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
#include "rtnhdr.h"
#include "mv_stent.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */

#ifndef EARLY_VARARGS
#include <varargs.h>	/* needs to be after the above include files otherwise linux/x86 gcc complains */
#endif

/*the header files below are for environment translation*/
#ifdef UNIX
#include "fgncalsp.h"
#endif
#include "gtm_env_xlate_init.h"

GBLREF bool		gv_curr_subsc_null;
GBLREF bool		gv_prev_subsc_null;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		transform;
GBLREF gd_addr		*gd_targ_addr;
GBLREF mstr             extnam_str;
GBLREF mval		dollar_zgbldir;
GBLREF gd_addr		*gd_header;

static int	extnam_str_alloc = 0;

void op_gvextnam(va_alist)
va_dcl
{
	va_list		var;
	int		count, len;
	bool		was_null, is_null;
	mstr		*tmp_mstr_ptr;
	mval		*val, *val1, *val2, val_xlated;
	short		max_key;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;

	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);

	VAR_START(var);
	count = va_arg(var,int4);
	val1 = va_arg(var, mval *);
	val2 = va_arg(var, mval *);
	MV_FORCE_STR(val1);
	GTM_ENV_TRANSLATE(val1, val2);

	assert(!extnam_str_alloc || (NULL != extnam_str.addr));
	if (val1->str.len)
	{
		tmp_mstr_ptr = &val1->str;
		gd_targ_addr = zgbldir(val1);
	} else
	{
		tmp_mstr_ptr = &dollar_zgbldir.str;
		if (!gd_header)
			gvinit();
		gd_targ_addr = gd_header;
	}
	extnam_str.len = tmp_mstr_ptr->len;
	if (extnam_str.len > extnam_str_alloc)
	{
		if (NULL != extnam_str.addr)
			free(extnam_str.addr);
		extnam_str_alloc = extnam_str.len;
		extnam_str.addr = (char *)malloc(extnam_str_alloc);
	}
	memcpy(extnam_str.addr, tmp_mstr_ptr->addr, tmp_mstr_ptr->len);
	if (gv_target)
	{
		assert(INVALID_GV_TARGET != gv_target);
		gv_target->clue.end = 0;
	}
	val = va_arg(var, mval *);
	if (!MV_IS_STRING(val))
		GTMASSERT;
	gv_bind_name(gd_targ_addr, &(val->str));
	was_null = is_null = FALSE;
	max_key = gv_cur_region->max_key_size;
	for (count -= 3;  count > 0;  count--)
	{
		COPY_SUBS_TO_GVCURRKEY(var, gv_currkey, was_null, is_null);	/* updates gv_currkey, was_null, is_null */
	}
	gv_prev_subsc_null = was_null; /* if true, it indicates there is a null subscript (except last subscript) in current key */
	gv_curr_subsc_null = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && (NEVER == gv_cur_region->null_subs))
		sgnl_gvnulsubsc();
	return;
}
