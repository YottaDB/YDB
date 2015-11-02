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
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "sgnl.h"
#include "mvalconv.h"
#include "tp_set_sgm.h"

GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_binding	*gd_map;
GBLREF sgm_info		*first_sgm_info;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF uint4		dollar_tlevel;
GBLREF mstr		extnam_str;

#ifdef DEBUG
GBLDEF	boolean_t	dbg_opgvname_fast_path;
#endif

void op_gvname(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...)
{
	int		count;
	bool		was_null, is_null;
	boolean_t	bgormm;
	mval		*val;
	short int	max_key;
	va_list		var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	extnam_str.len = 0;
	if (!gd_header)
		gvinit();
	TREF(gd_targ_addr) = gd_header;
	VAR_START(var, val_arg);
	VMS_ONLY(va_count(count));
	UNIX_ONLY(count = count_arg);	/* need to preserve stack copy for i386 */
	count--;
	val = val_arg;
	if ((gd_header->maps == gd_map) && gv_currkey && (0 == gv_currkey->base[val->str.len]) &&
			(0 == memcmp(gv_currkey->base, val->str.addr, val->str.len)))
	{
		DEBUG_ONLY(dbg_opgvname_fast_path = TRUE);
		gv_currkey->end = val->str.len + 1;
		gv_currkey->base[gv_currkey->end] = 0;
		gv_currkey->prev = 0;
		bgormm = ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth));
		if (bgormm)
		{
			if (dollar_tlevel)
				tp_set_sgm();
			assert(INVALID_GV_TARGET != gv_target);
			GVCST_ROOT_SEARCH;
		}
		/* IF gv_target and cs_addrs are out of sync at this point, we could end up in database damage.
		 * Hence the assertpro. In the else block, we do a GV_BIND_NAME which ensures they are in sync
		 * (asserted by the DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC check later) hence this assertpro check
		 * is not done in that case.
		 */
		assertpro(gv_target->gd_csa == cs_addrs);
	} else
	{
		DEBUG_ONLY(dbg_opgvname_fast_path = FALSE);
		GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &(val->str));
		DEBUG_ONLY(
			bgormm = ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth)));
	}
	assert(!bgormm || (gv_cur_region && &FILE_INFO(gv_cur_region)->s_addrs == cs_addrs && cs_addrs->hdr == cs_data));
	assert(bgormm || !dollar_tlevel);
	assert(!dollar_tlevel || sgm_info_ptr && (sgm_info_ptr->tp_csa == cs_addrs));
	assert(NULL != gv_target);
	assert(gv_currkey->end);
	assert('\0' != gv_currkey->base[0]);	/* ensure the below DBG_CHECK_... assert does a proper check (which it wont
						 * if it finds gv_currkey->base[0] is '\0'
						 */
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	assert(TREF(gd_targ_addr) == gd_header);
	was_null = is_null = FALSE;
	max_key = gv_cur_region->max_key_size;
	for ( ; count > 0; count--)
	{
		val = va_arg(var, mval *);
		COPY_SUBS_TO_GVCURRKEY(val, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey, was_null, is_null */
	}
	va_end(var);
	TREF(gv_some_subsc_null) = was_null; /* if true, it indicates there is a null subscript (other than the last subscript)
						in current key */
	TREF(gv_last_subsc_null) = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();
	TREF(prev_gv_target) = gv_target;	/* note down gv_target in another global for debugging purposes (C9I09-003039) */
	return;
}
