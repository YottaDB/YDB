/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "min_max.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

GBLREF mur_gbls_t	murgbl;
DEBUG_ONLY(GBLREF mur_opt_struct	mur_options;)

void mur_process_seqno_table(seq_num *min_broken_seqno, seq_num *losttn_seqno)
{
	int		index;
        size_t		seq_arr_size;
	jnl_tm_t	min_time;
	seq_num		min_brkn_seqno, min_resolve_seqno, max_resolve_seqno;
	char		*seq_arr;
	multi_struct	*multi;
	ht_ent_int8	*curent, *topent;

	assert(mur_options.rollback);
	min_resolve_seqno = min_brkn_seqno = MAXUINT8;
	max_resolve_seqno = 0;
	for (curent = murgbl.token_table.base, topent = murgbl.token_table.top; curent < topent; curent++)
	{
		if ((HTENT_VALID_INT8(curent, multi_struct, multi)) && NULL != multi)
		{
			if (multi->token < min_resolve_seqno)
				min_resolve_seqno = multi->token;
			if (multi->token > max_resolve_seqno)
				max_resolve_seqno = multi->token;
			if (0 < multi->partner && multi->token < min_brkn_seqno)
				min_brkn_seqno = multi->token;	/* actually sequence number */
			assert(NULL == (multi_struct *)multi->next);
		}
	}
	if (*losttn_seqno >= min_resolve_seqno)
	{	/* Update losttn_seqno to the first seqno gap from min_resolve_seqno to max_resolve_seqno */
		assert(max_resolve_seqno >= min_resolve_seqno);
		seq_arr_size = (max_resolve_seqno - min_resolve_seqno + 1);
		/* To conserve space instead of int array char array is used. We can use bit map to save more space. */
		seq_arr = (char *) malloc(seq_arr_size);
		memset(seq_arr, 0, seq_arr_size);
		for (curent = murgbl.token_table.base, topent = murgbl.token_table.top; curent < topent; curent++)
		{
			if ((HTENT_VALID_INT8(curent, multi_struct, multi)) && NULL != multi)
				seq_arr[multi->token - min_resolve_seqno] = 1;
		}
		for (index = 0; (index < (int)seq_arr_size) && seq_arr[index]; index++)
			;
		free (seq_arr);
		*losttn_seqno = min_resolve_seqno + index;
	}
	if (*losttn_seqno > murgbl.stop_rlbk_seqno)
		*losttn_seqno = murgbl.stop_rlbk_seqno;
	if (*losttn_seqno > min_brkn_seqno)
		*losttn_seqno = min_brkn_seqno;
	*min_broken_seqno = min_brkn_seqno;
	mur_multi_rehash();	/* To release memory and shorten the table */
	return;
}
