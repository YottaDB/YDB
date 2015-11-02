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
#include "mupint.h"
#include "min_max.h"
#include "init_root_gv.h"
#include "util.h"
#include "print_target.h"
#include "gtmmsg.h"

GBLREF block_id		mu_int_path[];
GBLREF int		mu_int_plen;
GBLREF uint4		mu_int_offset[];
GBLREF uint4		mu_int_errknt;
GBLREF boolean_t	mu_int_err_ranges;
GBLREF boolean_t	master_dir;
GBLREF global_list	*trees;
GBLREF span_node_integ	*sndata;

#define MAX_UTIL_LEN 40
#define BLOCK_WINDOW 8
#define LEVEL_WINDOW 3
#define OFFSET_WINDOW 4

#define TEXT1 ":     "
#define TEXT2 " "
#define TEXT3 ":"
#define TEXT4 ", "

#if defined(UNIX)
#define	NEWLINE	"\n"
#elif defined(VMS)
#define	NEWLINE	"\r\n"
#else
#error UNSUPPORTED_PLATFORM
#endif

void	mu_int_err(
		int err,
		boolean_t do_path,
		boolean_t do_range,
		unsigned char *bot,
		int has_bot,
		unsigned char *top,
		int has_top,
	      	unsigned int level)
{
	int		i, util_len;
	unsigned char	util_buff[MAX_UTIL_LEN];
	unsigned char 	span_key[MAX_KEY_SZ + 1];

	if (!mu_int_errknt)
		util_out_print("!/Block:Offset Level", TRUE);
	mu_int_errknt++;
	mu_int_plen--;
	util_len=0;
	MEMCPY_LIT(&util_buff[util_len], NEWLINE);
	util_len += SIZEOF(NEWLINE) - 1;
	i2hex_blkfill(mu_int_path[mu_int_plen], &util_buff[util_len], BLOCK_WINDOW);
	util_len += BLOCK_WINDOW;
	MEMCPY_LIT(&util_buff[util_len], TEXT1);	/* OFFSET_WINDOW + 1 spaces */
	util_len += SIZEOF(TEXT3) - 1;				/* Using TEXT1 to clear space? */
	i2hex_nofill(mu_int_offset[mu_int_plen], (uchar_ptr_t)&util_buff[util_len], OFFSET_WINDOW);
	util_len += OFFSET_WINDOW + 1;
	i2hex_blkfill(level, (uchar_ptr_t)&util_buff[util_len], LEVEL_WINDOW);
	util_len += LEVEL_WINDOW;
	MEMCPY_LIT(&util_buff[util_len], TEXT2);
	util_len += SIZEOF(TEXT2) - 1;
	util_buff[util_len] = 0;
	if(sndata->sn_type)
		gtm_putmsg(VARLSTCNT(5) err, 3, LEN_AND_STR((char*)util_buff),
				(SPAN_NODE == sndata->sn_type) ? (sndata->span_prev_blk + 2) : (sndata->span_blk_cnt));
	else
		gtm_putmsg(VARLSTCNT(4) err, 2, LEN_AND_STR((char*)util_buff));
	if (do_path)
	{
		if (!master_dir)
		{
			util_out_print("                   Directory Path:  ", FALSE);
			for (i = 0;  trees->path[i + 1];  i++)
			{
				util_len = i2hex_nofill(trees->path[i], (uchar_ptr_t)util_buff, BLOCK_WINDOW);
				MEMCPY_LIT(&util_buff[util_len], TEXT3);
				util_len += SIZEOF(TEXT3) - 1;
				util_len += i2hex_nofill(trees->offset[i], (uchar_ptr_t)&util_buff[util_len], OFFSET_WINDOW);
				MEMCPY_LIT(&util_buff[util_len], TEXT4);
				util_len += SIZEOF(TEXT4) - 1;
				util_buff[util_len] = 0;
				util_out_print((caddr_t)util_buff, FALSE);
			}
			util_len = i2hex_nofill(trees->path[i], (uchar_ptr_t)util_buff, BLOCK_WINDOW);
			MEMCPY_LIT(&util_buff[util_len], TEXT3);
			util_len += SIZEOF(TEXT3) - 1;
			util_len += i2hex_nofill(trees->offset[i], (uchar_ptr_t)&util_buff[util_len], OFFSET_WINDOW);
			util_buff[util_len] = 0;
			util_out_print((caddr_t)util_buff, TRUE);
			util_out_print("                   Path:  ", FALSE);
		} else
			util_out_print("                   Directory Path:  ", FALSE);
		for (i = 0;  i < mu_int_plen;  i++)
		{
			util_len = i2hex_nofill(mu_int_path[i], (uchar_ptr_t)util_buff, BLOCK_WINDOW);
			MEMCPY_LIT(&util_buff[util_len], TEXT3);
			util_len += SIZEOF(TEXT3) - 1;
			util_len += i2hex_nofill(mu_int_offset[i], (uchar_ptr_t)&util_buff[util_len], OFFSET_WINDOW);
			MEMCPY_LIT(&util_buff[util_len], TEXT4);
			util_len += SIZEOF(TEXT4) - 1;
			util_buff[util_len] = 0;
			util_out_print((caddr_t)util_buff, FALSE);
		}
		util_len = i2hex_nofill(mu_int_path[i], (uchar_ptr_t)util_buff, BLOCK_WINDOW);
		MEMCPY_LIT(&util_buff[util_len], TEXT3);
		util_len += SIZEOF(TEXT3) - 1;
		util_len += i2hex_nofill(mu_int_offset[i], (uchar_ptr_t)&util_buff[util_len], OFFSET_WINDOW);
		util_buff[util_len] = 0;
		util_out_print((caddr_t)util_buff, TRUE);
	}
	if (do_range && mu_int_err_ranges)
	{
		util_out_print("Keys from ", FALSE);
		if (has_bot)
		{
			util_out_print("^", FALSE);
			/* in the case bot is the leftmost key of the gvtree, it needs a second null to be a properly terminated
			 * real key for print_target. since it is a simple set, we unconditionally do it for every key */
			bot[has_bot] = 0;
			print_target(bot);
		} else
		{
			assert(master_dir);	/* for a global variable tree, we better have a non-zero begin key */
			util_out_print("^%", FALSE);
		}
		util_out_print(" to ", FALSE);
		if (has_top)
		{
			util_out_print("^", FALSE);
			print_target(top);
		} else
			util_out_print("the end", FALSE);
		util_out_print(" are suspect.", TRUE);
	}
	if (!level && sndata->sn_type)
	{
		if (1 == sndata->sn_type)
			util_out_print("Spanning Node ^", FALSE);
		else
			util_out_print("Spanning Node Chunk ^", FALSE);
		/* in the case bot is the leftmost key of the gvtree, it needs a second null to be a properly terminated
		 * real key for print_target. since it is a simple set, we unconditionally do it for every key
		 */
		sndata->span_node_buf[sndata->key_len] = 0;
		sndata->span_node_buf[sndata->key_len+1] = 0;
		print_target(sndata->span_node_buf);
		util_out_print(" is suspect.", TRUE);
	}
	return;
}
