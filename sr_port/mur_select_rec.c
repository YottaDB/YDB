/****************************************************************
 *								*
 * Copyright (c) 2003-2020 Fidelity National Information	*
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

#include "gdsroot.h"
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
#include "copy.h"
#include "error.h"
#include "min_max.h"
#include "format_targ_key.h"
#include "jnl_typedef.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "real_len.h"		/* for real_len() prototype */

GBLREF	mur_opt_struct		mur_options;
GBLREF	mur_gbls_t		murgbl;

error_def(ERR_JNLEXTRCTSEQNO);

#define INITIALIZE_FLAGS					\
	inc_seen = inc_item_seen = exc_item_seen = FALSE;

#define SET_FLAG_IF_INCLUDE_PATTERN_SEEN(PATTERN)		\
{								\
	if (!PATTERN->exclude)					\
		inc_seen = TRUE;				\
}

#define SET_INCFLAG_OR_RETURN(PATTERN)				\
{								\
	if (PATTERN->exclude)					\
		return FALSE;	/* Exclude this item */		\
	else							\
		inc_item_seen = TRUE;				\
}

#define RETURN_IF_ITEM_DOESNT_MATCH_ANY_INCLUDE_PATTERN		\
{								\
	if (inc_seen && !inc_item_seen)				\
		return FALSE;					\
}

