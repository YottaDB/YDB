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

/*******************************************************************************
*
*	MODULE NAME:		CHANGE_FHEAD
*
*	CALLING SEQUENCE:	void change_fhead()
*
*	DESCRIPTION:	This module changes values of certain fields
*			of the file header.  The only range-checking
*			takes place on input, not in this routine, allowing
*			the user maximum control.
*
*	HISTORY:
*
*******************************************************************************/

#include "gtm_string.h"
#include "mdef.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "timersp.h"
#include "jnl.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "gt_timer.h"
#include "timers.h"
#include "dse.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		process_id;
GBLREF int4             image_count;

#define	CLNUP_CRIT					\
{							\
	if (!was_crit)					\
	{						\
		if (nocrit_present)			\
			cs_addrs->now_crit = FALSE;	\
		else					\
			rel_crit(gv_cur_region);	\
	}						\
}

void dse_chng_fhead(void)
{
	int4		x, prev_tn, index_x, save_x;
	unsigned short	buf_len;
	bool		was_crit;
	bool		override = FALSE;
	int4		nocrit_present;
	int4		location_present, location, value_present, value, old_value, size_present, size;
	seq_num		seq_no;
	trans_num	tn;
	char		temp_str[256], temp_str1[256], buf[MAX_LINE];
	int		gethostname_res;
	error_def(ERR_FREEZE);
	error_def(ERR_BLKSIZ512);
	error_def(ERR_DBRDONLY);
	error_def(ERR_SIZENOTVALID);

	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));

	memset(temp_str, 0, 256);
	memset(temp_str1, 0, 256);
	memset(buf, 0, MAX_LINE);
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	if (!was_crit)
	{
		if (nocrit_present)
			cs_addrs->now_crit = TRUE;
		else
			grab_crit(gv_cur_region);
	}
	if (CLI_PRESENT == cli_present("OVERRIDE"))
		override = TRUE;
#ifdef VMS
	if (cs_addrs->hdr->freeze && (cs_addrs->hdr->freeze != process_id ||
		cs_addrs->hdr->image_count != image_count) && !override)
#endif
#ifdef UNIX
	if (cs_addrs->hdr->freeze && (cs_addrs->hdr->image_count != process_id)
		&& !override)
