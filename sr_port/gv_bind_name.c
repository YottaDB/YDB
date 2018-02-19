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

#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gtmio.h"
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
#include "io.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;

error_def(ERR_KEY2BIG);
error_def(ERR_GVIS);

/* Map an unsubscripted global name to its corresponding region in the gld file */
gvnh_reg_t *gv_bind_name(gd_addr *addr, mname_entry *gvname)
{
	gd_binding		*map;
	ht_ent_mname		*tabent, *tabent1;
	gd_region		*reg;
	gvnh_reg_t		*gvnh_reg;
	int			keylen, count;
	char			format_key[MAX_MIDENT_LEN + 1];	/* max key length + 1 byte for '^' */
	gv_namehead		*tmp_gvt;
	sgmnt_addrs		*csa;
	hash_table_mname	*tab_ptr;

	assert(MAX_MIDENT_LEN >= gvname->var_name.len);
	tab_ptr = addr->tab_ptr;
	if (NULL == (tabent = lookup_hashtab_mname((hash_table_mname *)tab_ptr, gvname)))
	{
		count = tab_ptr->count;	/* Note down current # of valid entries in hash table */
		map = gv_srch_map(addr, gvname->var_name.addr, gvname->var_name.len, SKIP_BASEDB_OPEN_FALSE);
		reg = map->reg.addr;
		if (!reg->open)
			gv_init_reg(reg, addr);
		if (IS_STATSDB_REG(reg))
		{	/* In case of a statsDB, it is possible that "gv_srch_map" or "gv_init_reg" calls above end up doing
			 * a "op_gvname/gv_bind_name" if they in turn invoke "gvcst_init_statsDB". In that case, the hash table
			 * could have been updated since we did the "lookup_hashtab_mname" call above. So redo the lookup.
			 */
			tabent = lookup_hashtab_mname((hash_table_mname *)tab_ptr, gvname);
		} else
		{	/* If not a statsDB, then the above calls to "gv_srch_map" or "gv_init_reg" should not have changed
			 * the hashtable status of "gvname". There is an exception in that if gvname is "%YGS" (STATSDB_GBLNAME),
			 * then it is possible that the open of the statsDB failed (e.g. gtm_statsdir env var too long etc.) in
			 * which case the gvname would have been dynamically remapped to the baseDB. Assert that.
			 * Since we want to avoid a memcmp against STATSDB_GBLNAME, we check if the hashtable count has changed
			 * since we noted it down at function entry and if so redo the lookup hashtab in pro. Since only additions
			 * happen in this particular hashtable, it is enough to check for "count < tab_ptr->count".
			 */
#			ifdef DEBUG
			tabent1 = lookup_hashtab_mname((hash_table_mname *)tab_ptr, gvname);
			assert((tabent1 == tabent)
				|| ((gvname->var_name.len == STATSDB_GBLNAME_LEN)
					&& (0 == memcmp(gvname->var_name.addr, STATSDB_GBLNAME, STATSDB_GBLNAME_LEN))
					&& (count < tab_ptr->count)));
#			endif
			if (count < tab_ptr->count)
				tabent = lookup_hashtab_mname((hash_table_mname *)tab_ptr, gvname);
		}
	}
	if (NULL == tabent)
	{
		tmp_gvt = targ_alloc(reg->max_key_size, gvname, reg);
		GVNH_REG_INIT(addr, tab_ptr, map, tmp_gvt, reg, gvnh_reg, tabent);
	} else
	{
		gvnh_reg = (gvnh_reg_t *)tabent->value;
		assert(NULL != gvnh_reg);
		reg = gvnh_reg->gd_reg;
		if (!reg->open)
		{
			gv_init_reg(reg, addr);	/* could modify gvnh_reg->gvt if multiple regions map to same db file */
			assert((0 == gvnh_reg->gvt->clue.end) || IS_STATSDB_REG(reg)); /* A statsDB open writes to itself */
		}
		tmp_gvt = gvnh_reg->gvt;
	}
	if (((keylen = gvname->var_name.len) + 2) > reg->max_key_size)	/* caution: embedded assignment of "keylen" */
	{
		assert(ARRAYSIZE(format_key) >= (1 + gvname->var_name.len));
		format_key[0] = '^';
		memcpy(&format_key[1], gvname->var_name.addr, gvname->var_name.len);
		csa = &FILE_INFO(reg)->s_addrs;
		gv_currkey->end = 0;
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
