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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmidef.h"
#include "cmmdef.h"

#define DIR_ROOT 1

GBLREF int4		gv_keysize;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF short 		gtcm_ast_avail;

void gtcmd_cst_init(cm_region_head *ptr)
{
	gv_namehead	*g;
	gv_key          *temp_key;
	void		gvcst_init();
	error_def(CMERR_CMEXCDASTLM);

	if (gtcm_ast_avail > 0)
		gvcst_init(ptr->reg);
	else
		rts_error(VARLSTCNT(1) CMERR_CMEXCDASTLM);
	gtcm_ast_avail--;
	if (((ptr->reg->max_rec_size + MAX_NUM_SUBSC_LEN + 4) & (-4)) > gv_keysize)
	{
		gv_keysize = (ptr->reg->max_rec_size + MAX_NUM_SUBSC_LEN + 4) & (-4);
		temp_key = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
		if (gv_currkey)
		{
			assert(gv_keysize > gv_currkey->end);
			memcpy(temp_key, gv_currkey, sizeof(gv_key) + gv_currkey->end);
			free(gv_currkey);
		} else
			temp_key->base[0] = '\0';
		gv_currkey = temp_key;
		gv_currkey->top = gv_keysize;
		temp_key = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
		if (gv_altkey)
		{
			assert(gv_keysize > gv_altkey->end);
			memcpy(temp_key, gv_altkey, sizeof(gv_key) + gv_altkey->end);
			free(gv_altkey);
		} else
			temp_key->base[0] = '\0';
		gv_altkey = temp_key;
		gv_altkey->top = gv_keysize;
	}
	FILE_INFO(ptr->reg)->s_addrs.dir_tree = (gv_namehead *)targ_alloc(ptr->reg->max_rec_size);
	FILE_INFO(ptr->reg)->s_addrs.dir_tree->root = DIR_ROOT;
	ht_init(ptr->reg_hash,0);
	cm_add_gdr_ptr(ptr->reg);
}