#endif
	{
		CLNUP_CRIT;
                util_out_print("Region: !AD  is frozen by another user, not releasing freeze.",
                                        TRUE, REG_LEN_STR(gv_cur_region));
                rts_error(VARLSTCNT(4) ERR_FREEZE, 2, REG_LEN_STR(gv_cur_region));
                return;

	}
	prev_tn = cs_addrs->ti->curr_tn;
	location_present = FALSE;
	if (CLI_PRESENT == cli_present("LOCATION"))
	{
		location_present = TRUE;
		if (!cli_get_hex("LOCATION", &location))
		{
			CLNUP_CRIT;
			return;
		}
	}
	if (CLI_PRESENT == cli_present("HEXLOCATION"))
	{
		location_present = TRUE;
		if (!cli_get_hex("HEXLOCATION", &location))
		{
			CLNUP_CRIT;
			return;
		}
	}
	if (CLI_PRESENT == cli_present("DECLOCATION"))
	{
		location_present = TRUE;
		if (!cli_get_num("DECLOCATION", &location))
		{
			CLNUP_CRIT;
			return;
		}
	}
	size_present = FALSE;
	if (CLI_PRESENT == cli_present("SIZE"))
	{
		size_present = TRUE;
		if (!cli_get_num("SIZE", &size))
		{
			CLNUP_CRIT;
			return;
		}
	}
	value_present = FALSE;
	if (CLI_PRESENT == cli_present("VALUE"))
	{
		value_present = TRUE;
		if (!cli_get_hex("VALUE", &value))
		{
			CLNUP_CRIT;
			return;
		}
	}
	if (CLI_PRESENT == cli_present("HEXVALUE"))
	{
		value_present = TRUE;
		if (!cli_get_hex("HEXVALUE", &value))
		{
			CLNUP_CRIT;
			return;
		}
	}
	if (CLI_PRESENT == cli_present("DECVALUE"))
	{
		value_present = TRUE;
		if (!cli_get_num("DECVALUE", &value))
		{
			CLNUP_CRIT;
			return;
		}
	}
	if (TRUE == location_present)
	{
		if (FALSE == size_present)
			size = sizeof(int4);
		if (!((sizeof(char) == size) || (sizeof(short) == size) || (sizeof(int4) == size)))
		{
			CLNUP_CRIT;
                        rts_error(VARLSTCNT(1) ERR_SIZENOTVALID);
		}
		if (SGMNT_HDR_LEN < location + size)
			util_out_print("Error: Cannot modify any location outside the file-header", TRUE);
		else  if (0 != location % size)
			util_out_print("Error: Location !1UL [0x!1XL] should be a multiple of Size !1UL [0x!1XL] ",
							TRUE, location, location, size, size);
		else
		{
			if (sizeof(char) == size)
			{
				SPRINTF(temp_str, "!UB [0x!XB]");
				old_value = *(unsigned char *)((char *)cs_addrs->hdr + location);
			}
			else if (sizeof(short) == size)
			{
				SPRINTF(temp_str, "!UW [0x!XW]");
				old_value = *(unsigned short *)((char *)cs_addrs->hdr + location);
			}
			else if (sizeof(int4) == size)
			{
				SPRINTF(temp_str, "!UL [0x!XL]");
				old_value = *(uint4 *)((char *)cs_addrs->hdr + location);
			}
			if (value_present)
			{
				if (sizeof(char) == size)
					*(unsigned char *)((char *)cs_addrs->hdr + location) = value;
				else if (sizeof(short) == size)
					*(unsigned short *)((char *)cs_addrs->hdr + location) = value;
				else if (sizeof(int4) == size)
					*(uint4 *)((char *)cs_addrs->hdr + location) = value;
			}
			else
				value = old_value;
			SPRINTF(temp_str1, "Location !UL [0x!XL] :: Old Value = %s :: New Value = %s :: Size = !UB [0x!XB]",
											temp_str, temp_str);
			util_out_print(temp_str1, TRUE, location, location, old_value, old_value, value, value, size, size);
		}
	}
	if ((CLI_PRESENT == cli_present("TOTAL_BLKS")) && (cli_get_hex("TOTAL_BLKS", &x)))
		cs_addrs->ti->total_blks = x;
	if ((CLI_PRESENT == cli_present("BLOCKS_FREE")) && (cli_get_hex("BLOCKS_FREE", &x)))
		cs_addrs->ti->free_blocks = x;
	if ((CLI_PRESENT == cli_present("BLK_SIZE")) && (cli_get_num("BLK_SIZE", &x)))
	{
		if (!(x % DISK_BLOCK_SIZE) && (0 != x))
			cs_addrs->hdr->blk_size = x;
		else
		{
			cs_addrs->hdr->blk_size = ((x/DISK_BLOCK_SIZE) + 1) * DISK_BLOCK_SIZE;
			CLNUP_CRIT;
			rts_error(VARLSTCNT(4) ERR_BLKSIZ512, 2, x, cs_addrs->hdr->blk_size);
		}
	}
	if ((CLI_PRESENT == cli_present("RECORD_MAX_SIZE")) && (cli_get_num("RECORD_MAX_SIZE", &x)))
	{
		cs_addrs->hdr->max_rec_size = x;
		gv_cur_region->max_rec_size = x;
	}
	if ((CLI_PRESENT == cli_present("KEY_MAX_SIZE")) && (cli_get_num("KEY_MAX_SIZE", &x)))
	{
		cs_addrs->hdr->max_key_size = x;
		gv_cur_region->max_key_size = x;
	}
	if ((CLI_PRESENT == cli_present("REFERENCE_COUNT")) && (cli_get_num("REFERENCE_COUNT", &x)))
		cs_addrs->nl->ref_cnt = x;
	if ((CLI_PRESENT == cli_present("RESERVED_BYTES")) && (cli_get_num("RESERVED_BYTES", &x)))
		cs_addrs->hdr->reserved_bytes = x;
	if ((CLI_PRESENT == cli_present("DEF_COLLATION")) && (cli_get_num("DEF_COLLATION", &x)))
		cs_addrs->hdr->def_coll = x;
	if (CLI_PRESENT == cli_present("NULL_SUBSCRIPTS"))
	{
		x = cli_t_f_n("NULL_SUBSCRIPTS");
		if (1 == x)
			cs_addrs->hdr->null_subs = TRUE;
		else if (0 == x)
			cs_addrs->hdr->null_subs = FALSE;
		gv_cur_region->null_subs = cs_addrs->hdr->null_subs;
	}
	if ((CLI_PRESENT == cli_present("CURRENT_TN")) && (cli_get_hex("CURRENT_TN", &x)))
		cs_addrs->ti->curr_tn = cs_addrs->ti->early_tn = cs_addrs->ti->header_open_tn = x;
	if (CLI_PRESENT == cli_present("REG_SEQNO"))
	{
		buf_len = sizeof(buf);
		cli_get_str("REG_SEQNO", buf, &buf_len);
		seq_no = asc2l((uchar_ptr_t)buf, buf_len);
		QWASSIGN(cs_addrs->hdr->reg_seqno, seq_no);
	}
	if (CLI_PRESENT == cli_present("RESYNC_SEQNO"))
	{
		buf_len = sizeof(buf);
		cli_get_str("RESYNC_SEQNO", buf, &buf_len);
		seq_no = asc2l((uchar_ptr_t)buf, buf_len);
		QWASSIGN(cs_addrs->hdr->resync_seqno, seq_no);
	}
	if (CLI_PRESENT == cli_present("RESYNC_TN"))
	{
		cli_get_hex("RESYNC_TN", (int4 *)&tn);
		cs_addrs->hdr->resync_tn = tn;
	}
	if (CLI_PRESENT == cli_present("CORRUPT_FILE"))
	{
		x = cli_t_f_n("CORRUPT_FILE");
		if (1 == x)
			cs_addrs->hdr->file_corrupt = TRUE;
		else if (0 == x)
			cs_addrs->hdr->file_corrupt = FALSE;
	}
	if ((CLI_PRESENT == cli_present("TIMERS_PENDING")) && (cli_get_num("TIMERS_PENDING", &x)))
		cs_addrs->nl->wcs_timers = x - 1;
	change_fhead_timer("FLUSH_TIME", cs_addrs->hdr->flush_time,
			(dba_bg == cs_addrs->hdr->acc_meth ? TIM_FLU_MOD_BG : TIM_FLU_MOD_MM), FALSE);
	if ((CLI_PRESENT == cli_present("WRITES_PER_FLUSH")) && (cli_get_num("WRITES_PER_FLUSH", &x)))
		cs_addrs->hdr->n_wrt_per_flu = x;
	if ((CLI_PRESENT == cli_present("TRIGGER_FLUSH")) && (cli_get_num("TRIGGER_FLUSH", &x)))
		cs_addrs->hdr->flush_trigger = x;
	change_fhead_timer("STALENESS_TIMER", cs_addrs->hdr->staleness, 5000, TRUE);
	change_fhead_timer("TICK_INTERVAL", cs_addrs->hdr->ccp_tick_interval, 100, TRUE);
	change_fhead_timer("QUANTUM_INTERVAL", cs_addrs->hdr->ccp_quantum_interval, 1000, FALSE);
	change_fhead_timer("RESPONSE_INTERVAL", cs_addrs->hdr->ccp_response_interval, 60000, FALSE);
	if ((CLI_PRESENT == cli_present("B_BYTESTREAM")) && (cli_get_hex("B_BYTESTREAM", &x)))
		cs_addrs->hdr->last_inc_backup = x;
	if ((CLI_PRESENT == cli_present("B_COMPREHENSIVE")) && (cli_get_hex("B_COMPREHENSIVE", &x)))
		cs_addrs->hdr->last_com_backup = x;
	if ((CLI_PRESENT == cli_present("B_DATABASE")) && (cli_get_hex("B_DATABASE", &x)))
		cs_addrs->hdr->last_com_backup = x;
	if ((CLI_PRESENT == cli_present("B_INCREMENTAL")) && (cli_get_hex("B_INCREMENTAL", &x)))
		cs_addrs->hdr->last_inc_backup = x;
	if ((CLI_PRESENT == cli_present("WAIT_DISK")) && (cli_get_num("WAIT_DISK", &x)))
		cs_addrs->hdr->wait_disk_space = (x >= 0 ? x : 0);
