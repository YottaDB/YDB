/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cli.h"
#include "gdsblk.h"
#include "util.h"
#include "dse.h"
#include "stringpool.h"			/* for GET_CURR_TIME_IN_DOLLARH_AND_ZDATE macro */
#include "op.h"				/* for op_fnzdate and op_horolog prototype */
#include "wcs_recover.h"		/* for wcs_recover prototype */
#include "wcs_phase2_commit_wait.h"
#include "sleep_cnt.h"			/* for SIGNAL_WRITERS_TO_STOP/RESUME and WAIT_FOR_WRITERS_TO_STOP macro */
#include "memcoherency.h"		/* for SIGNAL_WRITERS_TO_STOP/RESUME and WAIT_FOR_WRITERS_TO_STOP macro */
#include "wcs_sleep.h"			/* for SIGNAL_WRITERS_TO_STOP/RESUME and WAIT_FOR_WRITERS_TO_STOP macro */

GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*original_header;

error_def(ERR_SIZENOTVALID4);

#define	DB_ABS2REL(X)	((sm_uc_ptr_t)(X) - (sm_uc_ptr_t)csa->nl)
#define MAX_UTIL_LEN 			40
#define	CLEAN_VERIFY			"verification is clean"
#define	UNCLEAN_VERIFY			"verification is NOT clean (see operator log for details)"
#define	RECOVER_DONE			"recovery complete (see operator log for details)"
#define	RECOVER_NOT_APPLIC		"recovery not applicable with MM access method"

error_def(ERR_SIZENOTVALID4);

