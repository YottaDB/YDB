/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information		*
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
#include "gtm_stdio.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "buddy_list.h"
#include "jnl.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "io.h"
#include "io_params.h"
#include "op.h"
#include "gtm_multi_proc.h"

GBLREF	mur_gbls_t	murgbl;

void jnlext_merge_sort_prepare(jnl_ctl_list *jctl, jnl_record *rec, enum broken_type recstat, int length)
{
	reg_ctl_list		*rctl;
	enum jnl_record_type	rectype;
	jnlext_multi_t		*jext_rec;
	boolean_t		is_logical_rec, need_new_elem;
	forw_multi_struct	*forw_multi;
	buddy_list		*list;

	rctl = jctl->reg_ctl;
	assert(1 < murgbl.reg_total);	/* caller should have ensured this */
	jext_rec = rctl->last_jext_rec[recstat];
	if (NULL != rec)
	{
		assert(rec == rctl->mur_desc->jnlrec);
		rectype = (enum jnl_record_type)rec->prefix.jrec_type;
		is_logical_rec = (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype) || IS_COM(rectype));
		need_new_elem = (is_logical_rec || (NULL == jext_rec) || rctl->last_jext_logical_rec[recstat]
				|| (jext_rec->time != rec->prefix.time));
		assert(need_new_elem || (NULL != rctl->jnlext_multi_list[recstat]));
	} else
		need_new_elem = FALSE;
	if (need_new_elem)
	{
		list = rctl->jnlext_multi_list[recstat];
		if (NULL == list)
		{
			list = (buddy_list *)malloc(SIZEOF(buddy_list));
			initialize_list(list, SIZEOF(jnlext_multi_t), MUR_JNLEXT_LIST_INIT_ELEMS);
			rctl->jnlext_multi_list[recstat] = list;
		}
		jext_rec = (jnlext_multi_t *)get_new_element(list, 1);
		rctl->jnlext_multi_list_size[recstat]++;
		jext_rec->time = rec->prefix.time;
		if (is_logical_rec)
		{
			jext_rec->token_seq = rec->jrec_set_kill.token_seq;
			if (!IS_COM(rectype))
				jext_rec->update_num = rec->jrec_set_kill.update_num * 2 + (IS_ZTWORM(rectype) ? 0 : 1);
			else
				jext_rec->update_num = MAXUINT4;
			forw_multi = rctl->forw_multi;
			if ((NULL != forw_multi) && (repl_closed == rctl->repl_state))
			{
				assert(!IS_TUPD(rectype)
					|| (1 < rec->jrec_set_kill.num_participants)
						&& (forw_multi->num_reg_seen_backward
							<= rec->jrec_set_kill.num_participants));
				assert(!IS_COM(rectype)
					|| (forw_multi->num_reg_seen_backward <= rec->jrec_tcom.num_participants));
				jext_rec->num_more_reg = IS_TUPD(rectype) ? forw_multi->num_reg_seen_backward - 1 : 0;
			} else
				jext_rec->num_more_reg = 0;
		} else
		{
			jext_rec->token_seq.token = 0;
			jext_rec->update_num = 0;
			jext_rec->num_more_reg = 0;
		}
		jext_rec->size = length;
		rctl->last_jext_rec[recstat] = jext_rec; /* Store this for next call to "jnlext_write" in case of need */
		rctl->last_jext_logical_rec[recstat] = is_logical_rec;
	} else
	{
		assert(rctl->jnlext_multi_list_size[recstat]);
		assert(jext_rec == (jnlext_multi_t *)find_element(rctl->jnlext_multi_list[recstat],
									rctl->jnlext_multi_list_size[recstat] - 1));
		jext_rec->size += length; /* Tag this record along with previous extract record */
	}
#	ifdef MUR_DEBUG
	jext_rec = rctl->last_jext_rec[recstat];
	fprintf(stderr, "%s : list size = %d : time = %d : token_seq = %lld : update_num = %u : num_reg = %d : "
			"size = %lld\n", rctl->gd->rname, rctl->jnlext_multi_list_size[recstat],
			jext_rec->time,
			(long long int)jext_rec->token_seq.token, jext_rec->update_num, jext_rec->num_more_reg,
			(long long int)jext_rec->size);
#	endif
}