boolean_t	mur_select_rec(jnl_ctl_list *jctl)
{
	boolean_t		exc_item_seen, inc_item_seen, inc_seen, wildcard_match, exact_match;
	char			key_buff[MAX_KEY_SZ + 1 + SIZEOF(uint4) * 2], asc_key_buff[MAX_ZWR_KEY_SZ], *ptr, *val_ptr;
	int			i, key_len, pat_pos, subs_pos;
	uint4			pini_addr;
	gv_key			*key;
	jnl_record		*rec;
	pini_list_struct	*plst;
	jnl_process_vector	*pv;
	enum jnl_record_type	rectype;
	long_list		*ll_ptr;
	long_long_list		*lll_ptr;
	select_list		*sl_ptr;
	jnl_string		*keystr;
	uint4			status;
	int4			pv_len, sl_len;
	seq_num			rec_token_seq;
	mstr			mstr_global_value;

	assert(mur_options.selection);
	rec = jctl->reg_ctl->mur_desc->jnlrec;
	rectype = (enum jnl_record_type)rec->prefix.jrec_type;
	key = NULL;
	if ((JRT_NULL == rectype) || (JRT_ALIGN == rectype))
		return TRUE;
	pini_addr = rec->prefix.pini_addr;	/* Since rectype != JRT_ALIGN, we can safely use "prefix.pini_addr" */
	status = mur_get_pini(jctl, pini_addr, &plst);
	if (SS_NORMAL != status)
		return TRUE;
	pv = &plst->jpv;
	/* Initialize the mstr */
	mstr_global_value.addr = NULL;
	mstr_global_value.len = 0;
	/* Sequence number of this record */
	if (mur_options.corruptdb)
	{
		rec_token_seq = REC_HAS_TOKEN_SEQ(rectype)
			? ((jctl->jfh && REPL_ALLOWED(jctl->jfh)) ? GET_JNL_SEQNO(rec) : rec->prefix.tn) : 0;
	} else
		rec_token_seq = REC_HAS_TOKEN_SEQ(rectype)
			? ((jctl->reg_ctl->csd && REPL_ALLOWED(jctl->reg_ctl->csd)) ? GET_JNL_SEQNO(rec) : rec->prefix.tn) : 0;
	if (IS_SET_KILL_ZKILL_ZTRIG(rectype))
	{	/* Translate internal format of jnl_record key to ascii */
		keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
		key = (gv_key *)key_buff;
		key->top = MAX_KEY_SZ;
		key->end = keystr->length;
		assert(key->end <= key->top);
		memcpy(key->base, &keystr->text[0], keystr->length);
		key->base[key->end] = '\0';
		key_len = INTCAST((format_targ_key((uchar_ptr_t)asc_key_buff, MAX_ZWR_KEY_SZ, key, FALSE) -
				   (unsigned char *)asc_key_buff));
		/* Get the value of the global, if it is a SET record */
		if (IS_SET(rectype))
		{
			val_ptr = &keystr->text[keystr->length];
			GET_MSTR_LEN(mstr_global_value.len, val_ptr);
			mstr_global_value.addr = val_ptr + SIZEOF(mstr_len_t);
		}
	}
	/* Check this record against the various selection lists */
	if (NULL != mur_options.user)
	{
		INITIALIZE_FLAGS;
		pv_len = real_len(JPV_LEN_USER, (uchar_ptr_t)pv->jpv_user);
		for (sl_ptr = mur_options.user;  NULL != sl_ptr;  sl_ptr = sl_ptr->next)
		{
			wildcard_match = FALSE;
			SET_FLAG_IF_INCLUDE_PATTERN_SEEN(sl_ptr);
			if (sl_ptr->has_wildcard)
				wildcard_match = mur_do_wildcard(pv->jpv_user, sl_ptr->buff, pv_len, sl_ptr->len);
			if (!wildcard_match)
				sl_len = MIN(sl_ptr->len, JPV_LEN_USER);
			if (wildcard_match || (pv_len == sl_len) && (0 == memcmp(pv->jpv_user, sl_ptr->buff, sl_len)))
				SET_INCFLAG_OR_RETURN(sl_ptr);
		}
		RETURN_IF_ITEM_DOESNT_MATCH_ANY_INCLUDE_PATTERN;
	}
	if ((NULL != mur_options.global) && (NULL != key))
	{
		INITIALIZE_FLAGS;
		for (sl_ptr = mur_options.global;  NULL != sl_ptr;  sl_ptr = sl_ptr->next)
		{
			wildcard_match = FALSE;
			SET_FLAG_IF_INCLUDE_PATTERN_SEEN(sl_ptr);
			if (sl_ptr->has_wildcard)
				wildcard_match = mur_do_wildcard(asc_key_buff, sl_ptr->buff, key_len, sl_ptr->len);
			i = sl_ptr->len;
			if (sl_ptr->buff[i - 1] == ')')
				--i;
			if (wildcard_match
				|| (key_len == i) && (0 == memcmp(asc_key_buff, sl_ptr->buff, i))
				|| (key_len >  i) && (0 == memcmp(asc_key_buff, sl_ptr->buff, i))
					&& (('(' == asc_key_buff[i]) || (')' == asc_key_buff[i]) || (',' == asc_key_buff[i])))
			{
					SET_INCFLAG_OR_RETURN(sl_ptr);
			}
		}
		RETURN_IF_ITEM_DOESNT_MATCH_ANY_INCLUDE_PATTERN;
	}
	if ((NULL != mur_options.patterns) && (0 != mstr_global_value.len) && (NULL != mstr_global_value.addr))
	{
		INITIALIZE_FLAGS;
		for (sl_ptr = mur_options.patterns;  NULL != sl_ptr;  sl_ptr = sl_ptr->next)
		{
			wildcard_match = exact_match = FALSE;
			SET_FLAG_IF_INCLUDE_PATTERN_SEEN(sl_ptr);
			if (sl_ptr->has_wildcard)
				wildcard_match = mur_do_wildcard(mstr_global_value.addr,
							sl_ptr->buff, mstr_global_value.len, sl_ptr->len);
			else
				exact_match = ((mstr_global_value.len == sl_ptr->len) &&
						(0 == memcmp(mstr_global_value.addr, sl_ptr->buff, sl_ptr->len)));
			if (exact_match || wildcard_match)
				SET_INCFLAG_OR_RETURN(sl_ptr);
		}
		RETURN_IF_ITEM_DOESNT_MATCH_ANY_INCLUDE_PATTERN;
	}
	if (NULL != mur_options.process)
	{
		INITIALIZE_FLAGS;
		for (sl_ptr = mur_options.process;  NULL != sl_ptr;  sl_ptr = sl_ptr->next)
		{
			wildcard_match = FALSE;
			SET_FLAG_IF_INCLUDE_PATTERN_SEEN(sl_ptr);
			if (sl_ptr->has_wildcard)
				wildcard_match = mur_do_wildcard(pv->jpv_prcnam, sl_ptr->buff, JPV_LEN_PRCNAM, sl_ptr->len);
			if (!wildcard_match)
			{
				pv_len = real_len(JPV_LEN_PRCNAM, (uchar_ptr_t)pv->jpv_prcnam);
				sl_len = MIN(sl_ptr->len, JPV_LEN_PRCNAM);
			}
			if (wildcard_match || (pv_len == sl_len) && (0 == memcmp(pv->jpv_prcnam, sl_ptr->buff, sl_len)))
				SET_INCFLAG_OR_RETURN(sl_ptr);
		}
		RETURN_IF_ITEM_DOESNT_MATCH_ANY_INCLUDE_PATTERN;
	}
	if ((NULL != mur_options.seqno) && (0 != rec_token_seq))
	{
		INITIALIZE_FLAGS;
		for (lll_ptr = mur_options.seqno;  NULL != lll_ptr;  lll_ptr = lll_ptr->next)
		{
			SET_FLAG_IF_INCLUDE_PATTERN_SEEN(lll_ptr);
			if (lll_ptr->u.seqno == rec_token_seq)
				SET_INCFLAG_OR_RETURN(lll_ptr);
		}
		RETURN_IF_ITEM_DOESNT_MATCH_ANY_INCLUDE_PATTERN;
	}
	if (NULL != mur_options.id)
	{
		INITIALIZE_FLAGS;
		for (ll_ptr = mur_options.id;  NULL != ll_ptr;  ll_ptr = ll_ptr->next)
		{
			SET_FLAG_IF_INCLUDE_PATTERN_SEEN(ll_ptr);
			if (ll_ptr->num == pv->jpv_pid)
				SET_INCFLAG_OR_RETURN(ll_ptr);
		}
		RETURN_IF_ITEM_DOESNT_MATCH_ANY_INCLUDE_PATTERN;
	}
	if (IS_SET_KILL_ZKILL_ZTRIG(rectype))
	{
		if (IS_SET(rectype))
			return mur_options.transaction != TRANS_KILLS;
		else
			return mur_options.transaction != TRANS_SETS;
	}
	return TRUE;
}
