/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"		/* needed for "jnl.h" and others */
#include "cmidef.h"
#include "hashtab_mname.h"
#include "cmmdef.h"
#include "gtcm_bind_name.h"
#include "gv_xform_key.h"
#include "targ_alloc.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "gvnh_spanreg.h"
#include "gtmimagename.h"
#include "gv_trigger_common.h"	/* for *HASHT* macros used inside GVNH_REG_INIT macro */
#include "jnl.h"		/* needed for "jgbl" */
#include "toktyp.h"		/* needed for "valid_mname.h" */
#include "valid_mname.h"

#define DIR_ROOT 1

GBLREF gv_key			*gv_currkey;
GBLREF sgmnt_data		*cs_data;
GBLREF gv_namehead		*gv_target;
GBLREF connection_struct	*curr_entry;

error_def(ERR_BADGTMNETMSG);

void gtcm_bind_name(cm_region_head *rh, boolean_t xform)
{
	ht_ent_mname	*tabent;
	mname_entry	 gvent;
	gvnh_reg_t	*gvnh_reg;

	ASSERT_IS_LIBGNPSERVER;
	GTCM_CHANGE_REG(rh);	/* sets the global variables gv_cur_region/cs_addrs/cs_data appropriately */
	gvent.var_name.addr = (char *)gv_currkey->base;
	gvent.var_name.len = STRLEN((char *)gv_currkey->base);
	/* The key this name comes from was assembled out of a client message, so the name has to be checked rather than
	 * asserted. "valid_mname()" is the same test every other way into the database applies: the M compiler for
	 * source, "op_indglvn()" for indirection and VALIDATE_VARNAME() for the Simple API. Without it a client can
	 * create a global whose name is not an M name at all, which MUPIP INTEG reports as DBBADKYNM, "Bad key name",
	 * and which M code cannot then reference. It also covers the two length cases: a key beginning with a <NUL>
	 * gives a length of 0, and one whose name runs past MAX_MIDENT_LEN gives a length too large to hash and look
	 * up.
	 *
	 * This is the point at which those bytes are used as a name, and so the only place the check belongs. Putting
	 * it in gtcmtr_get_key() instead would reject a name level $ORDER, whose key legitimately carries the 1 that
	 * GVKEY_INCREMENT_ORDER() writes over the <NUL> ending the name, making a name of exactly MAX_MIDENT_LEN
	 * measure one too long.
	 */
	if (!valid_mname(&gvent.var_name))
		CM_BADMSG(curr_entry);
	COMPUTE_HASH_MNAME(&gvent);
	if (NULL != (tabent = lookup_hashtab_mname(rh->reg_hash, &gvent)))	/* WARNING ASSIGNMENT */
	{
		gvnh_reg = (gvnh_reg_t *)tabent->value;
		assert(NULL != gvnh_reg);
		gv_target = gvnh_reg->gvt;
	} else
	{
		assert(IS_REG_BG_OR_MM(rh->reg));
		gv_target = targ_alloc(cs_data->max_key_size, &gvent, rh->reg);
		GVNH_REG_INIT(NULL, rh->reg_hash, NULL, gv_target, rh->reg, gvnh_reg, tabent);
	}
	GVCST_ROOT_SEARCH;
	if ((gv_target->collseq || gv_target->nct) && xform)
		gv_xform_key(gv_currkey, FALSE);
	return;
}
