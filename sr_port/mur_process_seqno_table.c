/****************************************************************
 *								*
 *	Copyright 2003, 2004 Sanchez Computer Associates, Inc.	*
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
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"

GBLREF mur_gbls_t	murgbl;
DEBUG_ONLY(GBLREF mur_opt_struct	mur_options;)

void mur_process_seqno_table(seq_num *min_broken_seqno, seq_num *losttn_seqno)
{
	int		index, seq_arr_size;
	jnl_tm_t	min_time;
	seq_num		min_brkn_seqno, min_resolve_seqno, max_resolve_seqno;
	char		*seq_arr;
	multi_struct	*multi;
	ht_entry	*table_base;

	assert(mur_options.rollback);
	table_base = murgbl.token_table.base;
	min_resolve_seqno = min_brkn_seqno = MAXUINT8;
	max_resolve_seqno = 0;
	for (index = 0; index < murgbl.token_table.size; index++)
	{
		multi = (multi_struct *)table_base[index].ptr;
		if (NULL != multi)
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
		seq_arr_size = max_resolve_seqno - min_resolve_seqno + 1;
		/* To conserve space instead of int array char array is used. We can use bit map to save more space. */
		seq_arr = (char *) malloc(seq_arr_size);
		memset(seq_arr, 0, seq_arr_size);
		for (index = 0; index < murgbl.token_table.size; index++)
		{
			multi = (multi_struct *)table_base[index].ptr;
			if (NULL != multi)
				seq_arr[multi->token - min_resolve_seqno] = 1;
		}
		for (index = 0; index < seq_arr_size && seq_arr[index]; index++)
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
