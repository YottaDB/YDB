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

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_binding	*gd_map, *gd_map_top;

error_def(ERR_KEY2BIG);
error_def(ERR_GVIS);

void gv_bind_name(gd_addr *addr, mstr *targ)
{
	gd_binding		*map;
	ht_ent_mname		*tabent;
	mname_entry		 gvent;
	int			res;
	boolean_t		added;
	enum db_acc_method	acc_meth;
	gd_region		*reg;
	gvnh_reg_t		*gvnh_reg;
	int			keylen;
	char			format_key[MAX_MIDENT_LEN + 1];	/* max key length + 1 byte for '^' */
	gv_namehead		*tmp_gvt;
	sgmnt_addrs		*csa;

	gd_map = addr->maps;
	gd_map_top = gd_map + addr->n_maps;
	gvent.var_name.addr = targ->addr;
	gvent.var_name.len = MIN(targ->len, MAX_MIDENT_LEN);
	COMPUTE_HASH_MNAME(&gvent);
	if ((NULL != (tabent = lookup_hashtab_mname((hash_table_mname *)addr->tab_ptr, &gvent)))
		&& (NULL != (gvnh_reg = (gvnh_reg_t *)tabent->value)))
	{
		reg = gvnh_reg->gd_reg;
		if (!reg->open)
		{
			gv_init_reg(reg);	/* could modify gvnh_reg->gvt if multiple regions map to same db file */
			assert(0 == gvnh_reg->gvt->clue.end);
		}
		gv_target = gvnh_reg->gvt;
		gv_cur_region = reg;
		acc_meth = gv_cur_region->dyn.addr->acc_meth;
	} else
	{
		map = gd_map + 1;	/* get past local locks */
		for (; (res = memcmp(gvent.var_name.addr, &(map->name[0]), gvent.var_name.len)) >= 0; map++)
		{
			assert(map < gd_map_top);
			if (0 == res && 0 != map->name[gvent.var_name.len])
				break;
		}
		if (!map->reg.addr->open)
			gv_init_reg(map->reg.addr);
		gv_cur_region = map->reg.addr;
		acc_meth = gv_cur_region->dyn.addr->acc_meth;
		if ((dba_cm == acc_meth) || (dba_usr == acc_meth))
		{
			tmp_gvt = malloc(SIZEOF(gv_namehead) + gvent.var_name.len);
			memset(tmp_gvt, 0, SIZEOF(gv_namehead) + gvent.var_name.len);
			tmp_gvt->gvname.var_name.addr = (char *)tmp_gvt + SIZEOF(gv_namehead);
			tmp_gvt->nct = 0;
			tmp_gvt->collseq = NULL;
			tmp_gvt->regcnt = 1;
			memcpy(tmp_gvt->gvname.var_name.addr, gvent.var_name.addr, gvent.var_name.len);
			tmp_gvt->gvname.var_name.len = gvent.var_name.len;
			tmp_gvt->gvname.hash_code = gvent.hash_code;
		} else
		{
			assert(gv_cur_region->max_key_size <= MAX_KEY_SZ);
			tmp_gvt = (gv_namehead *)targ_alloc(gv_cur_region->max_key_size, &gvent, gv_cur_region);
		}
		gvnh_reg = (gvnh_reg_t *)malloc(SIZEOF(gvnh_reg_t));
		gvnh_reg->gvt = tmp_gvt;
		gvnh_reg->gd_reg = gv_cur_region;
		if (NULL != tabent)
		{	/* Since the global name was found but gv_target was null and now we created a new gv_target,
			 * the hash table key must point to the newly created gv_target->gvname. */
			tabent->key = tmp_gvt->gvname;
			tabent->value = (char *)gvnh_reg;
		} else
		{
			added = add_hashtab_mname((hash_table_mname *)addr->tab_ptr, &tmp_gvt->gvname, gvnh_reg, &tabent);
			assert(added);
		}
		gv_target = tmp_gvt;	/* now that any error possibilities (out-of-memory issues in malloc/add_hashtab_mname)
					 * are all done, it is safe to set gv_target. Setting it before could casue gv_target
					 * and gv_currkey to get out of sync in case of an error condition.
					 */
	}
	if ((keylen = gvent.var_name.len + 2) > gv_cur_region->max_key_size)	/* caution: embedded assignment of "keylen" */
	{
		assert(ARRAYSIZE(format_key) >= (1 + gvent.var_name.len));
		format_key[0] = '^';
		memcpy(&format_key[1], gvent.var_name.addr, gvent.var_name.len);
		csa = &FILE_INFO(gv_cur_region)->s_addrs;
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(10) ERR_KEY2BIG, 4, keylen, (int4)gv_cur_region->max_key_size,
			REG_LEN_STR(gv_cur_region), ERR_GVIS, 2, 1 + gvent.var_name.len, format_key);
	}
	memcpy(gv_currkey->base, gvent.var_name.addr, gvent.var_name.len);
	gv_currkey->base[gvent.var_name.len] = 0;
	gvent.var_name.len++;
	gv_currkey->base[gvent.var_name.len] = 0;
	gv_currkey->end = gvent.var_name.len;
	gv_currkey->prev = 0;
	change_reg();
	return;
}
