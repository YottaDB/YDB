/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"

GBLREF	boolean_t	write_ztworm_jnl_rec;
GBLREF	jnl_gbls_t	jgbl;
GBLREF	mval		dollar_ztwormhole;

/* This function removes an already formatted ZTWORMHOLE record "ztworm_jfb" from the formatted list of journal records
 * for the region corresponding to the "sgm_info" structure pointed to by "si".
 */
void jnl_format_ztworm_remove_if_needed(jnl_format_buffer *ztworm_jfb, struct sgm_info_struct *si)
{
	boolean_t	remove_ztworm_jfb;

	assert(NULL != ztworm_jfb);
	if (!write_ztworm_jnl_rec || !dollar_ztwormhole.str.len)
	{ 	/* $ZTWORMHOLE is empty at the end of the trigger invocation OR it was never read/set inside
		 * the trigger. We need to remove the corresponding formatted ZTWORMHOLE journal record.
		 */
		remove_ztworm_jfb = TRUE;
		jgbl.prev_ztworm_ptr = NULL;
	} else
	{	/* $ZTWORMHOLE was not an empty string at the end of trigger invocation AND it was used inside
		 * the trigger. Check if $ZTWORMHOLE value BEFORE the trigger invocation is identical to the
		 * value AFTER the trigger invocation.
		 */
		mstr	pre_trig_str;

		pre_trig_str.len = (*(jnl_str_len_t *)jgbl.pre_trig_ztworm_ptr);
		pre_trig_str.addr = (char *)(jgbl.pre_trig_ztworm_ptr + SIZEOF(jnl_str_len_t));
		if ((pre_trig_str.len == dollar_ztwormhole.str.len)
			&& !memcmp(pre_trig_str.addr, dollar_ztwormhole.str.addr, pre_trig_str.len))
		{	/* $ZTWORMHOLE value is identical BEFORE and AFTER trigger invocation. So no changes
			 * needed to the already formatted journal record. But check if this is the same
			 * value as the previous formatted $ZTWORMHOLE journal record. If so, this one can
			 * be removed from the jnl format list (helps avoid unnecessary ZTWORM record in
			 * journal file).
			 */
			unsigned char	*prev_ztworm_ptr;

			prev_ztworm_ptr = jgbl.prev_ztworm_ptr;
			if (NULL != prev_ztworm_ptr)
			{
				mstr	prev_ztworm_str;

				prev_ztworm_str.len = (*(jnl_str_len_t *)prev_ztworm_ptr);
				prev_ztworm_str.addr = (char *)(prev_ztworm_ptr + SIZEOF(jnl_str_len_t));
				remove_ztworm_jfb = ((pre_trig_str.len == prev_ztworm_str.len)
							&& !memcmp(pre_trig_str.addr, prev_ztworm_str.addr,
									pre_trig_str.len));
			} else
			{	/* NULL previous ZTWORMHOLE so treat current value as different */
				remove_ztworm_jfb = FALSE;
			}
			if (!remove_ztworm_jfb)
			{	/* $ZTWORMHOLE value before trigger invocation matches after trigger invocation
				 * value but is different from the previous logical record $ZTWORMHOLE value.
				 * So we cannot remove this formatted ztwormhole record but no changes needed
				 * to already formatted ZTWORMHOLE journal record.
				 */
				jgbl.prev_ztworm_ptr = jgbl.pre_trig_ztworm_ptr;
			} else
			{	/* $ZTWORMHOLE value before trigger invocation matches after trigger invocation
				 * value and is the same as the previous logical record $ZTWORMHOLE value.
				 * Therefore jgbl.prev_ztworm_ptr is already accurate. No need of any changes.
				 */
			}
		} else
		{	/* $ZTWORMHOLE value BEFORE trigger invocation is different from that AFTER trigger
			 * invocation. Need to reformat the ZTWORMHOLE journal record to reflect new value.
			 * We invoke "jnl_format()" but use a special "JNL_ZTWORM_POST_TRIG" opcode to indicate
			 * the reformat below. And pass "ztworm_jfb" as the 2nd parameter that is normally
			 * reserved to pass the "key" (which is anyways NULL in case of ZTWORMHOLE record).
			 */
			ztworm_jfb = jnl_format(JNL_ZTWORM_POST_TRIG, (gv_key *)ztworm_jfb, &dollar_ztwormhole, 0);
			remove_ztworm_jfb = FALSE;
			/* Note: jgbl.prev_ztworm_ptr is updated inside "jnl_format" in this case. */
		}
	}
	if (remove_ztworm_jfb)
	{
		/* We dont free up the memory occupied by ztworm_jfb as it is not easy to free up memory in
		 * the middle of a buddy list. This memory will anyway be freed up eventually at tp_clean_up.
		 */
		jnl_format_buffer	*jfb_prev, *jfb_next;

		jfb_prev = ztworm_jfb->prev;
		jfb_next = ztworm_jfb->next;
		if (NULL != jfb_prev)
		{
			jfb_prev->next = jfb_next;
			jfb_next->prev = jfb_prev;
		} else
		{
			assert(si->jnl_head == ztworm_jfb);
			si->jnl_head = jfb_next;
			assert(IS_UUPD(jfb_next->rectype));
			assert(((jnl_record *)jfb_next->buff)->prefix.jrec_type == jfb_next->rectype);
			jfb_next->rectype--;
			assert(IS_TUPD(jfb_next->rectype));
			((jnl_record *)jfb_next->buff)->prefix.jrec_type = jfb_next->rectype;
		}
		assert(0 < jgbl.cumul_index);
		DEBUG_ONLY(jgbl.cumul_index--;)
		jgbl.cumul_jnl_rec_len -= ztworm_jfb->record_size;
	}
}
