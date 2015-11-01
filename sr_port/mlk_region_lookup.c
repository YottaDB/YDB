/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "hashdef.h"
#include "min_max.h"
#include "mlk_region_lookup.h"
#include "targ_alloc.h"

#define DIR_ROOT 1

gd_region *mlk_region_lookup(mval *ptr, gd_addr *addr)
{
	ht_entry		*h;
	char			stashed;
	gd_binding		*map;
	mname			lcl_name;
	register char		*c, *c_top, *p;
	register unsigned char	len;
	gv_namehead		*targ;
	gd_region		*reg;

	p = ptr->str.addr;
	if (*p != '^')				/* is local lock */
	{
		reg = addr->maps->reg.addr; 	/* local lock map is first */
		if (!reg->open)
			gv_init_reg (reg);
	} else
	{
		c = (char *)&lcl_name;
		c_top = c + sizeof(lcl_name);
		p++;
		for (len = MIN(ptr->str.len - 1, sizeof(lcl_name)); len; len--)
			*c++ = *p++;
		while (c < c_top)
			*c++ = 0;
		h = ht_put (addr->tab_ptr, &lcl_name, &stashed);
		if (!stashed && h->ptr)
		{
			reg = ((gv_namehead *)h->ptr)->gd_reg;
			if (!reg->open)
				gv_init_reg(reg);
		} else
		{
			map = addr->maps + 1;	/* get past local locks */
			for ( ; memcmp(&lcl_name, &(map->name[0]), sizeof(mident)) >= 0; map++)
				assert (map < addr->maps + addr->n_maps);

			reg = map->reg.addr;
			if (!reg->open)
				gv_init_reg (reg);
			if (reg->dyn.addr->acc_meth == dba_cm || reg->dyn.addr->acc_meth == dba_usr)
			{
				h->ptr = (char *) malloc(sizeof(gv_namehead));
				targ = (gv_namehead *)h->ptr;
				targ->gd_reg = reg;
				targ->nct = 0;
				targ->collseq = NULL;
			}
			else
			{
				targ = (gv_namehead *)targ_alloc(reg->max_key_size);
				targ->gd_reg = reg;
				h->ptr = (char *)targ;
				memcpy(&targ->gvname, &lcl_name, sizeof(mident));
			}
		}
	}

	return reg;
}