#ifdef UNIX
	if ((CLI_PRESENT == cli_present("MUTEX_HARD_SPIN_COUNT")) && (cli_get_num("MUTEX_HARD_SPIN_COUNT", &x)))
		cs_addrs->hdr->mutex_spin_parms.mutex_hard_spin_count = x;
	if ((CLI_PRESENT == cli_present("MUTEX_SLEEP_SPIN_COUNT")) && (cli_get_num("MUTEX_SLEEP_SPIN_COUNT", &x)))
		cs_addrs->hdr->mutex_spin_parms.mutex_sleep_spin_count = x;
	if ((CLI_PRESENT == cli_present("MUTEX_SPIN_SLEEP_TIME")) && (cli_get_num("MUTEX_SPIN_SLEEP_TIME", &x)))
	{
		if (x < 0)
			util_out_print("Error: MUTEX SPIN SLEEP TIME should be non negative", TRUE);
		else
		{
			save_x = x;
			for (index_x = 0;  0 != x;  x >>= 1, index_x++);
			if (index_x <= 1)
				x = index_x;
			else  if ((1 << (index_x - 1)) == save_x)
				x = save_x - 1;
			else
				x = (1 << index_x) - 1;
			if (x > 999999)
				util_out_print("Error: MUTEX SPIN SLEEP TIME should be less than one million micro seconds", TRUE);
			else
				cs_addrs->hdr->mutex_spin_parms.mutex_spin_sleep_mask = x;
		}
	}
