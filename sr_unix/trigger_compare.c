/****************************************************************
 *								*
 * Copyright (c) 2010-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER
#include "gtm_string.h"
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"			/* For gvcst_protos.h */
#include "gvcst_protos.h"
#include "hashtab_str.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_compare_protos.h"
#include "trigger_gbl_fill_xecute_buffer.h"
#include "mvalconv.h"			/* Needed for MV_FORCE_UMVAL, MV_FORCE_* variants */
#include "op.h"
#include "nametabtyp.h"
#include "targ_alloc.h"			/* for SET_GVTARGET_TO_HASHT_GBL & SWITCH_TO_DEFAULT_REGION */
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "hashtab_mname.h"
#include "t_retry.h"

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];

LITREF	mval			literal_one;
LITREF	mval			literal_ten;
LITREF	char 			*trigger_subs[];

LITDEF int trig_subsc_partofhash[] =
{
#define TRIGGER_SUBSDEF(SUBSTYPE, SUBSNAME, LITMVALNAME, TRIGFILEQUAL, PARTOFHASH)	PARTOFHASH,
#include "trigger_subs_def.h"
#undef TRIGGER_SUBSDEF
};

#define COPY_VAL_TO_INDEX_STR(SUB, PTR)		\
{						\
	int	len;				\
						\
	len = value_len[SUB];			\
	if (len)				\
	{					\
		memcpy(PTR, values[SUB], len);	\
		PTR += len;			\
		*PTR++ = '\0';			\
	}					\
}

error_def(ERR_TRIGDEFBAD);

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

void build_kill_cmp_str(char *trigvn, int trigvn_len, char **values, uint4 *value_len, mstr *kill_key, boolean_t multi_line_xecute)
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
	if (!multi_line_xecute)
	{
		COPY_VAL_TO_INDEX_STR(XECUTE_SUB, ptr);
	} else
		*ptr++ = '\0';
	kill_key->len = INTCAST(ptr - kill_key->addr) - 1;
}

void build_set_cmp_str(char *trigvn, int trigvn_len, char **values, uint4 *value_len, mstr *set_key, boolean_t multi_line_xecute)
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
	if (!multi_line_xecute)
	{
		COPY_VAL_TO_INDEX_STR(XECUTE_SUB, ptr);
	} else
		*ptr++ = '\0';
	set_key->len = INTCAST(ptr - set_key->addr) - 1;
}

