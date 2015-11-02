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
#include "send_msg.h"
#include "dse.h"
#include "gtmmsg.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF	VSIG_ATOMIC_T	util_interrupt;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data	*cs_data;
GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		process_id;
GBLREF	uint4		image_count;
LITREF	char		*gtm_dbversion_table[];

error_def(ERR_FREEZE);
error_def(ERR_BLKSIZ512);
error_def(ERR_DBRDONLY);
error_def(ERR_SIZENOTVALID8);
error_def(ERR_FREEZECTRL);

void dse_chng_fhead(void)
{
	int4		x, index_x, save_x, fname_len;
	unsigned short	buf_len;
	boolean_t	was_crit, was_hold_onto_crit, corrupt_file_present;
	boolean_t	override = FALSE;
	int4		nocrit_present;
	int4		location_present, value_present, size_present, size;
	uint4		location;
	boolean_t	max_tn_present, max_tn_warn_present, curr_tn_present, change_tn;
	gtm_uint64_t	value, old_value;
	seq_num		seq_no;
	trans_num	tn, prev_tn, max_tn_old, max_tn_warn_old, curr_tn_old, max_tn_new, max_tn_warn_new, curr_tn_new;
	char		temp_str[256], temp_str1[256], buf[MAX_LINE], *fname_ptr;
	int		gethostname_res;
	sm_uc_ptr_t	chng_ptr;
	const char 	*freeze_msg[] = { "UNFROZEN", "FROZEN" } ;
#	ifdef GTM_CRYPT
	char		hash_buff[GTMCRYPT_HASH_LEN];
	int		gtmcrypt_errno;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	memset(temp_str, 0, 256);
	memset(temp_str1, 0, 256);
	memset(buf, 0, MAX_LINE);
	was_crit = cs_addrs->now_crit;
	/* If the user requested DSE CHANGE -FILE -CORRUPT, then skip the check in grab_crit, which triggers an rts_error, as this
	 * is one of the ways of turning off the file_corrupt flag in the file header
	 */
	TREF(skip_file_corrupt_check) = corrupt_file_present = (CLI_PRESENT == cli_present("CORRUPT_FILE"));
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	TREF(skip_file_corrupt_check) = FALSE;	/* Now that grab_crit is done, reset the global variable */
	if (CLI_PRESENT == cli_present("OVERRIDE"))
		override = TRUE;
#	ifdef VMS
	if (cs_data->freeze && (cs_data->freeze != process_id ||
		cs_data->image_count != image_count) && !override)
#	endif
#	ifdef UNIX
	if (cs_data->freeze && (cs_data->image_count != process_id)
		&& !override)
#	endif
	{
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
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
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return;
		}
	}
	if (CLI_PRESENT == cli_present("HEXLOCATION"))
	{
		location_present = TRUE;
		if (!cli_get_hex("HEXLOCATION", &location))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return;
		}
	}
	if (CLI_PRESENT == cli_present("DECLOCATION"))
	{
		location_present = TRUE;
		if (!cli_get_int("DECLOCATION", (int4 *)&location))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return;
		}
	}
	size_present = FALSE;
	if (CLI_PRESENT == cli_present("SIZE"))
	{
		size_present = TRUE;
		if (!cli_get_int("SIZE", &size))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return;
		}
	}
	value_present = FALSE;
	if (CLI_PRESENT == cli_present("VALUE"))
	{
		value_present = TRUE;
		if (!cli_get_hex64("VALUE", (gtm_uint64_t *)&value))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return;
		}
	}
	if (CLI_PRESENT == cli_present("HEXVALUE"))
	{
		value_present = TRUE;
		if (!cli_get_hex64("HEXVALUE", &value))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return;
		}
	}
	if (CLI_PRESENT == cli_present("DECVALUE"))
	{
		value_present = TRUE;
		if (!cli_get_uint64("DECVALUE", (gtm_uint64_t *)&value))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return;
		}
	}
	if (TRUE == location_present)
	{
		if (FALSE == size_present)
			size = SIZEOF(int4);
		if (!((SIZEOF(char) == size) || (SIZEOF(short) == size) || (SIZEOF(int4) == size) ||
			(SIZEOF(gtm_int64_t) == size)))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
                        rts_error(VARLSTCNT(1) ERR_SIZENOTVALID8);
		}
		if ((0 > (int4)size) || ((uint4)SGMNT_HDR_LEN < (uint4)location)
				|| ((uint4)SGMNT_HDR_LEN < ((uint4)location + (uint4)size)))
			util_out_print("Error: Cannot modify any location outside the file-header", TRUE);
		else  if (0 != location % size)
			util_out_print("Error: Location !UL [0x!XL] should be a multiple of Size !UL",
							TRUE, location, location, size, size);
		else
		{
			chng_ptr = (sm_uc_ptr_t)cs_data + location;
			if (SIZEOF(char) == size)
			{
				SPRINTF(temp_str, "!UB [0x!XB]");
				old_value = *(sm_uc_ptr_t)chng_ptr;
			}
			else if (SIZEOF(short) == size)
			{
				SPRINTF(temp_str, "!UW [0x!XW]");
				old_value = *(sm_ushort_ptr_t)chng_ptr;
			}
			else if (SIZEOF(int4) == size)
			{
				SPRINTF(temp_str, "!UL [0x!XL]");
				old_value = *(sm_uint_ptr_t)chng_ptr;
			}
			else if (SIZEOF(gtm_int64_t) == size)
			{
				SPRINTF(temp_str, "!@UQ [0x!@XQ]");
				old_value = *(qw_num_ptr_t)chng_ptr;
			}
			if (value_present)
			{
				if (SIZEOF(char) == size)
					*(sm_uc_ptr_t)chng_ptr = (unsigned char) value;
				else if (SIZEOF(short) == size)
					*(sm_ushort_ptr_t)chng_ptr = (unsigned short) value;
				else if (SIZEOF(int4) == size)
					*(sm_uint_ptr_t)chng_ptr = (unsigned int) value;
				else if (SIZEOF(gtm_int64_t) == size)
					*(qw_num_ptr_t)chng_ptr = value;
			} else
				value = old_value;
			SPRINTF(temp_str1, "Location !UL [0x!XL] : Old Value = %s : New Value = %s : Size = !UB [0x!XB]",
				temp_str, temp_str);
			if (SIZEOF(int4) >= size)
				util_out_print(temp_str1, TRUE, location, location, (uint4)old_value, (uint4)old_value,
					(uint4)value, (uint4)value, size, size);
			else
				util_out_print(temp_str1, TRUE, location, location, &old_value, &old_value,
					&value, &value, size, size);
		}
	}
	if ((CLI_PRESENT == cli_present("TOTAL_BLKS")) && (cli_get_hex("TOTAL_BLKS", (uint4 *)&x)))
		cs_addrs->ti->total_blks = x;
	if ((CLI_PRESENT == cli_present("BLOCKS_FREE")) && (cli_get_hex("BLOCKS_FREE", (uint4 *)&x)))
		cs_addrs->ti->free_blocks = x;
	if ((CLI_PRESENT == cli_present("BLK_SIZE")) && (cli_get_int("BLK_SIZE", &x)))
	{
		if (!(x % DISK_BLOCK_SIZE) && (0 != x))
			cs_data->blk_size = x;
		else
		{
			cs_data->blk_size = ((x/DISK_BLOCK_SIZE) + 1) * DISK_BLOCK_SIZE;
			gtm_putmsg(VARLSTCNT(4) ERR_BLKSIZ512, 2, x, cs_data->blk_size);
		}
	}
	if ((CLI_PRESENT == cli_present("RECORD_MAX_SIZE")) && (cli_get_int("RECORD_MAX_SIZE", &x)))
	{
		cs_data->max_rec_size = x;
		gv_cur_region->max_rec_size = x;
	}
	if ((CLI_PRESENT == cli_present("KEY_MAX_SIZE")) && (cli_get_int("KEY_MAX_SIZE", &x)))
	{
		if (cs_data->max_key_size > x)
			cs_data->maxkeysz_assured = FALSE;
		cs_data->max_key_size = x;
		gv_cur_region->max_key_size = x;
	}
	if ((CLI_PRESENT == cli_present("INHIBIT_KILLS")) && (cli_get_int("INHIBIT_KILLS", &x)))
	{
		cs_addrs->nl->inhibit_kills = x;
	}
	if (CLI_PRESENT == cli_present("INTERRUPTED_RECOV"))
	{
		x = cli_t_f_n("INTERRUPTED_RECOV");
		if (1 == x)
			cs_data->recov_interrupted = TRUE;
		else if (0 == x)
			cs_data->recov_interrupted = FALSE;
	}
	if ((CLI_PRESENT == cli_present("REFERENCE_COUNT")) && (cli_get_int("REFERENCE_COUNT", &x)))
		cs_addrs->nl->ref_cnt = x;
	if ((CLI_PRESENT == cli_present("RESERVED_BYTES")) && (cli_get_int("RESERVED_BYTES", &x)))
		cs_data->reserved_bytes = x;
	if ((CLI_PRESENT == cli_present("DEF_COLLATION")) && (cli_get_int("DEF_COLLATION", &x)))
		cs_data->def_coll = x;
	if (CLI_PRESENT == cli_present("NULL_SUBSCRIPTS"))
	{
		x = cli_n_a_e("NULL_SUBSCRIPTS");
		if (-1 != x)
			gv_cur_region->null_subs = cs_data->null_subs = (unsigned char)x;
	}
	if (CLI_PRESENT == cli_present("CERT_DB_VER"))
	{
		buf_len = SIZEOF(buf);
		if (cli_get_str("CERT_DB_VER", buf, &buf_len))
		{
			lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
			for (index_x=0; index_x < GDSVLAST ; index_x++)
				if (0 == STRCMP(buf, gtm_dbversion_table[index_x]))
				{
					cs_data->certified_for_upgrade_to = (enum db_ver)index_x;
					break;
				}
			if (GDSVLAST <= index_x)
				util_out_print("Invalid value for CERT_DB_VER qualifier", TRUE);
		}
	}
	if (CLI_PRESENT == cli_present("DB_WRITE_FMT"))
	{
		buf_len = SIZEOF(buf);
		if (cli_get_str("DB_WRITE_FMT", buf, &buf_len))
		{
			lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
			for (index_x=0; index_x < GDSVLAST ; index_x++)
				if (0 == STRCMP(buf, gtm_dbversion_table[index_x]))
				{
					cs_data->desired_db_format = (enum db_ver)index_x;
					cs_data->fully_upgraded = FALSE;
					break;
				}
			if (GDSVLAST <= index_x)
				util_out_print("Invalid value for DB_WRITE_FMT qualifier", TRUE);
		}
	}
	/* ---------- Begin ------ CURRENT_TN/MAX_TN/WARN_MAX_TN processing -------- */
	max_tn_old = cs_data->max_tn;
	if ((CLI_PRESENT == cli_present("MAX_TN")) && (cli_get_hex64("MAX_TN", &max_tn_new)))
		max_tn_present = TRUE;
	else
	{
		max_tn_present = FALSE;
		max_tn_new = max_tn_old;
	}
	max_tn_warn_old = cs_data->max_tn_warn;
	if ((CLI_PRESENT == cli_present("WARN_MAX_TN")) && (cli_get_hex64("WARN_MAX_TN", &max_tn_warn_new)))
		max_tn_warn_present = TRUE;
	else
	{
		max_tn_warn_present = FALSE;
		max_tn_warn_new = max_tn_warn_old;
	}
	curr_tn_old = cs_addrs->ti->curr_tn;
	if ((CLI_PRESENT == cli_present("CURRENT_TN")) && (cli_get_hex64("CURRENT_TN", &curr_tn_new)))
		curr_tn_present = TRUE;
	else
	{
		curr_tn_present = FALSE;
		curr_tn_new = curr_tn_old;
	}
	change_tn = TRUE;
	if (max_tn_present)
	{
		if (max_tn_new < max_tn_warn_new)
		{
			change_tn = FALSE;
			util_out_print("MAX_TN value cannot be less than the current/specified value of WARN_MAX_TN", TRUE);
		}
	}
	if (max_tn_warn_present)
	{
		if (!max_tn_present && (max_tn_warn_new > max_tn_new))
		{
			change_tn = FALSE;
			util_out_print("WARN_MAX_TN value cannot be greater than the current/specified value of MAX_TN", TRUE);
		}
		if (max_tn_warn_new < curr_tn_new)
		{
			change_tn = FALSE;
			util_out_print("WARN_MAX_TN value cannot be less than the current/specified value of CURRENT_TN", TRUE);
		}
	}
	if (curr_tn_present)
	{
		if (!max_tn_warn_present && (curr_tn_new > max_tn_warn_new))
		{
			change_tn = FALSE;
			util_out_print("CURRENT_TN value cannot be greater than the current/specified value of WARN_MAX_TN", TRUE);
		}
	}
	if (change_tn)
	{
		if (max_tn_present)
			cs_data->max_tn = max_tn_new;
		if (max_tn_warn_present)
			cs_data->max_tn_warn = max_tn_warn_new;
		if (curr_tn_present)
			cs_addrs->ti->curr_tn = cs_addrs->ti->early_tn = curr_tn_new;
		assert(max_tn_new == cs_data->max_tn);
		assert(max_tn_warn_new == cs_data->max_tn_warn);
		assert(curr_tn_new == cs_addrs->ti->curr_tn);
		assert(max_tn_new >= max_tn_warn_new);
		assert(max_tn_warn_new >= curr_tn_new);
	} else
	{
		/* if (max_tn_present)
			util_out_print("MAX_TN value not changed", TRUE);
		   if (max_tn_warn_present)
			util_out_print("WARN_MAX_TN value not changed", TRUE);
		   if (curr_tn_present)
			util_out_print("CURRENT_TN value not changed", TRUE);
		*/
		assert(max_tn_old == cs_data->max_tn);
		assert(max_tn_warn_old == cs_data->max_tn_warn);
		assert(curr_tn_old == cs_addrs->ti->curr_tn);
	}
	/* ---------- End ------ CURRENT_TN/MAX_TN/WARN_MAX_TN processing -------- */
	if (CLI_PRESENT == cli_present("REG_SEQNO") && cli_get_hex64("REG_SEQNO", (gtm_uint64_t *)&seq_no))
		cs_data->reg_seqno = seq_no;
	UNIX_ONLY(
		if (CLI_PRESENT == cli_present("STRM_NUM"))
		{
			assert(CLI_PRESENT == cli_present("STRM_REG_SEQNO"));
			if (cli_get_int("STRM_NUM", &x) && (0 <= x) && (MAX_SUPPL_STRMS > x)
					&& (CLI_PRESENT == cli_present("STRM_REG_SEQNO"))
					&& cli_get_hex64("STRM_REG_SEQNO", (gtm_uint64_t *)&seq_no))
				cs_data->strm_reg_seqno[x] = seq_no;
		}
	)
	VMS_ONLY(
		if (CLI_PRESENT == cli_present("RESYNC_SEQNO") && cli_get_hex64("RESYNC_SEQNO", (gtm_uint64_t *)&seq_no))
			cs_data->resync_seqno = seq_no;
		if (CLI_PRESENT == cli_present("RESYNC_TN") && cli_get_hex64("RESYNC_TN", &tn))
			cs_data->resync_tn = tn;
	)
	UNIX_ONLY(
		if (CLI_PRESENT == cli_present("ZQGBLMOD_SEQNO") && cli_get_hex64("ZQGBLMOD_SEQNO", (gtm_uint64_t *)&seq_no))
			cs_data->zqgblmod_seqno = seq_no;
		if (CLI_PRESENT == cli_present("ZQGBLMOD_TN") && cli_get_hex64("ZQGBLMOD_TN", &tn))
			cs_data->zqgblmod_tn = tn;
	)
	if (CLI_PRESENT == cli_present("STDNULLCOLL"))
	{
		if ( -1 != (x = cli_t_f_n("STDNULLCOLL")))
			gv_cur_region->std_null_coll = cs_data->std_null_coll = x;
	}
	if (corrupt_file_present)
	{
		x = cli_t_f_n("CORRUPT_FILE");
		if (1 == x)
			cs_data->file_corrupt = TRUE;
		else if (0 == x)
			cs_data->file_corrupt = FALSE;
	}
	if ((CLI_PRESENT == cli_present("TIMERS_PENDING")) && (cli_get_int("TIMERS_PENDING", &x)))
		cs_addrs->nl->wcs_timers = x - 1;
	change_fhead_timer("FLUSH_TIME", cs_data->flush_time,
			(dba_bg == cs_data->acc_meth ? TIM_FLU_MOD_BG : TIM_FLU_MOD_MM), FALSE);
	if ((CLI_PRESENT == cli_present("WRITES_PER_FLUSH")) && (cli_get_int("WRITES_PER_FLUSH", &x)))
		cs_data->n_wrt_per_flu = x;
	if ((CLI_PRESENT == cli_present("TRIGGER_FLUSH")) && (cli_get_int("TRIGGER_FLUSH", &x)))
		cs_data->flush_trigger = x;
	if ((CLI_PRESENT == cli_present("GOT2V5ONCE")) && (cli_get_int("GOT2V5ONCE", &x)))
                cs_data->db_got_to_v5_once = (boolean_t)x;
	change_fhead_timer("STALENESS_TIMER", cs_data->staleness, 5000, TRUE);
	change_fhead_timer("TICK_INTERVAL", cs_data->ccp_tick_interval, 100, TRUE);
	change_fhead_timer("QUANTUM_INTERVAL", cs_data->ccp_quantum_interval, 1000, FALSE);
	change_fhead_timer("RESPONSE_INTERVAL", cs_data->ccp_response_interval, 60000, FALSE);
	if ((CLI_PRESENT == cli_present("B_BYTESTREAM")) && (cli_get_hex64("B_BYTESTREAM", &tn)))
		cs_data->last_inc_backup = tn;
	if ((CLI_PRESENT == cli_present("B_COMPREHENSIVE")) && (cli_get_hex64("B_COMPREHENSIVE", &tn)))
		cs_data->last_com_backup = tn;
	if ((CLI_PRESENT == cli_present("B_DATABASE")) && (cli_get_hex64("B_DATABASE", &tn)))
		cs_data->last_com_backup = tn;
	if ((CLI_PRESENT == cli_present("B_INCREMENTAL")) && (cli_get_hex64("B_INCREMENTAL", &tn)))
		cs_data->last_inc_backup = tn;
	if ((CLI_PRESENT == cli_present("WAIT_DISK")) && (cli_get_int("WAIT_DISK", &x)))
		cs_data->wait_disk_space = (x >= 0 ? x : 0);
	if (((CLI_PRESENT == cli_present("HARD_SPIN_COUNT")) && cli_get_int("HARD_SPIN_COUNT", &x))
	      UNIX_ONLY( || ((CLI_PRESENT == cli_present("MUTEX_HARD_SPIN_COUNT")) && cli_get_int("MUTEX_HARD_SPIN_COUNT", &x)))
	   ) /* Unix should be backward compatible, accept MUTEX_ prefix qualifiers as well */
	{
		if (0 < x)
			cs_data->mutex_spin_parms.mutex_hard_spin_count = x;
		else
			util_out_print("Error: HARD SPIN COUNT should be a non zero positive number", TRUE);
	}
	if (((CLI_PRESENT == cli_present("SLEEP_SPIN_COUNT")) && cli_get_int("SLEEP_SPIN_COUNT", &x))
	      UNIX_ONLY( || ((CLI_PRESENT == cli_present("MUTEX_SLEEP_SPIN_COUNT")) && cli_get_int("MUTEX_SLEEP_SPIN_COUNT", &x)))
	   ) /* Unix should be backward compatible, accept MUTEX_ prefix qualifiers as well */
	{
		if (0 < x)
			cs_data->mutex_spin_parms.mutex_sleep_spin_count = x;
		else
			util_out_print("Error: SLEEP SPIN COUNT should be a non zero positive number", TRUE);
	}
	if (((CLI_PRESENT == cli_present("SPIN_SLEEP_TIME")) && cli_get_int("SPIN_SLEEP_TIME", &x))
	      UNIX_ONLY( || ((CLI_PRESENT == cli_present("MUTEX_SPIN_SLEEP_TIME")) && cli_get_int("MUTEX_SPIN_SLEEP_TIME", &x)))
	   ) /* Unix should be backward compatible, accept MUTEX_ prefix qualifiers as well */
	{
		if (x < 0)
			util_out_print("Error: SPIN SLEEP TIME should be non negative", TRUE);
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
				util_out_print("Error: SPIN SLEEP TIME should be less than one million micro seconds", TRUE);
			else
				cs_data->mutex_spin_parms.mutex_spin_sleep_mask = x;
		}
	}
	UNIX_ONLY(
		if ((CLI_PRESENT == cli_present("COMMITWAIT_SPIN_COUNT")) && cli_get_int("COMMITWAIT_SPIN_COUNT", &x))
		{
			if (0 <= x)
				cs_data->wcs_phase2_commit_wait_spincnt = x;
			else
				util_out_print("Error: COMMITWAIT SPIN COUNT should be a positive number", TRUE);
		}
	)
	if ((CLI_PRESENT == cli_present("B_RECORD")) && (cli_get_hex64("B_RECORD", &tn)))
		cs_data->last_rec_backup = tn;
	if ((CLI_PRESENT == cli_present("BLKS_TO_UPGRADE")) && (cli_get_hex("BLKS_TO_UPGRADE", (uint4 *)&x)))
	{
		cs_data->blks_to_upgrd = x;
		cs_data->fully_upgraded = FALSE;
	}
	if ((CLI_PRESENT == cli_present("MBM_SIZE")) && (cli_get_int("MBM_SIZE", &x)))
		cs_data->master_map_len = x * DISK_BLOCK_SIZE;
	if (cs_data->clustered)
	{
		if (cs_addrs->ti->curr_tn == prev_tn)
		{
			CHECK_TN(cs_addrs, cs_data, cs_addrs->ti->curr_tn);/* can issue rts_error TNTOOLARGE */
			cs_addrs->ti->early_tn++;
			INCREMENT_CURR_TN(cs_data);
		}
	}
	if ((CLI_PRESENT == cli_present("RC_SRV_COUNT")) && (cli_get_int("RC_SRV_COUNT", &x)))
		cs_data->rc_srv_cnt = x;
	if (CLI_PRESENT == cli_present("FREEZE"))
	{
		x = cli_t_f_n("FREEZE");
		if (1 == x)
		{
			while (REG_ALREADY_FROZEN == region_freeze(gv_cur_region, TRUE, override, FALSE))
			{
				hiber_start(1000);
				if (util_interrupt)
				{
					gtm_putmsg(VARLSTCNT(1) ERR_FREEZECTRL);
					break;
				}
			}
		}
		else if (0 == x)
		{
			if (REG_ALREADY_FROZEN == region_freeze(gv_cur_region, FALSE, override, FALSE))
			{
				util_out_print("Region: !AD  is frozen by another user, not releasing freeze.",
					TRUE, REG_LEN_STR(gv_cur_region));
			}

		}
		if (x != !(cs_data->freeze))
			util_out_print("Region !AD is now !AD", TRUE, REG_LEN_STR(gv_cur_region), LEN_AND_STR(freeze_msg[x]));
		cs_addrs->persistent_freeze = x;	/* secshr_db_clnup() shouldn't clear the freeze up */
	}
	if (CLI_PRESENT == cli_present("FULLY_UPGRADED") && cli_get_int("FULLY_UPGRADED", &x))
	{
		cs_data->fully_upgraded = (boolean_t)x;
		if (x)
			cs_data->db_got_to_v5_once = TRUE;
	}
	if (CLI_PRESENT == cli_present("GVSTATSRESET"))
	{
		/* Clear statistics in NODE-LOCAL first */
#		define TAB_GVSTATS_REC(COUNTER,TEXT1,TEXT2)	cs_addrs->nl->gvstats_rec.COUNTER = 0;
#		include "tab_gvstats_rec.h"
#		undef TAB_GVSTATS_REC
		/* Do it in the file-header next */
		gvstats_rec_cnl2csd(cs_addrs);
	}
	if (CLI_PRESENT == cli_present("ONLINE_NBB"))
	{
		buf_len = SIZEOF(buf);
		if (cli_get_str("ONLINE_NBB", buf, &buf_len))
		{
			lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
			if (0 == STRCMP(buf, "NOT_IN_PROGRESS"))
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
				if (x < -1)
					util_out_print("Invalid value for online_nbb qualifier", TRUE);
				else
					cs_addrs->nl->nbb = x;
			}
		}
	}
	if (CLI_PRESENT == cli_present("ABANDONED_KILLS"))
	{
		buf_len = SIZEOF(buf);
		if (cli_get_str("ABANDONED_KILLS", buf, &buf_len))
		{
			lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
			if (0 == STRCMP(buf, "NONE"))
				cs_data->abandoned_kills = 0;
			else
			{
				if (('0' == buf[0]) && ('\0' == buf[1]))
					x = 0;
				else
				{
					x = ATOI(buf);
					if (0 == x)
						x = -1;
				}
				if (0 > x)
					util_out_print("Invalid value for abandoned_kills qualifier", TRUE);
				else
					cs_data->abandoned_kills = x;
			}
		}
	}
        if (CLI_PRESENT == cli_present("KILL_IN_PROG"))
        {
                buf_len = SIZEOF(buf);
                if (cli_get_str("KILL_IN_PROG", buf, &buf_len))
                {
                        lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
                        if (0 == STRCMP(buf, "NONE"))
                                cs_data->kill_in_prog = 0;
                        else
                        {
                                if (('0' == buf[0]) && ('\0' == buf[1]))
                                        x = 0;
                                else
                                {
                                        x = ATOI(buf);
                                        if (0 == x)
                                                x = -1;
                                }
                                if (0 > x)
                                        util_out_print("Invalid value for kill_in_prog qualifier", TRUE);
                                else
                                        cs_data->kill_in_prog = x;
                        }
                }
        }
        if (CLI_PRESENT == cli_present("MACHINE_NAME"))
	{
		buf_len = SIZEOF(buf);
		if (cli_get_str("MACHINE_NAME", buf, &buf_len))
		{
			lower_to_upper((uchar_ptr_t)buf, (uchar_ptr_t)buf, buf_len);
			if (0 == STRCMP(buf, "CURRENT"))
			{
				memset(cs_data->machine_name, 0, MAX_MCNAMELEN);
				GETHOSTNAME(cs_data->machine_name, MAX_MCNAMELEN, gethostname_res);
			}
			else if (0 == STRCMP(buf, "CLEAR"))
				memset(cs_data->machine_name, 0, MAX_MCNAMELEN);
			else
				util_out_print("Invalid value for the machine_name qualifier", TRUE);
		} else
			util_out_print("Error: cannot get value for !AD.", TRUE, LEN_AND_LIT("MACHINE_NAME"));

	}
