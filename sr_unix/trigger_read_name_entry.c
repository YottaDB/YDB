/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER

#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"
#include "gvcst_protos.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_read_name_entry.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "targ_alloc.h"			/* for SETUP_TRIGGER_GLOBAL & SWITCH_TO_DEFAULT_REGION */
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"				/* for sgm_info */

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	sgm_info		*sgm_info_ptr;

LITREF	mval			literal_hasht;

boolean_t trigger_read_name_entry(mident *trig_name, mval *val)
{
	sgmnt_addrs		*csa;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Trigger name should end with # -- so assert it */
	assert(TRIGNAME_SEQ_DELIM == trig_name->addr[trig_name->len - 1]);
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 == gv_target->root)
	{
		RESTORE_TRIGGER_REGION_INFO;
		return FALSE;
	}
	BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name->addr, trig_name->len - 1);
	status = gvcst_get(val);
	RESTORE_TRIGGER_REGION_INFO;
	return status;
}
#endif
