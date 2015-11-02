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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "min_max.h"
#include "mlk_region_lookup.h"
#include "targ_alloc.h"
#include "hashtab.h"
#include "hashtab_mname.h"

#define DIR_ROOT 1

gd_region *mlk_region_lookup(mval *ptr, gd_addr *addr)
{
	ht_ent_mname	*tabent;
	mname_entry	 gvent;
	gd_binding	*map;
	gv_namehead	*targ;
	gd_region	*reg;
	register char	*p;
	int		res, plen;
	boolean_t	added;

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
		if ((NULL != (tabent = lookup_hashtab_mname(addr->tab_ptr, &gvent))) &&
						(NULL != (targ = (gv_namehead *)tabent->value)))
		{
			reg = targ->gd_reg;
			if (!reg->open)
				gv_init_reg(reg);
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
			if (reg->dyn.addr->acc_meth == dba_cm || reg->dyn.addr->acc_meth == dba_usr)
			{
				targ = malloc(sizeof(gv_namehead) + gvent.var_name.len);
				memset(targ, 0, sizeof(gv_namehead) + gvent.var_name.len);
				targ->gvname.var_name.addr = (char *)targ + sizeof(gv_namehead);
				targ->nct = 0;
				targ->collseq = NULL;
				memcpy(targ->gvname.var_name.addr, gvent.var_name.addr, gvent.var_name.len);
				targ->gvname.var_name.len = gvent.var_name.len;
				targ->gvname.hash_code = gvent.hash_code;
			} else
				targ = (gv_namehead *)targ_alloc(reg->max_key_size, &gvent);
			targ->gd_reg = reg;
			if (NULL != tabent)
			{	/* Since the global name was found but gv_target was null and now we created a new targ,
				 * the hash table key must point to the newly created targ->gvname. */
				tabent->key = targ->gvname;
				tabent->value = targ;
			} else
			{
				added = add_hashtab_mname(addr->tab_ptr, &targ->gvname, targ, &tabent);
				assert(added);
			}
		}
	}
	return reg;
}
