/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <signal.h>

#include "error.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "dsefind.h"
#include "cli.h"
#include "copy.h"
#include "util.h"
#include "dse.h"

/* Include prototypes*/
#include "t_qread.h"

GBLDEF global_root_list	*global_roots_head, *global_roots_tail;
GBLDEF block_id		patch_left_sib, patch_right_sib, patch_find_blk;
GBLDEF block_id		patch_path[MAX_BT_DEPTH + 1];
GBLDEF block_id		patch_path1[MAX_BT_DEPTH + 1];
GBLDEF int4		patch_offset[MAX_BT_DEPTH + 1];
GBLDEF int4		patch_offset1[MAX_BT_DEPTH + 1];
GBLDEF bool		patch_find_sibs;
GBLDEF bool		patch_exh_found;
GBLDEF bool		patch_find_root_search;
GBLDEF short int	patch_dir_path_count;

GBLREF block_id		patch_curr_blk;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF short int	patch_path_count;

#define MAX_UTIL_LEN 33
static boolean_t	was_crit;
static int4		nocrit_present;

void dse_f_blk(void)
{
    global_root_list	*temp;
    global_dir_path	*d_ptr, *dtemp;
    block_id		blk;
    bool		exhaust;
    char		targ_key[256], util_buff[MAX_UTIL_LEN];
    sm_uc_ptr_t		bp, b_top, rp, r_top, key_top, blk_id;
    short int		size, count, rsize;
    int			util_len;
    int4		dummy_int;
    cache_rec_ptr_t	dummy_cr;

    error_def(ERR_DSEBLKRDFAIL);
    error_def(ERR_CTRLC);

    if (cli_present("BLOCK") == CLI_PRESENT)
    {
	if(!cli_get_hex("BLOCK", (uint4 *)&blk))
	    return;
	if (blk < 0 || blk >= cs_addrs->ti->total_blks
	    || !(blk % cs_addrs->hdr->bplmap))
	{
	    util_out_print("Error: invalid block number.", TRUE);
	    return;
	}
	patch_curr_blk = blk;
    }
    patch_find_sibs = (cli_present("SIBLINGS") == CLI_PRESENT);
    patch_find_blk = patch_curr_blk;
    was_crit = cs_addrs->now_crit;
    nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
    DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, nocrit_present, cs_addrs, gv_cur_region);
    /* ESTABLISH is done here because dse_f_blk_ch() assumes we already
     * have crit.
     */
    ESTABLISH(dse_f_blk_ch);

    if(!(bp = t_qread(patch_find_blk, &dummy_int, &dummy_cr)))
	rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
    if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
	b_top = bp + cs_addrs->hdr->blk_size;
    else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
	b_top = bp + SIZEOF(blk_hdr);
    else
	b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
    rp = bp + SIZEOF(blk_hdr);
    GET_SHORT(rsize, &((rec_hdr_ptr_t) rp)->rsiz);
    if (rsize < SIZEOF(rec_hdr))
	r_top = rp + SIZEOF(rec_hdr);
    else
	r_top = rp + rsize;
    if (r_top > b_top)
	r_top = b_top;
    for (key_top = rp + SIZEOF(rec_hdr); key_top < r_top ; )
    {
	if (!*key_top++)
	    break;
    }
    if (((blk_hdr_ptr_t)bp)->levl && key_top > (blk_id = r_top - SIZEOF(block_id)))
    	key_top = blk_id;
    patch_path_count = 1;
    patch_path[0] = get_dir_root();
    patch_left_sib = patch_right_sib = 0;
    size = key_top - rp - SIZEOF(rec_hdr);
    if (size > SIZEOF(targ_key))
	size = SIZEOF(targ_key);
    patch_find_root_search = TRUE;
    if ((exhaust = (cli_present("EXHAUSTIVE") == CLI_PRESENT)) || size <= 0)
    {
	if (size < 0)
	{
	    util_out_print("No keys in block, cannot perform ordered search.", TRUE);
            DSE_REL_CRIT_AS_APPROPRIATE(was_crit, nocrit_present, cs_addrs, gv_cur_region);
	    REVERT;
	    return;
	}
	if (patch_exh_found = (patch_find_blk == patch_path[0]))
	{
	    if (patch_find_sibs)
		util_out_print("!/    Left siblings    Right siblings!/	none		none", TRUE);
	    else
	    {
		memcpy(util_buff, "!/    Paths--blk:off!/	", 24);
		util_len = 24;
		util_len += i2hex_nofill(patch_find_blk, (uchar_ptr_t)&util_buff[util_len],
					 8);
		util_buff[util_len] = 0;
		util_out_print(util_buff, TRUE);
	    }
	}
	else
	{
	    global_roots_head = (global_root_list *)malloc(SIZEOF(global_root_list));
	    global_roots_tail = global_roots_head;
	    global_roots_head->link = (global_root_list *)0;
	    global_roots_head->dir_path = (global_dir_path *)0;
	    dse_exhaus(1, 0);
	    patch_find_root_search = FALSE;
	    while (!patch_exh_found && global_roots_head->link)
	    {
		patch_path[0] = global_roots_head->link->root;
		patch_path_count = 1;
		patch_left_sib = patch_right_sib = 0;
		if (patch_exh_found = (patch_find_blk == patch_path[0]))
		{
		    if (patch_find_sibs)
			util_out_print("!/    Left siblings    Right siblings!/	none		none", TRUE);
		    else
		    {
			patch_path_count--;
			util_out_print("	Directory path!/	Path--blk:off", TRUE);
			if (!patch_find_root_search)
			{
			    d_ptr = global_roots_head->link->dir_path;
			    while(d_ptr)
			    {
				memcpy(util_buff, "	", 1);
				util_len = 1;
				util_len += i2hex_nofill(d_ptr->block,
							 (uchar_ptr_t)&util_buff[util_len],
							 8);
				memcpy(&util_buff[util_len], ":", 1);
				util_len += 1;
				util_len += i2hex_nofill(d_ptr->offset,
							 (uchar_ptr_t)&util_buff[util_len],
							 4);
				memcpy(&util_buff[util_len], ",", 1);
				util_len += 1;
				util_buff[util_len] = 0;
				util_out_print(util_buff, FALSE);
				temp = (global_root_list *)d_ptr;
				d_ptr = d_ptr->next;
				free(temp);
			    }
			    global_roots_head->link->dir_path = 0;
			    util_out_print("!/!/	Global paths!/	Path--blk:off", TRUE);
			}
			for (count = 0; count < patch_path_count ;count++)
			{
			    memcpy(util_buff, "	", 1);
			    util_len = 1;
			    util_len += i2hex_nofill(patch_path[count],
						     (uchar_ptr_t)&util_buff[util_len],
						     8);
			    memcpy(&util_buff[util_len], ":", 1);
			    util_len += 1;
			    util_len += i2hex_nofill(patch_offset[count], (uchar_ptr_t)&util_buff[util_len], 4);
			    memcpy(&util_buff[util_len], ",", 1);
			    util_len += 1;
			    util_buff[util_len] = 0;
			    util_out_print(util_buff, FALSE);
			}
			memcpy(util_buff, "	", 1);
			util_len = 1;
			util_len += i2hex_nofill(patch_path[count],
						 (uchar_ptr_t)&util_buff[util_len], 8);
			util_buff[util_len] = 0;
			util_out_print(util_buff, TRUE);
		    }
		}
		else
		    dse_exhaus(1, 0);
		temp = global_roots_head;
		d_ptr = global_roots_head->link->dir_path;
		while(d_ptr)
		{
		    dtemp = d_ptr;
		    d_ptr = d_ptr->next;
		    free(dtemp);
		}
		global_roots_head = global_roots_head->link;
		free(temp);
	    }
	    while (global_roots_head->link)
	    {
		temp = global_roots_head;
		d_ptr = global_roots_head->link->dir_path;
		while(d_ptr)
		{
		    dtemp = d_ptr;
		    d_ptr = d_ptr->next;
		    free(dtemp);
		}
		global_roots_head = global_roots_head->link;
		free(temp);
	    }
	}
	if (!patch_exh_found)
	{
	    if (exhaust)
	    {
		util_out_print("Error: exhaustive search fail.", TRUE);
	    }
	    else
	    {
		util_out_print("Error: ordered search fail.", TRUE);
	    }
	}
	else
	    util_out_print(0, TRUE);
        DSE_REL_CRIT_AS_APPROPRIATE(was_crit, nocrit_present, cs_addrs, gv_cur_region);
	REVERT;
	return;
    }
    else /* !exhaust && size > 0 */
    {
	if (!dse_is_blk_in(rp, r_top, size))
	{
	    util_out_print("Error: ordered search fail.", TRUE);
            DSE_REL_CRIT_AS_APPROPRIATE(was_crit, nocrit_present, cs_addrs, gv_cur_region);
	    REVERT;
	    return;
	}
    }
    if (patch_find_sibs)
    {
	util_out_print("!/!_Left sibling!_Right sibling", TRUE);
	if (patch_left_sib)
	{
	    memcpy(util_buff, "!_", 2);
	    util_len = 2;
	    util_len += i2hex_nofill(patch_left_sib, (uchar_ptr_t)&util_buff[util_len], 8);
	    util_buff[util_len] = 0;
	    util_out_print(util_buff, FALSE);
	}
	else
	    util_out_print("!_none", FALSE);
	if (patch_right_sib)
	{
	    memcpy(util_buff, "!_!_", 4);
	    util_len = 4;
	    util_len += i2hex_nofill(patch_right_sib, (uchar_ptr_t)&util_buff[util_len], 8);
	    memcpy(&util_buff[util_len], "!/", 2);
	    util_len += 2;
	    util_buff[util_len] = 0;
	    util_out_print(util_buff, TRUE);
	} else
	    util_out_print("!_!_none!/", TRUE);
        DSE_REL_CRIT_AS_APPROPRIATE(was_crit, nocrit_present, cs_addrs, gv_cur_region);
	REVERT;
	return;
    }
    util_out_print("!/    Directory path!/    Path--blk:off", TRUE);
    patch_dir_path_count--;
    for (count = 0; count < patch_dir_path_count ;count++)
    {
	memcpy(util_buff, "	", 1);
	util_len = 1;
	util_len += i2hex_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], ":", 1);
	util_len += 1;
	util_len += i2hex_nofill(patch_offset[count], (uchar_ptr_t)&util_buff[util_len], 4);
	memcpy(&util_buff[util_len], ",", 1);
	util_len += 1;
	util_buff[util_len] = 0;
	util_out_print(util_buff, FALSE);
    }
    util_out_print("!/    Global tree path!/    Path--blk:off", TRUE);
    if (patch_path_count)
    {
	patch_path_count--;
	for (count = 0; count < patch_path_count ;count++)
	{
	    memcpy(util_buff, "	", 1);
	    util_len = 1;
	    util_len += i2hex_nofill(patch_path1[count], (uchar_ptr_t)&util_buff[util_len], 8);
	    memcpy(&util_buff[util_len], ":", 1);
	    util_len += 1;
	    util_len += i2hex_nofill(patch_offset1[count], (uchar_ptr_t)&util_buff[util_len], 4);
	    memcpy(&util_buff[util_len], ",", 1);
	    util_len += 1;
	    util_buff[util_len] = 0;
	    util_out_print(util_buff, FALSE);
	}
	memcpy(util_buff, "	", 1);
	util_len = 1;
	util_len += i2hex_nofill(patch_path1[count], (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], "!/", 2);
	util_len += 2;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
    }
    else
    {
	memcpy(util_buff, "	", 1);
	util_len = 1;
	util_len += i2hex_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], "!/", 2);
	util_len += 2;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
    }
    DSE_REL_CRIT_AS_APPROPRIATE(was_crit, nocrit_present, cs_addrs, gv_cur_region);
    REVERT;
    return;
}

/* Control-C condition handler */
CONDITION_HANDLER(dse_f_blk_ch)
{
    error_def(ERR_CTRLC);
    START_CH;

    if (SIGNAL == ERR_CTRLC)
        DSE_REL_CRIT_AS_APPROPRIATE(was_crit, nocrit_present, cs_addrs, gv_cur_region);
    NEXTCH;
}