boolean_t search_trigger_hash(char *trigvn, int trigvn_len, stringkey *trigger_hash, int match_index, int *hash_indx)
{
	mval			collision_indx;
	mval			*collision_indx_ptr;
	int			hash_index;
	mval			key_val;
	int4			len;
	mval			mv_hash;
	boolean_t		match;
	char			*ptr, *ptr2;
	int			trig_index;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	collision_indx_ptr = &collision_indx;
	MV_FORCE_UMVAL(&mv_hash, trigger_hash->hash_code);
	BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len,
		LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
	while (TRUE)
	{
		op_gvorder(collision_indx_ptr);
		if (0 == collision_indx_ptr->str.len)
		{
			hash_index = 0;
			match = FALSE;
			break;
		}
		BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY(trigvn, trigvn_len,
			LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, collision_indx);
		if (!gvcst_get(&key_val))
		{	/* There has to be a #TRHASH entry or else it is a retry situation (due to concurrent updates) */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#TRHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		ptr = key_val.str.addr;
		ptr2 = memchr(ptr, '\0', key_val.str.len);
		if (NULL == ptr2)
		{	/* We expect $c(0) in the middle of ptr. If we dont find it, this is a restartable situation */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#TRHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		len = ptr2 - ptr;
		if ((len != trigvn_len) || (0 != memcmp(trigvn, ptr, len)))
			continue;
		assert(0 != match_index);
		assert(('\0' == *ptr2) && (key_val.str.len > len));
		ptr2++;
		A2I(ptr2, key_val.str.addr + key_val.str.len, trig_index);
		assert(-1 != trig_index);
		match = (match_index == trig_index);
		if (match)
		{
			hash_index = MV_FORCE_UINT(collision_indx_ptr);
			break;
		}
	}
	*hash_indx = hash_index;
	return match;
}

boolean_t search_triggers(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *trigger_hash, int *hash_indx,
			  int *trig_indx, boolean_t doing_set)
{
	mval			collision_indx;
	mval			*collision_indx_ptr;
	mval			data_val;
	boolean_t		have_value;
	mval			key_val;
	int4			len;
	boolean_t		multi_record, kill_cmp, first_match_kill_cmp;
	mval			mv_hash;
	mval			mv_trig_indx;
	boolean_t		match, first_match;
	char			*ptr, *ptr2;
	int			sub_indx;
	int			trig_hash_index;
	int			trig_index;
	char			*xecute_buff;
	int4			xecute_len;
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];	/* needed for HASHT_GVN_DEFINITION_RETRY_OR_ERROR macro */
	int4			util_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
	collision_indx_ptr = &collision_indx;
	MV_FORCE_UMVAL(&mv_hash, trigger_hash->hash_code);
	BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len,
		LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
	DEBUG_ONLY(xecute_buff = NULL;)
	first_match = TRUE;
	while (TRUE)
	{
		match = TRUE;
		op_gvorder(collision_indx_ptr);
		if (0 == collision_indx_ptr->str.len)
		{
			match = FALSE;
			break;
		}
		BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY(trigvn, trigvn_len,
			LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, collision_indx);
		if (!gvcst_get(&key_val))
		{	/* There has to be a #TRHASH entry or else it is a retry situation (due to concurrent updates) */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#TRHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		ptr = key_val.str.addr;
		ptr2 = memchr(ptr, '\0', key_val.str.len);
		if (NULL == ptr2)
		{	/* We expect $c(0) in the middle of ptr. If we dont find it, this is a restartable situation */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#TRHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		len = ptr2 - ptr;
		if ((len != trigvn_len) || (0 != memcmp(trigvn, ptr, len)))
		{	/* We expect all hashes under ^#t(<gbl>,"#TRHASH",...) to correspond to <gbl> in their value.
			 * If not this is a TRIGDEFBAD situation.
			 */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#TRHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		ptr += len;
		assert(('\0' == *ptr) && (key_val.str.len > len));
		ptr++;
		A2I(ptr, key_val.str.addr + key_val.str.len, trig_index);
		assert(-1 != trig_index);
		MV_FORCE_UMVAL(&mv_trig_indx, trig_index);
		for (sub_indx = 0; sub_indx < NUM_TOTAL_SUBS; sub_indx++)
		{
			if (((TRSBS_IN_NONE == trig_subsc_partofhash[sub_indx]) && (CMD_SUB != sub_indx))
					|| (!doing_set && (TRSBS_IN_BHASH == trig_subsc_partofhash[sub_indx])))
				continue;
			assert((CMD_SUB == sub_indx) || ((TRSBS_IN_LHASH | TRSBS_IN_BHASH) == trig_subsc_partofhash[sub_indx])
				|| (doing_set && (TRSBS_IN_BHASH == trig_subsc_partofhash[sub_indx])));
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx, trigger_subs[sub_indx],
							 STRLEN(trigger_subs[sub_indx]));
			multi_record = FALSE;
			have_value = gvcst_get(&key_val);
			if (CMD_SUB == sub_indx)
			{
				if (!have_value || !key_val.str.len)
				{	/* No CMD node. Retry situation (due to concurrent updates) */
					HASHT_GVN_DEFINITION_RETRY_OR_ERROR(trig_index, ",\"CMD\"", cs_addrs);
				}
				kill_cmp = ((NULL != memchr(key_val.str.addr, 'K', key_val.str.len))
						|| (NULL != memchr(key_val.str.addr, 'R', key_val.str.len)));
				continue;
			}
			if (!have_value && (XECUTE_SUB == sub_indx))
			{
				op_gvdata(&data_val);
				multi_record = (literal_ten.m[0] == data_val.m[0]) && (literal_ten.m[1] == data_val.m[1]);
				have_value = multi_record;
			}
			if ((0 == value_len[sub_indx]) && !have_value)
				continue;
			if (XECUTE_SUB == sub_indx)
			{
				assert(NULL == xecute_buff);	/* We don't want a memory leak */
				xecute_buff = trigger_gbl_fill_xecute_buffer(trigvn, trigvn_len, &mv_trig_indx,
									     multi_record ? NULL : &key_val, &xecute_len);
				if ((value_len[sub_indx] == xecute_len)
					&& (0 == memcmp(values[sub_indx], xecute_buff, value_len[sub_indx])))
				{
					free(xecute_buff);
					DEBUG_ONLY(xecute_buff = NULL;)
					continue;
				} else
				{
					free(xecute_buff);
					DEBUG_ONLY(xecute_buff = NULL;)
					match = FALSE;
					break;
				}
			} else
			{
				if (have_value && (value_len[sub_indx] == key_val.str.len)
						&& (0 == memcmp(values[sub_indx], key_val.str.addr, value_len[sub_indx])))
					continue;
				else
				{
					match = FALSE;
					break;
				}
			}
		}
		if (match)
		{
			trig_hash_index = MV_FORCE_UINT(collision_indx_ptr);
			assert(trig_index);
			/* It is possible for 2 triggers to match (in case a KILL and SET trigger for same gvn/subs/xecute string
			 * exists separately). In this case, based on "doing_set", we match the appropriate trigger. If 2 triggers
			 * dont match, we use the only matching trigger even if the trigger type (set/kill) does not match.
			 */
			if (first_match)
			{
				*trig_indx = trig_index;
				*hash_indx = trig_hash_index;
				if (!doing_set && kill_cmp)
					break;	/* KILL type trigger sought and found one. Stop at first. */
				first_match = FALSE; /* search once more */
				first_match_kill_cmp = kill_cmp;
				/* Assume this is the only matching trigger for now. Later match if found will override */
			} else
			{
				assert((first_match_kill_cmp != kill_cmp) || !kill_cmp ||
						(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number));
				/* We have TWO matches. Pick the more appropriate one. */
				if (doing_set != kill_cmp)
				{	/* Current trigger matches input trigger type. Overwrite first_match */
					*trig_indx = trig_index;
					*hash_indx = trig_hash_index;
				}
				/* else current trigger does not match input trigger type. Use first_match as is (already done) */
				break;
			}
		}
		/* We did a BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY at the beginning of this for loop but gv_currkey would have been
		 * overwritten in various places since then so reinitialize it before doing op_gvorder of next iteration.
		 */
		BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY(trigvn, trigvn_len,
			LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, collision_indx);
	}
	if (!match)
	{
		if (first_match)
		{
			*trig_indx = 0;
			*hash_indx = 0;
		} else
			match = TRUE;
	}
	return match;
}
#endif /* GTM_TRIGGER */
