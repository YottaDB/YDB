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

GBLDEF	buddy_list	*mur_multi_list;
GBLDEF	que_ent		mur_multi_broken_que_head;
GBLDEF	htab_desc	mur_multi_token_hashtable;

typedef struct
{
	que_ent		broken_que;	/* should be the first field in this structure */
	int4		epid;
	token_num	token;
	int4		count;
	uint4		look_back_time;
} multi_struct;

struct multi_seqno_struct
{
	struct multi_seqno_struct	*next;
	seq_num				rlbk_seqno;
	int4				rlbk_count;
};

struct multi_seqno_struct	*multi_seqno;

void	mur_multi_initialize(void)
{
	mur_multi_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(mur_multi_list, sizeof(multi_struct), MUR_MULTI_LIST_INIT_ALLOC);
	ht_init(&mur_multi_token_hashtable, MUR_MULTI_TOKEN_HASHTABLE_INIT_ELEMS);
}

void	mur_cre_multi_seqno(int4 count, seq_num seqno)
{
	struct multi_seqno_struct	*ms;

	assert(count > 0);

	ms = (struct multi_seqno_struct *)malloc(sizeof(struct multi_seqno_struct));
	ms->next = multi_seqno;
	multi_seqno = ms;

	ms->rlbk_count = count;
	ms->rlbk_seqno = seqno;
}

void	mur_cre_multi(int4 epid, token_num token, int4 count, uint4 lookback_time)
{
	multi_struct	*m;
	char		dummy;
	ht_entry	*h;


	assert(count > 0);

	m = (multi_struct *)get_new_element(mur_multi_list, 1);
	insqh(&m->broken_que, &mur_multi_broken_que_head);
	m->epid = epid;
	QWASSIGN(m->token, token);
	m->count = count;
	m->look_back_time = lookback_time;
	h = ht_put(&mur_multi_token_hashtable, (mname *)&token, &dummy);
	h->ptr = (char *)m;
	if (!dummy && h->ptr)
		assert(FALSE);
}

bool	mur_lookup_multi(ctl_list *ctl, uint4 pini, token_num token)
{
	multi_struct		*m;
	jnl_process_vector	*pv;
	ht_entry		*h;



	if ((pv = mur_get_pini_jpv(ctl, pini)))
	{
		h = ht_get(&mur_multi_token_hashtable, (mname *)&token);
		if (h)
			m = (multi_struct *)h->ptr;
		else
			m = NULL;
		if (m && m->count && m->epid == pv->jpv_pid && QWEQ(m->token, token))
			return TRUE;
    	}
	return FALSE;
}

int4	mur_decrement_multi_seqno(seq_num seqno)
{
	struct multi_seqno_struct	*ms, *prevms;
	int4				count;

	for (ms = multi_seqno, prevms = NULL;  ms != NULL;  prevms = ms, ms = ms->next)
	{
		if (QWEQ(ms->rlbk_seqno, seqno))
		{
			if ((count = --ms->rlbk_count) == 0)
			{
				/* Delete this ectry */
				if (prevms == NULL)
					multi_seqno = ms->next;
				else
					prevms->next = ms->next;

				free(ms);
			}
			return count;
		}
	}
	return -1;
}

int4	mur_decrement_multi(int4 epid, token_num token)
{
	multi_struct	*m, *prev;
	int4		count;
	ht_entry	*h;


	h = ht_get(&mur_multi_token_hashtable, (mname *)&token);
	if (h)
		m = (multi_struct *)h->ptr;
	else
		m = NULL;
	if ((NULL != m)&& (QWEQ(m->token, token) && m->epid == epid))
	{
		assert(QWEQ(m->token, token) && m->epid == epid);
		if (0 == (count = --m->count))
			remqh((que_ent_ptr_t)((sm_uc_ptr_t)&m->broken_que + m->broken_que.bl));
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
	multi_struct	*m;
	que_ent_ptr_t	head;

	head = (que_ent_ptr_t)&mur_multi_broken_que_head;
	for (m = (multi_struct *)((sm_uc_ptr_t)head + head->fl);
			(que_ent_ptr_t)m != head;
					m = (multi_struct *)((sm_uc_ptr_t)m + m->broken_que.fl))
	{
		if (m->epid == epid)
			return TRUE;
	}
	return FALSE;
}

seq_num	rlbk_lookup_seqno(void)
{
	struct multi_seqno_struct	*m;
	seq_num			min_seqno;

	QWASSIGN(min_seqno, seq_num_minus_one);

	for (m = multi_seqno;  m != NULL;  m = m->next)
	{
		if (QWLT(m->rlbk_seqno, min_seqno))
			QWASSIGN(min_seqno, m->rlbk_seqno);
	}
	return min_seqno;
}

uint4	mur_lookup_lookback_time(void)
{
	multi_struct	*m;
	que_ent_ptr_t	head;
	uint4		min_time;

	min_time = -1;
	head = (que_ent_ptr_t)&mur_multi_broken_que_head;

	for (m = (multi_struct *)((sm_uc_ptr_t)head + head->fl);  (que_ent_ptr_t)m != head;
					m = (multi_struct *)((sm_uc_ptr_t)m + m->broken_que.fl))
	{
		if (m->look_back_time < min_time)
			min_time = m->look_back_time;
	}
	return min_time;
}
