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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"

GBLDEF gv_key		*gv_currkey;
GBLDEF gv_key		*gv_altkey;
GBLDEF int4		gv_keysize;
GBLDEF gd_addr		*gd_header=0;
GBLDEF gd_binding	*gd_map;
GBLDEF gd_binding	*gd_map_top;

GBLREF gd_addr		*gd_targ_addr;

void gvinit(void)
{
	mval	v;
	int4	keysize;
	gv_key	*tmp_currkey, *tmp_altkey;

	if (!gd_header)
	{
		v.mvtype = MV_STR;
		v.str.len = 0;
		gd_header = zgbldir(&v);
		gd_map = gd_header->maps;
		gd_map_top = gd_map + gd_header->n_maps;
		gd_targ_addr = gd_header;
	}

	/* May get in here after an extended ref call, so you don't want to
		lose any preexisting keys */

	keysize = (gd_header->regions->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4);
	assert(keysize);
	if (keysize > gv_keysize)
	{
		tmp_currkey = gv_currkey;
		tmp_altkey = gv_altkey;
		gv_altkey = (gv_key*)malloc(sizeof(gv_key) - 1 + keysize);
		gv_currkey = (gv_key*)malloc(sizeof(gv_key) - 1 + keysize);
		gv_keysize = keysize;
		gv_currkey->top = gv_altkey->top = gv_keysize;
		if (tmp_currkey)
		{
			assert(tmp_altkey);
			free(tmp_currkey);
			free(tmp_altkey);
		}
	}
	else
		assert(gv_currkey && gv_altkey);

	gv_currkey->end = gv_currkey->prev = gv_altkey->end = gv_altkey->prev = 0;
	gv_altkey->base[0] = gv_currkey->base[0] = '\0';
}
