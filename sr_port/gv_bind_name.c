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
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "collseq.h"
#include "gdsfhead.h"
#include "hashdef.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "gvcst_root_search.h"
#include "change_reg.h"
#include "targ_alloc.h"

GBLREF gv_namehead	*gv_target;
GBLREF short            dollar_trestart;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_binding	*gd_map, *gd_map_top;

void gv_bind_name(gd_addr *addr, mstr *targ)
{
	char		stashed;
	unsigned char	*cptr, *c_top, *in, *in_top;
	int		targlen;
	gd_binding	*map;
	ht_entry	*ht_ptr;
	mname		lcl_name;

	gd_map = addr->maps;
	gd_map_top = gd_map + addr->n_maps;
	targlen = targ->len < sizeof(mident) ? targ->len : sizeof(mident);
	for (cptr = (unsigned char *)&lcl_name, c_top = cptr + sizeof(lcl_name),
		in = (unsigned char *)targ->addr, in_top = in + targlen ; in < in_top ;)
			*cptr++ = *in++;
	while (cptr < c_top)
		*cptr++ = 0;
	ht_ptr = ht_put(addr->tab_ptr, &lcl_name, &stashed);
	if (!stashed && ht_ptr->ptr)
	{
		gv_target = (gv_namehead *)ht_ptr->ptr;
		if (!gv_target->gd_reg->open)
		{
			gv_target->clue.end = 0;
			gv_init_reg(gv_target->gd_reg);
		}
		gv_cur_region = gv_target->gd_reg;
		if (dollar_trestart)
			gv_target->clue.end = 0;
	} else
	{
		map = gd_map + 1;	/* get past local locks */
		for (; memcmp(&lcl_name, &(map->name[0]), sizeof(mident)) >= 0; map++)
			assert(map < gd_map_top);
		if (!map->reg.addr->open)
			gv_init_reg(map->reg.addr);
		gv_cur_region = map->reg.addr;
		if ((dba_cm == gv_cur_region->dyn.addr->acc_meth) || (dba_usr == gv_cur_region->dyn.addr->acc_meth))
		{
			ht_ptr->ptr = (char *)malloc(sizeof(gv_namehead));
			gv_target = (gv_namehead *)ht_ptr->ptr;
			gv_target->gd_reg = gv_cur_region;
			gv_target->nct = 0;
			gv_target->collseq = NULL;
		} else
		{
			assert(gv_cur_region->max_key_size <= MAX_KEY_SZ);
			gv_target = (gv_namehead *)targ_alloc(gv_cur_region->max_key_size);
			gv_target->gd_reg = gv_cur_region;
			ht_ptr->ptr = (char *)gv_target;
			memcpy(&gv_target->gvname, &lcl_name, sizeof(mident));
		}
	}
	change_reg();
	for (cptr = (unsigned char *)gv_currkey->base, in = (unsigned char *)targ->addr, in_top = in + targlen ; in < in_top ;)
		*cptr++ = *in++;
	gv_currkey->end = targlen + 1;
	gv_currkey->prev = 0;
	*cptr++ = 0;
	*cptr = 0;
	if ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth))
	{
		if ((0 == gv_target->root) || (DIR_ROOT == gv_target->root))
			gvcst_root_search();

	}
	return;
}
