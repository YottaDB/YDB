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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cli.h"
#include "util.h"
#include "dse.h"
#include "print_target.h"
#include "gv_trigger_common.h" /* for IS_GVKEY_HASHT_GBLNAME macro */

GBLREF short int	patch_path_count;
GBLREF block_id		ksrch_root;
GBLREF bool		patch_find_root_search;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*original_header;

#define MAX_UTIL_LEN	64

STATICFNDCL void print_reg_if_mismatch(char *key, int keylen);

/* If current region does not match region where TARG_KEY maps to, and if -nogbldir was not specified,
 * issue an info message indicating the mapping region.
 */
STATICFNDEF void print_reg_if_mismatch(char *key, int keylen)
{
	gd_binding		*map;
	gd_region		*reg;

	assert(3 <= keylen);
	assert(KEY_DELIMITER == key[keylen - 1]);
	assert(KEY_DELIMITER == key[keylen - 2]);
	assert(KEY_DELIMITER != key[keylen - 3]);
	/* If input key is ^#t, then do not call gv_srch_map as it does not belong to a particular region,
	 * i.e. the gld does not map globals beginning with #. There is no reg mismatch possible in that case.
	 * So skip this processing entirely.
	 */
	if (!IS_GVKEY_HASHT_GBLNAME(keylen -2, key))
	{
		map = gv_srch_map(original_header, key, keylen - 2, SKIP_BASEDB_OPEN_FALSE); /* -2 to remove two trailing 0s */
		reg = map->reg.addr;
		if (gv_cur_region != reg)
		{
			/* At this point, gv_target and gv_target->collseq are already setup (by the dse_getki call in caller).
			 * This is needed by the "print_target" --> "gvsub2str" call below.
			 */
			util_out_print("Key ^", FALSE);
			print_target((unsigned char *)key);
			util_out_print(" maps to Region !AD; Run \"find -region=!AD\" before looking for this node",
					TRUE, REG_LEN_STR(reg), REG_LEN_STR(reg));
		}
	}
}

void dse_f_key(void)
{
	block_id	path[MAX_BT_DEPTH + 1], root_path[MAX_BT_DEPTH + 1];
	boolean_t	found, nocrit_present, nogbldir_present, was_crit, was_hold_onto_crit;
	char		targ_key[MAX_KEY_SZ + 1], targ_key_root[MAX_KEY_SZ + 1], *key_top, util_buff[MAX_UTIL_LEN];
	int		size, size_root, root_path_count, count, util_len;
	int4		offset[MAX_BT_DEPTH + 1], root_offset[MAX_BT_DEPTH + 1];

	if (!dse_getki(&targ_key[0], &size, LIT_AND_LEN("KEY")))
		return;
	patch_path_count = 1;
	root_path[0] = get_dir_root();
	for (key_top = &targ_key[0]; key_top < ARRAYTOP(targ_key); )
		if (!*key_top++)
			break;
	size_root = (int)(key_top - &targ_key[0] + 1);
	memcpy(&targ_key_root[0], &targ_key[0], size_root);
	targ_key_root[size_root - 1] = targ_key_root[size_root] = 0;
	patch_find_root_search = TRUE;
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	/* -NOGBLDIR is currently supported only in Unix */
	UNIX_ONLY(nogbldir_present = (CLI_NEGATED == cli_present("GBLDIR"));)
	VMS_ONLY(nogbldir_present = TRUE;)
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (!dse_ksrch(root_path[0], &root_path[1], &root_offset[0], &targ_key_root[0], size_root))
	{
		util_out_print("!/Key not found, no root present.", TRUE);
		if (!nogbldir_present)
			print_reg_if_mismatch(&targ_key[0], size);
		util_out_print("", TRUE);
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		return;
	}
	root_path_count = patch_path_count;
	patch_path_count = 1;
	path[0] = ksrch_root;
	patch_find_root_search = FALSE;
	if (!dse_ksrch(path[0], &path[1], &offset[0], &targ_key[0], size))
	{	memcpy(util_buff, "!/Key not found, would be in block  ", 36);
		util_len = 36;
		util_len += i2hex_nofill(path[patch_path_count - 2], (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], ".", 1);
		util_len += 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff, FALSE);
		patch_path_count -= 1;
	} else
	{	memcpy(util_buff, "!/Key found in block  ", 22);
		util_len = 22;
		util_len += i2hex_nofill(path[patch_path_count - 1], (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], ".", 1);
		util_len += 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff, FALSE);
	}
	util_out_print("!/    Directory path!/    Path--blk:off", TRUE);
	for (count = 0; count < root_path_count ;count++)
	{	memcpy(util_buff, "	", 1);
		util_len = 1;
		util_len += i2hex_nofill(root_path[count], (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], ":", 1);
		util_len += 1;
		util_len += i2hex_nofill(root_offset[count], (uchar_ptr_t)&util_buff[util_len], 4);
		memcpy(&util_buff[util_len], ", ", 1);
		util_len += 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff, FALSE);
	}
	util_out_print("!/    Global tree path!/    Path--blk:off", TRUE);
	if (patch_path_count)
	{	for (count = 0; count < patch_path_count ;count++)
		{	memcpy(util_buff, "	", 1);
			util_len = 1;
			util_len += i2hex_nofill(path[count], (uchar_ptr_t)&util_buff[util_len], 8);
			memcpy(&util_buff[util_len], ":", 1);
			util_len += 1;
			util_len += i2hex_nofill(offset[count], (uchar_ptr_t)&util_buff[util_len], 4);
			memcpy(&util_buff[util_len], ", ", 1);
			util_len += 1;
			util_buff[util_len] = 0;
			util_out_print(util_buff, FALSE);
		}
		util_out_print(0, TRUE);
	} else
	{	memcpy(util_buff, "	", 1);
		util_len = 1;
		util_len += i2hex_nofill(root_path[count], (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], "!/", 2);
		util_len += 2;
		util_buff[util_len] = 0;
		util_out_print(util_buff, TRUE);
	}
	if (!nogbldir_present)
		print_reg_if_mismatch(&targ_key[0], size);
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return;
}
