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

GBLREF uint4		dollar_tlevel;
GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF sgm_info         *first_sgm_info;
GBLREF mstr		extnam_str;

error_def(ERR_GVNAKED);
error_def(ERR_MAXNRSUBSCRIPTS);

void op_gvnaked(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...)
{
	va_list		var;
	int		count;
	bool		was_null, is_null, sbs_cnt;
	mval		*val;
	short 		max_key;
	unsigned char	*ptr, *end_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	extnam_str.len = 0;
	if (!gv_currkey || (0 == gv_currkey->prev) || (0 == gv_currkey->end))
		rts_error(VARLSTCNT(1) ERR_GVNAKED);
	if ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth))
	{
		assert(INVALID_GV_TARGET != gv_target);
                if (dollar_tlevel)
			tp_set_sgm();
		GVCST_ROOT_SEARCH;
		assert(gv_target->gd_csa == cs_addrs);
	}
	VMS_ONLY(va_count(count));
	UNIX_ONLY(count = count_arg;)	/* i386 assembler modules may depend on unchanged count */
	if (0 >= count)
		GTMASSERT;
	is_null = FALSE;
	assert(gv_currkey->prev);
	was_null = TREF(gv_some_subsc_null);
	sbs_cnt = 0;
	if (1 < count)
	{
		/* Use of naked reference can cause increase in number of subscripts.   So count the subscripts */
		ptr = gv_currkey->base;
		end_ptr = ptr + gv_currkey->prev;
		while (ptr < end_ptr)
			if (KEY_DELIMITER == *ptr++)
				sbs_cnt++;
		if (MAX_GVSUBSCRIPTS < (count + sbs_cnt))
			rts_error(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	}
	/* else naked reference will not increase number of subscripts, so do not worry about exceeding the limit */
	gv_currkey->end = gv_currkey->prev;
	VAR_START(var, val_arg);
	val = val_arg;
	max_key = gv_cur_region->max_key_size;
	for ( ; ; )
	{
		COPY_SUBS_TO_GVCURRKEY(val, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey, was_null, is_null */
		if (0 < --count)
			val = va_arg(var, mval *);
		else
			break;
	}
	va_end(var);
	TREF(gv_some_subsc_null) = was_null; /* if true, it indicates there is a null subscript (other than the last subscript)
						in current key */
	TREF(gv_last_subsc_null) = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && (NEVER == gv_cur_region->null_subs))
		sgnl_gvnulsubsc();
	return;
}
