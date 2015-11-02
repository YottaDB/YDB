/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
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
#include "filestruct.h"
#include "jnl.h"
#include "min_max.h"
#include "repl_sort_tr_buff.h"

void repl_sort_tr_buff(uchar_ptr_t tr_buff, uint4 tr_bufflen)
{
	boolean_t		already_sorted, is_set_kill_zkill_ztrig_ztworm, sorting_needed;
	uchar_ptr_t		tb, dst_addr, this_jrec_addr, working_record_addr, next_record_addr, reg_top;
	static uchar_ptr_t	private_tr_buff;
	static reg_jrec_info_t	*reg_jrec_info_array;
	static uint4		private_tr_bufflen = 0, max_participants = 0;
	struct_jrec_tcom	*last_tcom_rec_ptr;
	enum jnl_record_type	rectype;
	int			balanced, tlen;
	uint4			num_records, cur_rec_idx, reg_idx, reclen, cur_updnum, max_updnum = 0, prev_updnum = 0;
	uint4			working_record, copy_len, idx, min_updnum_reg, min_val, next_min_val, this_reg_updnum;
	uint4			participants;
#	ifdef DEBUG
	uint4			tmp_sum, tcom_num = 0, prev_updnum_this_reg;
#	endif
	jnl_record		*rec;
	jrec_prefix		*prefix;
	long			first_tcom_offset = 0;

	tb = tr_buff;
	tlen = tr_bufflen;
	assert(0 != tr_bufflen);
	assert(0 == ((UINTPTR_T)tb % SIZEOF(uint4)));
	prefix = (jrec_prefix *)tb;
	rectype = (enum jnl_record_type)prefix->jrec_type;
	assert(!IS_ZTP(rectype));
	if (prefix->forwptr == tlen)
	{	/* there is only one journal record in this buffer. Make sure it is either JRT_SET/JRT_KILL/JRT_NULL */
		assert((JRT_SET == rectype) || (JRT_KILL == rectype) || (JRT_ZKILL == rectype) || (JRT_NULL == rectype));
		/* No sorting needed. */
		return;
	} else /* We have a TP transaction buffer */
	{
		if (!IS_TUPD(rectype))
		{
			assert(FALSE);
			return;
		}
		/* We should have at least one TCOM record at the end. The check for balanced TSET/TCOM pairs will be done below */
		last_tcom_rec_ptr = (struct_jrec_tcom *)(tb + tlen - SIZEOF(struct_jrec_tcom));
		prefix = (jrec_prefix *)(last_tcom_rec_ptr);
		participants = last_tcom_rec_ptr->num_participants;
		if (JRT_TCOM != prefix->jrec_type)
		{
			assert(FALSE);
			return;
		}
	}
	PRO_ONLY(
		/* A single region TP transaction is always sorted. So, for pro, return without addition sorting */
		if (1 == participants)
			return;
	)
	already_sorted = TRUE;
	num_records = cur_rec_idx = reg_idx = balanced = 0;
	if (max_participants < participants)
	{
		if (NULL != reg_jrec_info_array)
			free(reg_jrec_info_array);
		max_participants = participants;
		reg_jrec_info_array = malloc(SIZEOF(reg_jrec_info_t) * max_participants);
	}
	while(JREC_PREFIX_SIZE <= tlen)
	{
		assert(0 == ((UINTPTR_T)tb % SIZEOF(uint4)));
		prefix = (jrec_prefix *)tb;
		rectype = (enum jnl_record_type)prefix->jrec_type;
		assert(!IS_ZTP(rectype));
		rec = (jnl_record *)tb;	/* Start of this record */
		reclen = prefix->forwptr;
		if ((0 == reclen) || (reclen > tlen))
		{	/* Bad record. For pro, we return. The actual error will be reported by update process */
			assert(FALSE);
			return;
		}
		is_set_kill_zkill_ztrig_ztworm = IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype);
		assert(IS_REPLICATED(rectype));
		assert(is_set_kill_zkill_ztrig_ztworm || (JRT_TCOM == rectype));
		if (is_set_kill_zkill_ztrig_ztworm)
		{
			assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
			cur_updnum = rec->jrec_set_kill.update_num;
			already_sorted = (already_sorted && (prev_updnum <= cur_updnum));
			max_updnum = MAX(max_updnum, cur_updnum);
			if (IS_TUPD(rectype))
			{	/* Begin of a new region's transaction. Note it down. */
				DEBUG_ONLY(prev_updnum_this_reg = cur_updnum;)
				balanced++;
				assert(reg_idx < participants);
				reg_jrec_info_array[reg_idx].working_offset = (long)(tb - tr_buff);
				if (0 < reg_idx)
					reg_jrec_info_array[reg_idx - 1].end = (long)(tb - tr_buff);
				reg_idx++;
			}
			DEBUG_ONLY(
				/* update_num within a region SHOULD be sorted */
				assert(prev_updnum_this_reg <= cur_updnum);
				prev_updnum_this_reg = cur_updnum;
			)
			prev_updnum = cur_updnum;
		} else
		{	/* TCOM records does not have update_num. */
			if (!first_tcom_offset)
			{
				first_tcom_offset = (long)(tb - tr_buff);
				assert(first_tcom_offset);
			}
			DEBUG_ONLY(tcom_num++;)
			balanced--;
			if (0 > balanced)
				break;
		}
		num_records++;
		tlen -= reclen;
		tb += reclen;
	}
	assert(reg_idx ==  participants);
	assert((tr_buff + tr_bufflen) == tb);
	if ((0 != tlen) || (0 != balanced))
	{	/* Bad journal records. For pro, we return. The actual error will be
		 * reported by update process */
		assert(FALSE);
		return;
	}
	reg_jrec_info_array[reg_idx - 1].end = first_tcom_offset; /* The offset of the first TCOM record will be the end of
								   * the last region */
	if (already_sorted)
	{	/* No sorting needed */
		return;
	}
	/* Records are already not sorted. N-Way merge sort required. Take a private copy of the transaction buffer */
	if (private_tr_bufflen < tr_bufflen)
	{
		if (NULL != private_tr_buff)
			free(private_tr_buff);
		private_tr_buff = malloc(tr_bufflen);
		private_tr_bufflen = tr_bufflen;
	}
	memcpy(private_tr_buff, tr_buff, tr_bufflen);
	dst_addr = tr_buff;	/* Next address in the replication pool where a sorted journal record will be copied to */
	while (TRUE)
	{
		/* Find the region that has the minimum of the update_num among all the regions that participated in this TP.
		 * At any point, 'working_offset' of a region will point to the record having the minimum update_num in that region.
		 * Find the minimum of all these update_num and the region which had this minimum update_num (min_update_num_reg).
		 * We are guranteed that this update_num would be the least of all the update_num and the record bearing this
		 * update_num can be copied to the pool.
		 * As a minor optimization, we also find the the second minimum update_num. We are guaranteed that all records
		 * whose update_num in min_update_num_reg is less than second minimum can also be copied to the pool and
		 * increment the 'working_offset' of min_update_num_reg accordingly.
		 */
		min_updnum_reg = 0;
		min_val = max_updnum + 1;
		next_min_val = min_val;
		sorting_needed = FALSE;
		for (reg_idx = 0; reg_idx < participants; reg_idx++)
		{
			if (reg_jrec_info_array[reg_idx].working_offset >= reg_jrec_info_array[reg_idx].end)
				continue;	/* All records in this region are sorted, continue */
			sorting_needed = TRUE;
			/* Extract the update_num of the current record */
			this_jrec_addr = (private_tr_buff + reg_jrec_info_array[reg_idx].working_offset);
			prefix = (jrec_prefix *)(this_jrec_addr);
			assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(prefix->jrec_type));
			assert(SIZEOF(struct_jrec_upd) == SIZEOF(struct_jrec_ztworm));
			assert(OFFSETOF(struct_jrec_upd, update_num) == OFFSETOF(struct_jrec_ztworm, update_num));
			this_reg_updnum = ((struct_jrec_upd *)(this_jrec_addr))->update_num;
			assert(this_reg_updnum < (max_updnum + 1));
			assert(min_val != this_reg_updnum);
			/* Update minimum and second miminum. */
			if (this_reg_updnum < min_val)
			{
				next_min_val = min_val;
				min_val = this_reg_updnum;
				min_updnum_reg = reg_idx;

			} else if (this_reg_updnum < next_min_val)
				next_min_val = this_reg_updnum;
		}
		if (!sorting_needed)
			break;
		assert(min_val != (max_updnum + 1)); /* There HAS to be a minimum */
		/* At this point, min_updnum_reg points to the region which has the least update_num for this iteration. */
		working_record_addr = (private_tr_buff + reg_jrec_info_array[min_updnum_reg].working_offset);
		reg_top = (private_tr_buff + reg_jrec_info_array[min_updnum_reg].end);
		copy_len = ((jrec_prefix *)(working_record_addr))->forwptr;
		/* See if we can minimize the copy by finding records in this region, whose update_num are less
		 * than second minimum.
		 */
		next_record_addr = working_record_addr + copy_len;
		while (next_record_addr < reg_top)
		{
			prefix = (jrec_prefix *)(next_record_addr);
			if (((struct_jrec_upd *)(next_record_addr))->update_num < next_min_val)
				copy_len += prefix->forwptr;
			else
				break;
			next_record_addr += prefix->forwptr;
		}
		reg_jrec_info_array[min_updnum_reg].working_offset = (next_record_addr - private_tr_buff);
		assert(dst_addr < (tr_buff + tr_bufflen));
		assert(copy_len <= tr_bufflen);
		memcpy(dst_addr, working_record_addr, copy_len);
		dst_addr += copy_len;
	}
#	ifdef DEBUG
	/* Verify that there are exactly tcom_num JRT_TCOM records */
	for (idx = 0; idx < tcom_num; idx++)
	{
		prefix = (jrec_prefix *)(dst_addr);
		assert(JRT_TCOM == prefix->jrec_type);
		dst_addr += prefix->forwptr;
	}
	assert(dst_addr == tr_buff + tr_bufflen);
#	endif
	return;
}
