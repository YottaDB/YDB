/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
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
#include "stringpool.h"		/* for GET_CURR_TIME_IN_DOLLARH_AND_ZDATE macro */
#include "op.h"			/* for op_fnzdate and op_horolog prototype */
#include "wcs_recover.h"	/* for wcs_recover prototype */

GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*original_header;

#define	DB_ABS2REL(X)	((sm_uc_ptr_t)(X) - (sm_uc_ptr_t)csa->db_addrs[0])
#define MAX_UTIL_LEN 			40
#define	CLEAN_VERIFY			"verification is clean"
#define	UNCLEAN_VERIFY			"verification is NOT clean (see operator log for details)"
#define	RECOVER_DONE			"recovery complete (see operator log for details)"

void dse_cache(void)
{
	boolean_t	all_present, change_present, recover_present, show_present, verify_present, was_crit, is_clean;
	boolean_t	nocrit_present, offset_present, size_present, value_present;
	gd_region	*reg, *r_top;
	sgmnt_addrs	*csa;
	mval		dollarh_mval, zdate_mval;
	int4		size;
	uint4		offset, value, old_value;
	char		dollarh_buffer[MAXNUMLEN], zdate_buffer[sizeof(DSE_DMP_TIME_FMT)];
	char		temp_str[256], temp_str1[256];
	sm_uc_ptr_t	chng_ptr;
	cache_rec_ptr_t	cr_que_lo;

	error_def(ERR_SIZENOTVALID4);

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
		if (!((sizeof(char) == size) || (sizeof(short) == size) || (sizeof(int4) == size)))
			rts_error(VARLSTCNT(1) ERR_SIZENOTVALID4);
	}
	if (value_present  && !cli_get_hex("VALUE",  &value))
		return;
	assert(change_present || recover_present || show_present || verify_present);
	for (reg = original_header->regions, r_top = reg + original_header->n_regions; reg < r_top; reg++)
	{
		if (!all_present && (reg != gv_cur_region))
			continue;
		if (!reg->open || reg->was_open || (dba_bg != reg->dyn.addr->acc_meth))
			continue;
		csa = &FILE_INFO(reg)->s_addrs;
		was_crit = csa->now_crit;
		if (!was_crit && !nocrit_present)
			grab_crit(reg);
		if (verify_present || recover_present)
		{
			GET_CURR_TIME_IN_DOLLARH_AND_ZDATE(dollarh_mval, dollarh_buffer, zdate_mval, zdate_buffer);
			if (verify_present)
				is_clean = wcs_verify(reg, TRUE, FALSE); /* expect_damage is TRUE, caller_is_wcs_recover is FALSE */
			else
			{
				csa->hdr->wc_blocked = TRUE;
				wcs_recover(reg);
				assert(FALSE == csa->hdr->wc_blocked);	/* wcs_recover() should have cleared this */
			}
			assert(20 == STR_LIT_LEN(DSE_DMP_TIME_FMT)); /* if they are not the same, the !20AD below should change */
			util_out_print("Time !20AD : Region !12AD : Cache !AZ", TRUE,
						zdate_mval.str.len, zdate_mval.str.addr,
						REG_LEN_STR(reg),
						verify_present ? (is_clean ? CLEAN_VERIFY : UNCLEAN_VERIFY)
								: RECOVER_DONE);
		} else if (offset_present)
		{
			if ((reg->sec_size VMS_ONLY(* OS_PAGELET_SIZE)) < (offset + size))
				util_out_print("Region !12AD : Error: offset + size is greater than region's max_offset = 0x!XL",
						TRUE, REG_LEN_STR(reg), (reg->sec_size VMS_ONLY(* OS_PAGELET_SIZE)));
			else
			{
				chng_ptr = (sm_uc_ptr_t)csa->db_addrs[0] + offset;
				if (sizeof(char) == size)
				{
					SPRINTF(temp_str, "!UB [0x!XB]");
					old_value = *(sm_uc_ptr_t)chng_ptr;
				} else if (sizeof(short) == size)
				{
					SPRINTF(temp_str, "!UW [0x!XW]");
					old_value = *(sm_ushort_ptr_t)chng_ptr;
				} else if (sizeof(int4) == size)
				{
					SPRINTF(temp_str, "!UL [0x!8XL]");
					old_value = *(sm_uint_ptr_t)chng_ptr;
				}
				if (value_present)
				{
					if (sizeof(char) == size)
						*(sm_uc_ptr_t)chng_ptr = value;
					else if (sizeof(short) == size)
						*(sm_ushort_ptr_t)chng_ptr = value;
					else if (sizeof(int4) == size)
						*(sm_uint_ptr_t)chng_ptr = value;
				} else
					value = old_value;
				if (show_present)
				{
					SPRINTF(temp_str1, "Region !12AD : Location !UL [0x!8XL] : Value = %s :"
								" Size = !UB [0x!XB]", temp_str);
					util_out_print(temp_str1, TRUE, REG_LEN_STR(reg), offset, offset,
								value, value, size, size);
				} else
				{
					SPRINTF(temp_str1, "Region !12AD : Location !UL [0x!8XL] : Old Value = %s : "
						"New Value = %s : Size = !UB [0x!XB]", temp_str, temp_str);
					util_out_print(temp_str1, TRUE, REG_LEN_STR(reg), offset, offset,
								old_value, old_value, value, value, size, size);
				}
			}
		} else
		{
			assert(show_present);	/* this should be a DSE CACHE -SHOW command with no other qualifiers */
			util_out_print("Region !AD : Shared_memory       = 0x!XL",
					TRUE, REG_LEN_STR(reg), csa->db_addrs[0]);
			util_out_print("Region !AD :  node_local         = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->nl));
			util_out_print("Region !AD :  critical           = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->critical));
			if (JNL_ALLOWED(csa))
			{
				util_out_print("Region !AD :  jnl_buffer_struct  = 0x!XL",
						TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->jnl->jnl_buff));
				util_out_print("Region !AD :  jnl_buffer_data    = 0x!XL",
						TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->jnl->jnl_buff->buff));
			}
			util_out_print("Region !AD :  shmpool_buffer     = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->shmpool_buffer));
			util_out_print("Region !AD :  lock_space         = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->lock_addrs[0]));
			util_out_print("Region !AD :  cache_queues_state = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->acc_meth.bg.cache_state));
			cr_que_lo = &csa->acc_meth.bg.cache_state->cache_array[0];
			util_out_print("Region !AD :  cache_que_header   = 0x!XL : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(cr_que_lo), csa->hdr->bt_buckets, sizeof(cache_rec));
			util_out_print("Region !AD :  cache_record       = 0x!XL : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(cr_que_lo + csa->hdr->bt_buckets),
					csa->hdr->n_bts, sizeof(cache_rec));
			util_out_print("Region !AD :  global_buffer      = 0x!XL : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg),
					ROUND_UP2(DB_ABS2REL(cr_que_lo + csa->hdr->bt_buckets + csa->hdr->n_bts), OS_PAGE_SIZE),
					csa->hdr->n_bts, csa->hdr->blk_size);
			util_out_print("Region !AD :  db_file_header     = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->hdr));
			util_out_print("Region !AD :  bt_que_header      = 0x!XL : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->bt_header), csa->hdr->bt_buckets, sizeof(bt_rec));
			util_out_print("Region !AD :  th_base            = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->th_base));
			util_out_print("Region !AD :  bt_record          = 0x!XL : Numelems = 0x!XL : Elemsize = 0x!XL",
					TRUE, REG_LEN_STR(reg), DB_ABS2REL(csa->bt_base), csa->hdr->n_bts, sizeof(bt_rec));
			util_out_print("Region !AD :  shared_memory_size = 0x!XL",
					TRUE, REG_LEN_STR(reg), reg->sec_size VMS_ONLY(* OS_PAGELET_SIZE));
		}
		if (!was_crit && !nocrit_present)
			rel_crit(reg);
	}
	return;
}
