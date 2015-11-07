/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "collseq.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_mname.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "change_reg.h"
#include "targ_alloc.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "min_max.h"
#include "gtmimagename.h"
#include "gvnh_spanreg.h"
#include "gv_trigger_common.h"	/* for *HASHT* macros used inside GVNH_REG_INIT macro */

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;

error_def(ERR_KEY2BIG);
error_def(ERR_GVIS);

/* Map an unsubscripted global name to its corresponding region in the gld file */
gvnh_reg_t *gv_bind_name(gd_addr *addr, mname_entry *gvname)
{
	gd_binding		*map;
	ht_ent_mname		*tabent;
	gd_region		*reg;
	gvnh_reg_t		*gvnh_reg;
	int			keylen;
	char			format_key[MAX_MIDENT_LEN + 1];	/* max key length + 1 byte for '^' */
	gv_namehead		*tmp_gvt;
	sgmnt_addrs		*csa;

	assert(MAX_MIDENT_LEN >= gvname->var_name.len);
	if (NULL != (tabent = lookup_hashtab_mname((hash_table_mname *)addr->tab_ptr, gvname)))
	{
		gvnh_reg = (gvnh_reg_t *)tabent->value;
		assert(NULL != gvnh_reg);
		reg = gvnh_reg->gd_reg;
		if (!reg->open)
		{
			gv_init_reg(reg);	/* could modify gvnh_reg->gvt if multiple regions map to same db file */
			assert(0 == gvnh_reg->gvt->clue.end);
		}
		tmp_gvt = gvnh_reg->gvt;
	} else
	{
		map = gv_srch_map(addr, gvname->var_name.addr, gvname->var_name.len);
		reg = map->reg.addr;
		if (!reg->open)
			gv_init_reg(reg);
		tmp_gvt = targ_alloc(reg->max_key_size, gvname, reg);
		GVNH_REG_INIT(addr, addr->tab_ptr, map, tmp_gvt, reg, gvnh_reg, tabent);
	}
	if (((keylen = gvname->var_name.len) + 2) > reg->max_key_size)	/* caution: embedded assignment of "keylen" */
	{
		assert(ARRAYSIZE(format_key) >= (1 + gvname->var_name.len));
		format_key[0] = '^';
		memcpy(&format_key[1], gvname->var_name.addr, gvname->var_name.len);
		csa = &FILE_INFO(reg)->s_addrs;
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(10) ERR_KEY2BIG, 4, keylen + 2, (int4)reg->max_key_size,
			REG_LEN_STR(reg), ERR_GVIS, 2, 1 + gvname->var_name.len, format_key);
	}
	gv_target = tmp_gvt;	/* now that any rts_error possibilities are all past us, it is safe to set gv_target.
				 * Setting it before could casue gv_target and gv_currkey to get out of sync in case of
				 * an error condition and fail asserts in mdb_condition_handler (for example).
				 */
	memcpy(gv_currkey->base, gvname->var_name.addr, keylen);
	gv_currkey->base[keylen] = KEY_DELIMITER;
	keylen++;
	gv_currkey->base[keylen] = KEY_DELIMITER;
	gv_currkey->end = keylen;
	gv_currkey->prev = 0;
	if (NULL == gvnh_reg->gvspan)
	{	/* Global does not span multiple regions. In that case, open the only region that this global maps to right here.
		 * In case of spanning globals, the subscripted reference will be used to find the mapping region (potentially
		 * different from "reg" computed here. And that is the region to do a "change_reg" on. Will be done later
		 * in "gv_bind_subsname".
		 */
		gv_cur_region = reg;
		change_reg();
	}
	return gvnh_reg;
}
