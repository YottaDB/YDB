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
#include "copy.h"
#include "min_max.h"
#include "format_targ_key.h"


GBLREF	mur_opt_struct	mur_options;


bool	mur_do_record(ctl_list *ctl)
{
	bool			exc_item_seen, inc_item_seen, inc_seen, subs_match, wildcard_match;
	char			t_key_temp_buffer[MAX_KEY_SZ + 1 + sizeof(uint4) * 2], key_buffer[512];
	char			*t_key_buffer;
	int			i, count, key_len, exc_pos, inc_pos, pat_pos, subs_pos;
	gv_key			*t_key;
	jnl_record		*rec;
	jnl_process_vector	*pv;
	enum jnl_record_type	rectype;
	long_list		*ll_ptr;
	select_list		*sl_ptr;


	assert(mur_options.selection);

	rec = (jnl_record *)ctl->rab->recbuff;
	t_key = NULL;
	t_key_buffer = (char *)ROUND_UP((int)(&t_key_temp_buffer[0]), sizeof(uint4) * 2);
	switch (rectype = REF_CHAR(&rec->jrec_type))
	{
	case JRT_NULL:
	case JRT_ALIGN:

		assert(FALSE);
		return TRUE;

	case JRT_PBLK:
	case JRT_EPOCH:
	case JRT_TCOM:
	case JRT_ZTCOM:

		assert(&rec->val.jrec_pblk.pini_addr == &rec->val.jrec_epoch.pini_addr);
		assert(&rec->val.jrec_pblk.pini_addr == &rec->val.jrec_tcom.pini_addr);
		assert(&rec->val.jrec_pblk.pini_addr == &rec->val.jrec_ztcom.pini_addr);

		if ((pv = mur_get_pini_jpv(ctl, rec->val.jrec_pblk.pini_addr)) == NULL)
			return TRUE;

		break;


	case JRT_PINI:
	case JRT_PFIN:
	case JRT_EOF:

		assert(&rec->val.jrec_pini.process_vector == &rec->val.jrec_pfin.process_vector);
		assert(&rec->val.jrec_pini.process_vector == &rec->val.jrec_eof.process_vector);

		pv = &rec->val.jrec_pini.process_vector;

		break;


	case JRT_SET:
	case JRT_KILL:
	case JRT_ZKILL:

		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_kill.pini_addr);
		assert(&rec->val.jrec_set.mumps_node == &rec->val.jrec_kill.mumps_node);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_zkill.pini_addr);
		assert(&rec->val.jrec_set.mumps_node == &rec->val.jrec_zkill.mumps_node);

		if ((pv = mur_get_pini_jpv(ctl, rec->val.jrec_set.pini_addr)) == NULL)
			return TRUE;

		/* Translate internal format of jnl_record key to ascii */
		t_key = (gv_key *)t_key_buffer;
		t_key->top = 255;
		t_key->end = rec->val.jrec_set.mumps_node.length;
		memcpy(t_key->base, rec->val.jrec_set.mumps_node.text, t_key->end);
		t_key->base[t_key->end] = '\0';
		key_len = format_targ_key((uchar_ptr_t)key_buffer, sizeof(key_buffer), t_key, FALSE) - (unsigned char *)key_buffer;

		break;


	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:

		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_gset.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_tset.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_uset.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_fkill.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_gkill.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_tkill.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_ukill.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_fzkill.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_gzkill.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_tzkill.pini_addr);
		assert(&rec->val.jrec_fset.pini_addr == &rec->val.jrec_uzkill.pini_addr);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_gset.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_tset.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_uset.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_fkill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_gkill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_tkill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_ukill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_fzkill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_gzkill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_tzkill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_uzkill.mumps_node);

		if ((pv = mur_get_pini_jpv(ctl, rec->val.jrec_fset.pini_addr)) == NULL)
			return TRUE;

		/* Translate internal format of jnl_record key to ascii */
		t_key = (gv_key *)t_key_buffer;
		t_key->top = 255;
		t_key->end = rec->val.jrec_fset.mumps_node.length;
		memcpy(t_key->base, rec->val.jrec_fset.mumps_node.text, t_key->end);
		t_key->base[t_key->end] = '\0';
		key_len = format_targ_key((uchar_ptr_t)key_buffer, sizeof(key_buffer), t_key, FALSE) - (unsigned char *)key_buffer;
	}


	/* Check this record against the various selection lists */

	if (mur_options.user != NULL)
	{
		inc_seen = inc_item_seen
			 = exc_item_seen
			 = wildcard_match
			 = FALSE;

		for (sl_ptr = mur_options.user, count = 0;  sl_ptr != NULL;  sl_ptr = sl_ptr->next, ++count)
		{
			if (!sl_ptr->exclude)
				inc_seen = TRUE;

			for (i = 0;  i < sl_ptr->len;  ++i)
				if (sl_ptr->buff[i] == '*'  ||  sl_ptr->buff[i] == '%')
				{
					wildcard_match = mur_do_wildcard(pv->jpv_user, sl_ptr->buff, JPV_LEN_USER, sl_ptr->len);
					break;
				}

			if (wildcard_match  ||  memcmp(pv->jpv_user, sl_ptr->buff, MIN(sl_ptr->len, JPV_LEN_USER)) == 0)
				if (sl_ptr->exclude)
				{
					exc_item_seen = TRUE;
					exc_pos = count;
				}
				else
				{
					inc_item_seen = TRUE;
					inc_pos = count;
				}
		}

		if (exc_item_seen  &&  !inc_item_seen  ||
		    inc_seen  &&  !inc_item_seen  ||
		    exc_item_seen  &&  inc_item_seen  &&  exc_pos > inc_pos)
			return FALSE;
	}


	if (mur_options.global != NULL  &&  t_key != NULL)
	{
		inc_seen = inc_item_seen
			 = exc_item_seen
			 = wildcard_match
			 = subs_match
			 = FALSE;

		for (sl_ptr = mur_options.global, count = 0;  sl_ptr != NULL;  sl_ptr = sl_ptr->next, ++count)
		{
			if (!sl_ptr->exclude)
				inc_seen = TRUE;

			for (i = 0;  i < sl_ptr->len;  ++i)
				if (sl_ptr->buff[i] == '*'  ||  sl_ptr->buff[i] == '%')
				{
					wildcard_match = mur_do_wildcard(key_buffer, sl_ptr->buff, key_len, sl_ptr->len);
					break;
				}

			if (wildcard_match)
			{
				pat_pos = subs_pos
					= 0;

				for (i = 0;  i < key_len;  ++i)
					if (key_buffer[i] == '(')
					{
						subs_pos = i;
						break;
					}

				for (i = 0;  i < sl_ptr->len;  ++i)
					if (sl_ptr->buff[i] == '(')
					{
						pat_pos = i;
						break;
					}

				subs_match = pat_pos == 0  ||
					     (key_len - subs_pos >= sl_ptr->len - pat_pos  &&
					      memcmp(&key_buffer[subs_pos], &sl_ptr->buff[pat_pos], sl_ptr->len - pat_pos - 1)
						== 0);
			}

			i = sl_ptr->len;
			if (sl_ptr->buff[i - 1] == ')')
				--i;

			if (wildcard_match  &&  subs_match  ||
			    key_len == i &&  memcmp(key_buffer, sl_ptr->buff, i) == 0)
				if (sl_ptr->exclude)
				{
					exc_item_seen = TRUE;
					exc_pos = count;
				}
				else
				{
					inc_item_seen = TRUE;
					inc_pos = count;
				}
		}

		if (exc_item_seen  &&  !inc_item_seen  ||
		    inc_seen  &&  !inc_item_seen  ||
		    exc_item_seen  &&  inc_item_seen  &&  exc_pos > inc_pos)
			return FALSE;
	}


	if (mur_options.process != NULL)
	{
		inc_seen = inc_item_seen
			 = exc_item_seen
			 = wildcard_match
			 = FALSE;

		for (sl_ptr = mur_options.process, count = 0;  sl_ptr != NULL;  sl_ptr = sl_ptr->next, ++count)
		{
			if (!sl_ptr->exclude)
				inc_seen = TRUE;

			for (i = 0;  i < sl_ptr->len;  ++i)
				if (sl_ptr->buff[i] == '*'  ||  sl_ptr->buff[i] == '%')
				{
					wildcard_match = mur_do_wildcard(pv->jpv_prcnam, sl_ptr->buff, JPV_LEN_PRCNAM, sl_ptr->len);
					break;
				}

			if (wildcard_match  ||  memcmp(pv->jpv_prcnam, sl_ptr->buff, MIN(sl_ptr->len, JPV_LEN_PRCNAM)) == 0)
				if (sl_ptr->exclude)
				{
					exc_item_seen = TRUE;
					exc_pos = count;
				}
				else
				{
					inc_item_seen = TRUE;
					inc_pos = count;
				}
		}

		if (exc_item_seen  &&  !inc_item_seen  ||
		    inc_seen  &&  !inc_item_seen  ||
		    exc_item_seen  &&  inc_item_seen  &&  exc_pos > inc_pos)
			return FALSE;
	}


	if (mur_options.id != NULL)
	{
		inc_seen = inc_item_seen
			 = exc_item_seen
			 = FALSE;

		for (ll_ptr = mur_options.id, count = 0;  ll_ptr != NULL;  ll_ptr = ll_ptr->next, ++count)
		{
			if (!ll_ptr->exclude)
				inc_seen = TRUE;

			if (ll_ptr->num == pv->jpv_pid)
				if (ll_ptr->exclude)
				{
					exc_item_seen = TRUE;
					exc_pos = count;
				}
				else
				{
					inc_item_seen = TRUE;
					inc_pos = count;
				}
		}

		if (exc_item_seen  &&  !inc_item_seen  ||
		    inc_seen  &&  !inc_item_seen  ||
		    exc_item_seen  &&  inc_item_seen  &&  exc_pos > inc_pos)
			return FALSE;
	}


	switch (rectype)
	{
	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		return mur_options.transaction != TRANS_KILLS;

	case JRT_KILL:
	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_ZKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		return mur_options.transaction != TRANS_SETS;

	default:
		return TRUE;
	}

}