void dse_cache(void)
{
	boolean_t	all_present, change_present, recover_present, show_present, verify_present, was_crit, is_clean;
	boolean_t	nocrit_present, offset_present, size_present, value_present;
	gd_region	*reg, *r_top;
	sgmnt_addrs	*csa;
	mval		dollarh_mval, zdate_mval;
	int4		size;
	uint4		offset, value, old_value, lcnt;
	char		dollarh_buffer[MAXNUMLEN], zdate_buffer[SIZEOF(DSE_DMP_TIME_FMT)];
	char		temp_str[256], temp_str1[256];
	sm_uc_ptr_t	chng_ptr;
	cache_rec_ptr_t	cr_que_lo;
	boolean_t	is_mm, was_hold_onto_crit, wc_blocked_ok;

	all_present = (CLI_PRESENT == cli_present("ALL"));

	recover_present = (CLI_PRESENT == cli_present("RECOVER"));
	verify_present = (CLI_PRESENT == cli_present("VERIFY"));
	change_present = (CLI_PRESENT == cli_present("CHANGE"));
	show_present = (CLI_PRESENT == cli_present("SHOW"));

	offset_present = (CLI_PRESENT == cli_present("OFFSET"));
	size_present = (CLI_PRESENT == cli_present("SIZE"));
	value_present = (CLI_PRESENT == cli_present("VALUE"));

	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	assert(!nocrit_present || show_present);	/* NOCRIT is only applicable for SHOW */
	if (offset_present && !cli_get_hex("OFFSET", &offset))
		return;
	if (size_present)
	{
		if (!cli_get_int("SIZE",   &size))
			return;
		if (!((SIZEOF(char) == size) || (SIZEOF(short) == size) || (SIZEOF(int4) == size)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_SIZENOTVALID4);
	}
	if (value_present  && !cli_get_hex("VALUE",  &value))
		return;
	assert(change_present || recover_present || show_present || verify_present);
	for (reg = original_header->regions, r_top = reg + original_header->n_regions; reg < r_top; reg++)
	{
		if (!all_present && (reg != gv_cur_region))
			continue;
		if (!reg->open || reg->was_open)
			continue;
		is_mm = (dba_mm == reg->dyn.addr->acc_meth);
		csa = &FILE_INFO(reg)->s_addrs;
		assert(is_mm || (csa->db_addrs[0] == (sm_uc_ptr_t)csa->nl));
		was_crit = csa->now_crit;
		DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, csa, reg);
		if (verify_present || recover_present)
		{
			GET_CURR_TIME_IN_DOLLARH_AND_ZDATE(dollarh_mval, dollarh_buffer, zdate_mval, zdate_buffer);
			if (verify_present)
			{	/* Before invoking wcs_verify, wait for any pending phase2 commits to finish. Need to wait as
				 * otherwise ongoing phase2 commits can result in cache verification returning FALSE (e.g. due to
				 * DBCRERR message indicating that cr->in_tend is non-zero).
				 * Also, need to wait for concurrent writers to stop to avoid wcs_verify from incorrectly concluding
				 * that there is a problem with the active queue.
				 */
				wc_blocked_ok = UNIX_ONLY(TRUE) VMS_ONLY(!is_mm); /* MM on VMS doesn't support wcs_recvoer */
				if (wc_blocked_ok)
					SIGNAL_WRITERS_TO_STOP(csa->nl); /* done sooner to avoid any new writers starting up */
				if (csa->nl->wcs_phase2_commit_pidcnt && !is_mm)
				{	/* No need to check return value since even if it fails, we want to do cache verification */
					wcs_phase2_commit_wait(csa, NULL);
				}
				if (wc_blocked_ok)
					WAIT_FOR_WRITERS_TO_STOP(csa->nl, lcnt, MAXWTSTARTWAIT / 4); /* reduced wait time for DSE */
				is_clean = wcs_verify(reg, FALSE, FALSE); /* expect_damage is FALSE, caller_is_wcs_recover is
									   * FALSE */
				if (wc_blocked_ok)
					SIGNAL_WRITERS_TO_RESUME(csa->nl);
			} else
			{
				if (UNIX_ONLY(TRUE)VMS_ONLY(!is_mm))
				{
					SET_TRACEABLE_VAR(csa->nl->wc_blocked, TRUE);
					/* No need to invoke function "wcs_phase2_commit_wait" as "wcs_recover" does that anyways */
					wcs_recover(reg);
					assert(FALSE == csa->nl->wc_blocked);	/* wcs_recover() should have cleared this */
				}
			}
			assert(20 == STR_LIT_LEN(DSE_DMP_TIME_FMT)); /* if they are not the same, the !20AD below should change */
			util_out_print("Time !20AD : Region !12AD : Cache !AZ", TRUE, zdate_mval.str.len, zdate_mval.str.addr,
				       REG_LEN_STR(reg), verify_present ? (is_clean ? CLEAN_VERIFY : UNCLEAN_VERIFY)
				       	: UNIX_ONLY(RECOVER_DONE)
				          VMS_ONLY(is_mm ? RECOVER_NOT_APPLIC : RECOVER_DONE));
		} else if (offset_present)
		{
			if ((csa->nl->sec_size VMS_ONLY(* OS_PAGELET_SIZE)) < (offset + size))
				util_out_print("Region !12AD : Error: offset + size is greater than region's max_offset = 0x!XL",
						TRUE, REG_LEN_STR(reg), (csa->nl->sec_size VMS_ONLY(* OS_PAGELET_SIZE)));
			else
			{
				chng_ptr = (sm_uc_ptr_t)csa->nl + offset;
				if (SIZEOF(char) == size)
				{
					SPRINTF(temp_str, "!UB [0x!XB]");
					old_value = *(sm_uc_ptr_t)chng_ptr;
				} else if (SIZEOF(short) == size)
				{
					SPRINTF(temp_str, "!UW [0x!XW]");
					old_value = *(sm_ushort_ptr_t)chng_ptr;
				} else if (SIZEOF(int4) == size)
				{
					SPRINTF(temp_str, "!UL [0x!XL]");
					old_value = *(sm_uint_ptr_t)chng_ptr;
				}
				if (value_present)
				{
					if (SIZEOF(char) == size)
						*(sm_uc_ptr_t)chng_ptr = value;
					else if (SIZEOF(short) == size)
						*(sm_ushort_ptr_t)chng_ptr = value;
					else if (SIZEOF(int4) == size)
						*(sm_uint_ptr_t)chng_ptr = value;
				} else
					value = old_value;
				if (show_present)
				{
					SPRINTF(temp_str1, "Region !12AD : Location !UL [0x!XL] : Value = %s :"
						" Size = !UB [0x!XB]", temp_str);
					util_out_print(temp_str1, TRUE, REG_LEN_STR(reg), offset, offset, value, value, size, size);
				} else
				{
					SPRINTF(temp_str1, "Region !12AD : Location !UL [0x!XL] : Old Value = %s : "
						"New Value = %s : Size = !UB [0x!XB]", temp_str, temp_str);
					util_out_print(temp_str1, TRUE, REG_LEN_STR(reg), offset, offset,
						old_value, old_value, value, value, size, size);
				}
			}
		} else
		{
			assert(show_present);	/* this should be a DSE CACHE -SHOW command with no other qualifiers */
			util_out_print("Region !AD : Shared_memory       = 0x!XJ",
				TRUE, REG_LEN_STR(reg), csa->nl);
			util_out_print("Region !AD :  node_local         = 0x!XJ",
				TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->nl));
			util_out_print("Region !AD :  critical           = 0x!XJ",
				TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->critical));
			if (JNL_ALLOWED(csa))
			{
				util_out_print("Region !AD :  jnl_buffer_struct  = 0x!XJ",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->jnl->jnl_buff));
				util_out_print("Region !AD :  jnl_buffer_data    = 0x!XJ", TRUE, REG_LEN_STR(reg),
					DB_ABS2REL(&csa->jnl->jnl_buff->buff[csa->jnl->jnl_buff->buff_off]));
			}
			util_out_print("Region !AD :  shmpool_buffer     = 0x!XJ",
				TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->shmpool_buffer));
			util_out_print("Region !AD :  lock_space         = 0x!XJ",
				TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->lock_addrs[0]));
			if (!is_mm)
			{
				util_out_print("Region !AD :  cache_queues_state = 0x!XJ",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->acc_meth.bg.cache_state));
				cr_que_lo = &csa->acc_meth.bg.cache_state->cache_array[0];
				util_out_print("Region !AD :  cache_que_header   = 0x!XJ : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(cr_que_lo), csa->hdr->bt_buckets, SIZEOF(cache_rec));
				util_out_print("Region !AD :  cache_record       = 0x!XJ : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(cr_que_lo + csa->hdr->bt_buckets), csa->hdr->n_bts,
					SIZEOF(cache_rec));
				util_out_print("Region !AD :  global_buffer      = 0x!XJ : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg),
					ROUND_UP2(DB_ABS2REL(cr_que_lo + csa->hdr->bt_buckets + csa->hdr->n_bts), OS_PAGE_SIZE),
					csa->hdr->n_bts, csa->hdr->blk_size);
				util_out_print("Region !AD :  db_file_header     = 0x!XJ", TRUE,
					REG_LEN_STR(reg), DB_ABS2REL(csa->hdr));
				util_out_print("Region !AD :  bt_que_header      = 0x!XJ : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->bt_header), csa->hdr->bt_buckets, SIZEOF(bt_rec));
				util_out_print("Region !AD :  th_base            = 0x!XJ",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->th_base));
				util_out_print("Region !AD :  bt_record          = 0x!XJ : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->bt_base), csa->hdr->n_bts, SIZEOF(bt_rec));
				util_out_print("Region !AD :  shared_memory_size = 0x!XL",
					TRUE, REG_LEN_STR(reg), csa->nl->sec_size VMS_ONLY(* OS_PAGELET_SIZE));
			} else
			{
				util_out_print("Region !AD :  shared_memory_size = 0x!XL",
					TRUE, REG_LEN_STR(reg), csa->nl->sec_size VMS_ONLY(* OS_PAGELET_SIZE));
				util_out_print("Region !AD :  db_file_header     = 0x!XJ", TRUE, REG_LEN_STR(reg), csa->hdr);
			}
		}
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, csa, reg);
	}
	return;
}
