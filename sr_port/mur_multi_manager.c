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

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "relqop.h"

GBLREF	seq_num		max_reg_seqno;
GBLREF	seq_num		seq_num_minus_one;
GBLREF	mur_opt_struct	mur_options;

GBLDEF	buddy_list	*mur_multi_list;
GBLDEF	que_ent		mur_multi_broken_que_head;
GBLDEF	htab_desc	mur_multi_token_hashtable;
GBLDEF	htab_desc	mur_multi_seqno_hashtable;

typedef struct
{
	que_ent			broken_que;	/* should be the first field in this structure */
	int4			epid;
	token_num		token;
	seq_num			rlbk_seqno;
	int4			count;
	uint4			look_back_time;
	struct multi_struct	*next;
} multi_struct;

void	mur_multi_initialize(void)
{
	mur_multi_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(mur_multi_list, sizeof(multi_struct), MUR_MULTI_LIST_INIT_ALLOC);
	if (mur_options.rollback)
		ht_init(&mur_multi_seqno_hashtable, MUR_MULTI_SEQNO_HASHTABLE_INIT_ELEMS);
	else
		ht_init(&mur_multi_token_hashtable, MUR_MULTI_TOKEN_HASHTABLE_INIT_ELEMS);
}


void	mur_cre_multi(int4 epid, token_num token, int4 count, uint4 lookback_time, seq_num seqno)
{
	multi_struct	*multi;
	char		dummy;
	ht_entry	*hentry;


	assert(count > 0);

	multi = (multi_struct *)get_new_element(mur_multi_list, 1);
	insqh(&multi->broken_que, &mur_multi_broken_que_head);
	if (!mur_options.rollback)
	{
		multi->epid = epid;
		QWASSIGN(multi->token, token);
		multi->look_back_time = lookback_time;
	} else
		QWASSIGN(multi->rlbk_seqno, seqno);
	multi->count = count;
	multi->next = NULL;
	if (mur_options.rollback)
		hentry = ht_put(&mur_multi_seqno_hashtable, (mname *)&seqno, &dummy);
	else
		hentry = ht_put(&mur_multi_token_hashtable, (mname *)&token, &dummy);
	/* In case we have a dummy token, maintain a list of PID and token combination */
	if (!dummy && hentry->ptr)
		multi->next = (struct multi_struct *)hentry->ptr;
	hentry->ptr = (char *)multi;
}

bool	mur_lookup_multi(ctl_list *ctl, uint4 pini, token_num token, seq_num seqno)
{
	multi_struct		*multi;
	jnl_process_vector	*pv;
	ht_entry		*hentry;



	if ((pv = mur_get_pini_jpv(ctl, pini)))
	{
		if (!mur_options.rollback)
			hentry = ht_get(&mur_multi_token_hashtable, (mname *)&token);
		else
			hentry = ht_get(&mur_multi_seqno_hashtable, (mname *)&seqno);
		if (hentry)
		{
			if (!mur_options.rollback)
			{
				for (multi = (multi_struct *)hentry->ptr; (NULL != multi) &&
					(multi->epid != pv->jpv_pid); multi = (multi_struct *)multi->next)
						;
			}
			else
				multi = (multi_struct *)hentry->ptr;
		} else
			multi = NULL;
		if (multi && multi->count && (mur_options.rollback ? QWEQ(multi->rlbk_seqno, seqno) : (multi->epid == pv->jpv_pid &&
				QWEQ(multi->token, token))))
			return TRUE;
    	}
	return FALSE;
}

int4	mur_decrement_multi(int4 epid, token_num token, seq_num seqno)
{
	multi_struct	*multi;
	int4		count;
	ht_entry	*hentry;


	if (mur_options.rollback)
		hentry = ht_get(&mur_multi_seqno_hashtable, (mname *)&seqno);
	else
		hentry = ht_get(&mur_multi_token_hashtable, (mname *)&token);
	if (hentry)
	{
		if (!mur_options.rollback)
		{
			for (multi = (multi_struct *)hentry->ptr; (NULL != multi) && (multi->epid != epid);
				multi = (multi_struct *)multi->next)
				;
		}
		else
			multi = (multi_struct *)hentry->ptr;
	} else
		multi = NULL;

	if ((NULL != multi))
	{
		if ((mur_options.rollback ? (QWEQ(multi->rlbk_seqno, seqno)) : (QWEQ(multi->token, token) && multi->epid == epid)))
		{
			if (0 == (count = --multi->count))
				remqh((que_ent_ptr_t)((sm_uc_ptr_t)&multi->broken_que + multi->broken_que.bl));
		}
		return count;
	}
	return -1;
}

bool	mur_multi_extant(void)
{
	return (0 != mur_multi_broken_que_head.fl);
}

bool	mur_multi_missing(int4 epid)
{
	multi_struct	*multi;
	que_ent_ptr_t	head;

	head = (que_ent_ptr_t)&mur_multi_broken_que_head;
	for (multi = (multi_struct *)((sm_uc_ptr_t)head + head->fl);
			(que_ent_ptr_t)multi != head;
					multi = (multi_struct *)((sm_uc_ptr_t)multi + multi->broken_que.fl))
	{
		if (multi->epid == epid)
			return TRUE;
	}
	return FALSE;
}

uint4	mur_lookup_lookback_time(void)
{
	multi_struct	*multi;
	que_ent_ptr_t	head;
	uint4		min_time;

	min_time = -1;
	head = (que_ent_ptr_t)&mur_multi_broken_que_head;

	for (multi = (multi_struct *)((sm_uc_ptr_t)head + head->fl);  (que_ent_ptr_t)multi != head;
					multi = (multi_struct *)((sm_uc_ptr_t)multi + multi->broken_que.fl))
	{
		if (multi->look_back_time < min_time)
			min_time = multi->look_back_time;
	}
	return min_time;
}

seq_num	rlbk_lookup_seqno(void)
{
	multi_struct	*multi;
	que_ent_ptr_t	head;
	seq_num		min_seqno;

	QWASSIGN(min_seqno, seq_num_minus_one);
	head = (que_ent_ptr_t)&mur_multi_broken_que_head;

	for (multi = (multi_struct *)((sm_uc_ptr_t)head + head->fl);  (que_ent_ptr_t)multi != head;
					multi = (multi_struct *)((sm_uc_ptr_t)multi + multi->broken_que.fl))
	{
		if (QWLT(multi->rlbk_seqno, min_seqno))
			QWASSIGN(min_seqno, multi->rlbk_seqno);
	}
	return min_seqno;
}
