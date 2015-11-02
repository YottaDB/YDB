/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "min_max.h"
#include "mlk_region_lookup.h"
#include "targ_alloc.h"
#include "hashtab_mname.h"

#define DIR_ROOT 1

gd_region *mlk_region_lookup(mval *ptr, gd_addr *addr)
{
	ht_ent_mname		*tabent;
	mname_entry		 gvent;
	gd_binding		*map;
	gv_namehead		*targ;
	gd_region		*reg;
	register char		*p;
	int			res, plen;
	boolean_t		added;
	enum db_acc_method	acc_meth;
	gvnh_reg_t		*gvnh_reg;

	p = ptr->str.addr;
	plen = ptr->str.len;
	if (*p != '^')				/* is local lock */
	{
		reg = addr->maps->reg.addr; 	/* local lock map is first */
		if (!reg->open)
			gv_init_reg (reg);
	} else
	{
		p++;
		plen--;
		gvent.var_name.addr = p;
		gvent.var_name.len = MIN(plen, MAX_MIDENT_LEN);
		COMPUTE_HASH_MNAME(&gvent);
		if ((NULL != (tabent = lookup_hashtab_mname(addr->tab_ptr, &gvent)))
			&& (NULL != (gvnh_reg = (gvnh_reg_t *)tabent->value)))
		{
			targ = gvnh_reg->gvt;
			reg = gvnh_reg->gd_reg;
			if (!reg->open)
			{
				targ->clue.end = 0;
				gv_init_reg(reg);
			}
		} else
		{
			map = addr->maps + 1;	/* get past local locks */
			for (; (res = memcmp(gvent.var_name.addr, &(map->name[0]), gvent.var_name.len)) >= 0; map++)
			{
				assert (map < addr->maps + addr->n_maps);
				if (0 == res && 0 != map->name[gvent.var_name.len])
					break;
			}
			reg = map->reg.addr;
			if (!reg->open)
				gv_init_reg (reg);
			acc_meth = reg->dyn.addr->acc_meth;
			if ((dba_cm == acc_meth) || (dba_usr == acc_meth))
			{
				targ = malloc(SIZEOF(gv_namehead) + gvent.var_name.len);
				memset(targ, 0, SIZEOF(gv_namehead) + gvent.var_name.len);
				targ->gvname.var_name.addr = (char *)targ + SIZEOF(gv_namehead);
				targ->nct = 0;
				targ->collseq = NULL;
				targ->regcnt = 1;
				memcpy(targ->gvname.var_name.addr, gvent.var_name.addr, gvent.var_name.len);
				targ->gvname.var_name.len = gvent.var_name.len;
				targ->gvname.hash_code = gvent.hash_code;
			} else
				targ = (gv_namehead *)targ_alloc(reg->max_key_size, &gvent, reg);
			gvnh_reg = (gvnh_reg_t *)malloc(SIZEOF(gvnh_reg_t));
			gvnh_reg->gvt = targ;
			gvnh_reg->gd_reg = reg;
			if (NULL != tabent)
			{	/* Since the global name was found but gv_target was null and now we created a new targ,
				 * the hash table key must point to the newly created targ->gvname. */
				tabent->key = targ->gvname;
				tabent->value = (char *)gvnh_reg;
			} else
			{
				added = add_hashtab_mname((hash_table_mname *)addr->tab_ptr, &targ->gvname, gvnh_reg, &tabent);
				assert(added);
			}
		}
	}
	return reg;
}
