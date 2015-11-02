/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
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
#include "min_max.h"
#include "format_targ_key.h"
#include "jnl_typedef.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "real_len.h"		/* for real_len() prototype */

GBLREF	mur_opt_struct	mur_options;

boolean_t	mur_select_rec(jnl_ctl_list *jctl)
{
	boolean_t		exc_item_seen, inc_item_seen, inc_seen, wildcard_match;
	char			key_buff[MAX_KEY_SZ + 1 + SIZEOF(uint4) * 2], asc_key_buff[MAX_ZWR_KEY_SZ], *ptr;
	int			i, key_len, pat_pos, subs_pos;
	uint4			pini_addr;
	gv_key			*key;
	jnl_record		*rec;
	pini_list_struct	*plst;
	jnl_process_vector	*pv;
	enum jnl_record_type	rectype;
	long_list		*ll_ptr;
	select_list		*sl_ptr;
	jnl_string		*keystr;
	uint4			status;
	int4			pv_len, sl_len;

	assert(mur_options.selection);
	rec = jctl->reg_ctl->mur_desc->jnlrec;
	rectype = (enum jnl_record_type)rec->prefix.jrec_type;
	pini_addr = rec->prefix.pini_addr;
	key = NULL;
	if (JRT_NULL == rectype || JRT_ALIGN == rectype)
		return TRUE;
	status = mur_get_pini(jctl, pini_addr, &plst);
	if (SS_NORMAL != status)
		return TRUE;
	pv = &plst->jpv;
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
	}
	/* Check this record against the various selection lists */
	if (NULL != mur_options.user)
	{
		inc_seen = inc_item_seen = exc_item_seen = FALSE;
		for (sl_ptr = mur_options.user;  NULL != sl_ptr;  sl_ptr = sl_ptr->next)
		{
			wildcard_match = FALSE;
			if (!sl_ptr->exclude)
				inc_seen = TRUE;
			if (sl_ptr->has_wildcard)
				wildcard_match = mur_do_wildcard(pv->jpv_user, sl_ptr->buff, JPV_LEN_USER, sl_ptr->len);
			if (!wildcard_match)
			{
				pv_len = real_len(JPV_LEN_USER, (uchar_ptr_t)pv->jpv_user);
				sl_len = MIN(sl_ptr->len, JPV_LEN_USER);
			}
			if (wildcard_match || (pv_len == sl_len) && (0 == memcmp(pv->jpv_user, sl_ptr->buff, sl_len)))
			{
				if (sl_ptr->exclude)
					exc_item_seen = TRUE;
				else
					inc_item_seen = TRUE;
			}
		}
		if (exc_item_seen || (inc_seen && !inc_item_seen))
			return FALSE;
	}
	if ((NULL != mur_options.global) && (NULL != key))
	{
		inc_seen = inc_item_seen = exc_item_seen = FALSE;
		for (sl_ptr = mur_options.global;  NULL != sl_ptr;  sl_ptr = sl_ptr->next)
		{
			wildcard_match = FALSE;
			if (!sl_ptr->exclude)
				inc_seen = TRUE;
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
				if (sl_ptr->exclude)
					exc_item_seen = TRUE;
				else
					inc_item_seen = TRUE;
			}
		}
		if (exc_item_seen || (inc_seen && !inc_item_seen))
			return FALSE;
	}
	if (NULL != mur_options.process)
	{
		inc_seen = inc_item_seen = exc_item_seen = FALSE;
		for (sl_ptr = mur_options.process;  NULL != sl_ptr;  sl_ptr = sl_ptr->next)
		{
			wildcard_match = FALSE;
			if (!sl_ptr->exclude)
				inc_seen = TRUE;
			if (sl_ptr->has_wildcard)
				wildcard_match = mur_do_wildcard(pv->jpv_prcnam, sl_ptr->buff, JPV_LEN_PRCNAM, sl_ptr->len);
			if (!wildcard_match)
			{
				pv_len = real_len(JPV_LEN_PRCNAM, (uchar_ptr_t)pv->jpv_prcnam);
				sl_len = MIN(sl_ptr->len, JPV_LEN_PRCNAM);
			}
			if (wildcard_match || (pv_len == sl_len) && (0 == memcmp(pv->jpv_prcnam, sl_ptr->buff, sl_len)))
			{
				if (sl_ptr->exclude)
					exc_item_seen = TRUE;
				else
					inc_item_seen = TRUE;
			}
		}
		if (exc_item_seen || (inc_seen && !inc_item_seen))
			return FALSE;
	}
	if (NULL != mur_options.id)
	{
		inc_seen = inc_item_seen = exc_item_seen = FALSE;
		for (ll_ptr = mur_options.id;  NULL != ll_ptr;  ll_ptr = ll_ptr->next)
		{
			if (!ll_ptr->exclude)
				inc_seen = TRUE;
			if (ll_ptr->num == pv->jpv_pid)
			{
				if (ll_ptr->exclude)
					exc_item_seen = TRUE;
				else
					inc_item_seen = TRUE;
			}
		}
		if (exc_item_seen || (inc_seen && !inc_item_seen))
			return FALSE;
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