#	ifdef GTM_CRYPT
	if (CLI_PRESENT == cli_present("ENCRYPTION_HASH"))
	{
		if (1 < cs_addrs->nl->ref_cnt)
		{
			util_out_print("Cannot reset encryption hash in file header while !XL other processes are "
					"accessing the database.",
					TRUE,
					cs_addrs->nl->ref_cnt - 1);
			return;
		}
		fname_ptr = (char *)gv_cur_region->dyn.addr->fname;
		fname_len = gv_cur_region->dyn.addr->fname_len;
		ASSERT_ENCRYPTION_INITIALIZED;
		/* Now generate the new hash to be placed in the database file header. */
		GTMCRYPT_HASH_GEN(cs_addrs, fname_ptr, fname_len, hash_buff, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, fname_len, fname_ptr);
		memcpy(cs_data->encryption_hash, hash_buff, GTMCRYPT_HASH_LEN);
		DEBUG_ONLY(GTMCRYPT_HASH_CHK(cs_addrs, cs_data->encryption_hash, gtmcrypt_errno));
		assert(0 == gtmcrypt_errno);
	}
#	endif

#	ifdef UNIX
	if (CLI_PRESENT == cli_present("JNL_YIELD_LIMIT") && cli_get_int("JNL_YIELD_LIMIT", &x))
	{
		if (0 > x)
			util_out_print("YIELD_LIMIT cannot be NEGATIVE", TRUE);
		else if (MAX_YIELD_LIMIT < x)
			util_out_print("YIELD_LIMIT cannot be greater than !UL", TRUE, MAX_YIELD_LIMIT);
		else
			cs_data->yield_lmt = x;
	}
	if (CLI_PRESENT == cli_present("QDBRUNDOWN"))
	{
		cs_data->mumps_can_bypass = TRUE;
		util_out_print("Database file !AD now has quick database rundown flag set to TRUE", TRUE,
					DB_LEN_STR(gv_cur_region));
	}
	else if (CLI_NEGATED == cli_present("QDBRUNDOWN"))
	{
		cs_data->mumps_can_bypass = FALSE;
		util_out_print("Database file !AD now has quick database rundown flag set to FALSE", TRUE,
					DB_LEN_STR(gv_cur_region));
	}