#endif
	if ((CLI_PRESENT == cli_present("B_RECORD")) && (cli_get_hex("B_RECORD", &x)))
		cs_addrs->hdr->last_rec_backup = x;
	if (cs_addrs->hdr->clustered)
	{
		cs_addrs->ti->header_open_tn = cs_addrs->ti->curr_tn;		/* Force write of header */
		if (cs_addrs->ti->curr_tn == prev_tn)
		{
			cs_addrs->ti->curr_tn++;
			cs_addrs->ti->early_tn = cs_addrs->ti->curr_tn;
		}
	}
	if ((CLI_PRESENT == cli_present("RC_SRV_COUNT")) && (cli_get_num("RC_SRV_COUNT", &x)))
		cs_addrs->hdr->rc_srv_cnt = x;
	if (CLI_PRESENT == cli_present("FREEZE"))
	{
		x = cli_t_f_n("FREEZE");
		if (1 == x)
		{
			while (!region_freeze(gv_cur_region, TRUE, override))
			{
				hiber_start(1000);
			}
		}
		else if (0 == x)
		{
			if (!region_freeze(gv_cur_region, FALSE, override))
			{
				util_out_print("Region: !AD  is frozen by another user, not releasing freeze.",
					TRUE, REG_LEN_STR(gv_cur_region));
			}

		}
		cs_addrs->persistent_freeze = x;	/* secshr_db_clnup() shouldn't clear the freeze up */
	}
	if (CLI_PRESENT == cli_present("ONLINE_NBB"))
	{
		buf_len = sizeof(buf);
		if (cli_get_str("ONLINE_NBB", buf, &buf_len))
		{
			lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
			if (0 == strcmp(buf, "NOT_IN_PROGRESS"))
				cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			else
			{
				if (('0' == buf[0]) && ('\0' == buf[1]))
					x = 0;
				else
				{
					x = ATOI(buf);
					if (0 == x)
						x = -2;
				}
				if ((x < -1) || (x > BACKUP_NOT_IN_PROGRESS))
					util_out_print("Invalid value for online_nbb qualifier", TRUE);
				else
					cs_addrs->nl->nbb = x;
			}
		}
	}
        if (CLI_PRESENT == cli_present("KILL_IN_PROG"))
        {
                buf_len = sizeof(buf);
                if (cli_get_str("KILL_IN_PROG", buf, &buf_len))
                {
                        lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
                        if (0 == strcmp(buf, "NONE"))
                                cs_addrs->hdr->kill_in_prog = 0;
                        else
                        {
                                if (('0' == buf[0]) && ('\0' == buf[1]))
                                        x = 0;
                                else
                                {
                                        x = ATOI(buf);
                                        if (0 == x)
                                                x = -2;
                                }
                                if (x < -1)
                                        util_out_print("Invalid value for kill_in_prog qualifier", TRUE);
                                else
                                        cs_addrs->hdr->kill_in_prog = x;
                        }
                }
        }
        if (CLI_PRESENT == cli_present("MACHINE_NAME"))
	{
		buf_len = sizeof(buf);
		if (cli_get_str("MACHINE_NAME", buf, &buf_len))
		{
			lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
			if (0 == strcmp(buf, "CURRENT"))
			{
				memset(cs_addrs->hdr->machine_name, 0, MAX_MCNAMELEN);
				GETHOSTNAME(cs_addrs->hdr->machine_name, MAX_MCNAMELEN, gethostname_res);
			}
			else if (0 == strcmp(buf, "CLEAR"))
				memset(cs_addrs->hdr->machine_name, 0, MAX_MCNAMELEN);
			else
				util_out_print("Invalid value for the machine_name qualifier", TRUE);
		} else
			util_out_print("Error: cannot get value for !AD.", TRUE, LEN_AND_LIT("MACHINE_NAME"));

	}
#ifdef UNIX
	if (CLI_PRESENT == cli_present("JNL_YIELD_LIMIT") && cli_get_num("JNL_YIELD_LIMIT", &x))
	{
		if (0 > x)
			util_out_print("YIELD_LIMIT cannot be NEGATIVE", TRUE);
		else if (MAX_YIELD_LIMIT < x)
			util_out_print("YIELD_LIMIT cannot be greater than !UL", TRUE, MAX_YIELD_LIMIT);
		else
			cs_addrs->hdr->yield_lmt = x;
	}
	if (CLI_PRESENT == cli_present("JNL_SYNCIO"))
	{
		x = cli_t_f_n("JNL_SYNCIO");
		if (1 == x)
			cs_addrs->hdr->jnl_sync_io = TRUE;
		else if (0 == x)
			cs_addrs->hdr->jnl_sync_io = FALSE;
	}
#endif
	CLNUP_CRIT;
	return;
}
