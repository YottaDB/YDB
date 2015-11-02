/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "cli.h"
#include "util.h"
#include "dse.h"

GBLREF short int	patch_path_count;
GBLREF block_id		ksrch_root;
GBLREF bool		patch_find_root_search;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;

#define MAX_UTIL_LEN	64

void dse_f_key(void)
{
	block_id	path[MAX_BT_DEPTH + 1], root_path[MAX_BT_DEPTH + 1];
	int4		offset[MAX_BT_DEPTH + 1], root_offset[MAX_BT_DEPTH + 1], nocrit_present;
	char		targ_key[MAX_KEY_SZ + 1], targ_key_root[MAX_KEY_SZ + 1], *key_top, util_buff[MAX_UTIL_LEN];
	int		size, size_root, root_path_count, count, util_len;
	boolean_t	found, was_crit, was_hold_onto_crit;

	if (!dse_getki(&targ_key[0], &size, LIT_AND_LEN("KEY")))
		return;
	patch_path_count = 1;
	root_path[0] = get_dir_root();
	for (key_top = &targ_key[0]; key_top < ARRAYTOP(targ_key); )
		if (!*key_top++)
			break;
	size_root = (int)(key_top - &targ_key[0] + 1);
	memcpy(&targ_key_root[0],&targ_key[0],size_root);
	targ_key_root[size_root - 1] = targ_key_root[size_root] = 0;
	patch_find_root_search = TRUE;
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (!dse_key_srch(root_path[0], &root_path[1], &root_offset[0], &targ_key_root[0], size_root))
	{
		util_out_print("!/Key not found, no root present.!/",TRUE);
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		return;
	}
	root_path_count = patch_path_count;
	patch_path_count = 1;
	path[0] = ksrch_root;
	patch_find_root_search = FALSE;
	if (!dse_key_srch(path[0], &path[1], &offset[0], &targ_key[0], size))
	{	memcpy(util_buff,"!/Key not found, would be in block  ",36);
		util_len = 36;
		util_len += i2hex_nofill(path[patch_path_count - 2], (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], ".",1);
		util_len += 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff,FALSE);
		patch_path_count -= 1;
	}else
	{	memcpy(util_buff,"!/Key found in block  ",22);
		util_len = 22;
		util_len += i2hex_nofill(path[patch_path_count - 1], (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], ".",1);
		util_len += 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff,FALSE);
	}
	util_out_print("!/    Directory path!/    Path--blk:off",TRUE);
	for (count = 0; count < root_path_count ;count++)
	{	memcpy(util_buff,"	",1);
		util_len = 1;
		util_len += i2hex_nofill(root_path[count],(uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len],":",1);
		util_len += 1;
		util_len += i2hex_nofill(root_offset[count],(uchar_ptr_t)&util_buff[util_len], 4);
		memcpy(&util_buff[util_len],",",1);
		util_len += 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff,FALSE);
	}
	util_out_print("!/    Global tree path!/    Path--blk:off",TRUE);
	if (patch_path_count)
	{	for (count = 0; count < patch_path_count ;count++)
		{	memcpy(util_buff,"	",1);
			util_len = 1;
			util_len += i2hex_nofill(path[count],(uchar_ptr_t)&util_buff[util_len], 8);
			memcpy(&util_buff[util_len],":",1);
			util_len += 1;
			util_len += i2hex_nofill(offset[count],(uchar_ptr_t)&util_buff[util_len], 4);
			memcpy(&util_buff[util_len],",",1);
			util_len += 1;
			util_buff[util_len] = 0;
			util_out_print(util_buff,FALSE);
		}
		util_out_print(0,TRUE);
	} else
	{	memcpy(util_buff,"	",1);
		util_len = 1;
		util_len += i2hex_nofill(root_path[count],(uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len],"!/",2);
		util_len += 2;
		util_buff[util_len] = 0;
		util_out_print(util_buff,TRUE);
	}
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return;
}