#	endif
	if (CLI_PRESENT == cli_present(UNIX_ONLY("JNL_SYNCIO") VMS_ONLY("JNL_CACHE")))
	{
		x = cli_t_f_n(UNIX_ONLY("JNL_SYNCIO") VMS_ONLY("JNL_CACHE"));
		if (1 == x)
			cs_data->jnl_sync_io = UNIX_ONLY(TRUE) VMS_ONLY(FALSE);
		else if (0 == x)
			cs_data->jnl_sync_io = UNIX_ONLY(FALSE) VMS_ONLY(TRUE);
	}
	if ((CLI_PRESENT == cli_present("AVG_BLKS_READ")) && (cli_get_int("AVG_BLKS_READ", &x)))
	{
		if (x <= 0)
			util_out_print("Invalid value for AVG_BLKS_READ qualifier", TRUE);
		else
			cs_data->avg_blks_per_100gbl = x;
	}
	if ((CLI_PRESENT == cli_present("PRE_READ_TRIGGER_FACTOR")) && (cli_get_int("PRE_READ_TRIGGER_FACTOR", &x)))
	{
		if ((x < 0) || (x > 100))
			util_out_print("Invalid value for PRE_READ_TRIGGER_FACTOR qualifier", TRUE);
		else
			cs_data->pre_read_trigger_factor = x;
	}
	if ((CLI_PRESENT == cli_present("UPD_RESERVED_AREA")) && (cli_get_int("UPD_RESERVED_AREA", &x)))
	{
		if ((x < 0) || (x > 100))
			util_out_print("Invalid value for UPD_RESERVED_AREA qualifier", TRUE);
		else
			cs_data->reserved_for_upd = x;
	}
	if ((CLI_PRESENT == cli_present("UPD_WRITER_TRIGGER_FACTOR")) && (cli_get_int("UPD_WRITER_TRIGGER_FACTOR", &x)))
	{
		if ((x < 0) || (x > 100))
			util_out_print("Invalid value for UPD_WRITER_TRIGGER_FACTOR qualifier", TRUE);
		else
			cs_data->writer_trigger_factor = x;
	}
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return;
}
