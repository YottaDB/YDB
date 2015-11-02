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

#include "gtm_string.h"
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"			/* For gvcst_protos.h */
#include "gvcst_protos.h"
#include "hashtab_str.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "hashtab.h"			/* for STR_HASH (in COMPUTE_HASH_MNAME)*/
#include "rtnhdr.h"
#include "gv_trigger.h"
#include "trigger_compare_protos.h"
#include "trigger.h"
#include "mvalconv.h"			/* Needed for MV_FORCE_MVAL and MV_FORCE_UMVAL */
#include "op.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;

#define COPY_VAL_TO_INDEX_STR(SUB, PTR)						\
{										\
	memcpy(PTR, values[SUB], value_len[SUB]);				\
	PTR += value_len[SUB];							\
	*PTR++ = '\0';								\
}

STATICFNDEF boolean_t compare_vals(char *trigger_val, uint4 trigger_val_len, char *key_val, uint4 key_val_len)
{
	char		*end_ptr;
	char		*k_ptr;
	char		*t_ptr;

	if (key_val_len < trigger_val_len)
		return FALSE;
	end_ptr = trigger_val + trigger_val_len;
	t_ptr = trigger_val;
	k_ptr = key_val;
	while ((end_ptr > t_ptr) && (*t_ptr == *k_ptr))
	{
		t_ptr++;
		k_ptr++;
	}
	return ((end_ptr > t_ptr) && ('\0' == *k_ptr));
}

void build_kill_cmp_str(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, mstr *kill_key)
{
	uint4			lcl_len;
	char			*ptr;

	lcl_len = kill_key->len;
	assert(lcl_len >= (trigvn_len + 1 + value_len[GVSUBS_SUB] + 1 + value_len[XECUTE_SUB] + 1));
	ptr = kill_key->addr;
	memcpy(ptr, trigvn, trigvn_len);
	ptr += trigvn_len;
	*ptr++ = '\0';
	COPY_VAL_TO_INDEX_STR(GVSUBS_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(XECUTE_SUB, ptr);
	kill_key->len = INTCAST(ptr - kill_key->addr) - 1;
}

void build_set_cmp_str(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, mstr *set_key)
{
	uint4			lcl_len;
	char			*ptr;

	lcl_len = set_key->len;
	assert(lcl_len >= (trigvn_len + 1 + value_len[GVSUBS_SUB] + 1 + value_len[PIECES_SUB] + 1 + value_len[DELIM_SUB] + 1
			   + value_len[ZDELIM_SUB] + 1 + value_len[XECUTE_SUB] + 1));
	ptr = set_key->addr;
	memcpy(ptr, trigvn, trigvn_len);
	ptr += trigvn_len;
	*ptr++ = '\0';
	COPY_VAL_TO_INDEX_STR(GVSUBS_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(PIECES_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(DELIM_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(ZDELIM_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(XECUTE_SUB, ptr);
	set_key->len = INTCAST(ptr - set_key->addr) - 1;
}

boolean_t search_triggers(char *cmp_trig_str, uint4 cmp_trig_str_len, uint4 hash_val, int *hash_indx, int *trig_indx,
			  int match_index)
{
	mval			key_val;
	mval			mv_hash;
	mval			mv_indx;
	mval			*mv_indx_ptr;
	boolean_t		match;
	char			*ptr;
	uint4			trig_hash;
	int			trig_hash_index;
	int			trig_index;

	mv_indx_ptr = &mv_indx;
	MV_FORCE_UMVAL(&mv_hash, hash_val);
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
	gv_currkey->end -= 1;
	while (TRUE)
	{
		op_gvorder(mv_indx_ptr);
		if (0 == mv_indx_ptr->str.len)
		{
			trig_hash_index = trig_index = 0;
			match = FALSE;
			break;
		}
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash,
			mv_indx.str.addr, mv_indx.str.len);
		if (!gvcst_get(&key_val))
			assert(FALSE);
		if ((cmp_trig_str_len < key_val.str.len)
				&& (0 == memcmp(cmp_trig_str, key_val.str.addr, cmp_trig_str_len))
				&& ('\0' == *(key_val.str.addr + cmp_trig_str_len)))
		{
			ptr = key_val.str.addr + cmp_trig_str_len + 1;
			A2I(ptr, key_val.str.addr + key_val.str.len, trig_index);
			if ((0 != match_index) && (match_index != trig_index))
				continue;
			trig_hash_index = MV_FORCE_INT(mv_indx_ptr);
			match = TRUE;
			break;
		}
	}
	*trig_indx = trig_index;
	*hash_indx = trig_hash_index;
	return match;
}

