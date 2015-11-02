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

#ifdef GTM_TRIGGER
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"			/* For gvcst_protos.h */
#include "gvcst_protos.h"
#include "rtnhdr.h"			/* for gv_trigger.h */
#include "gv_trigger.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "gdsblk.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "hashtab.h"			/* for STR_HASH (in COMPUTE_HASH_MNAME)*/
#include "targ_alloc.h"			/* for SETUP_TRIGGER_GLOBAL & SWITCH_TO_DEFAULT_REGION */
#include "hashtab_str.h"
#include "trigger_delete_protos.h"
#include "trigger.h"
#include "trigger_incr_cycle.h"
#include "trigger_user_name.h"
#include "trigger_compare_protos.h"
#include "trigger_parse_protos.h"
#include "min_max.h"
#include "mvalconv.h"			/* Needed for MV_FORCE_* */
#include "change_reg.h"
#include "op.h"
#include "util.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_addr			*gd_header;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gv_namehead		*gv_target;

LITREF	mval			literal_hasht;

#define MAX_CMD_LEN		20	/* Plenty of room for S,K,ZK,ZTK */

#define SEARCH_AND_KILL_BY_HASH(TMP_STR, LEN, HASH_VAL, INDEX)									\
{																\
	int			hash_index;											\
	mval			mv_hash_indx;											\
	mval			mv_hash_val;											\
	int			trig_index;											\
																\
	if (search_triggers(TMP_STR, LEN, HASH_VAL, &hash_index, &trig_index, INDEX))						\
	{															\
		MV_FORCE_UMVAL(&mv_hash_val, HASH_VAL);										\
		MV_FORCE_MVAL(&mv_hash_indx, hash_index);									\
		BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash_val, mv_hash_indx);	\
		gvcst_kill(FALSE);												\
	} else															\
		assert(FALSE);													\
}

STATICFNDEF void cleanup_trigger_hash(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, uint4 set_hash,
				      uint4 kill_hash, boolean_t del_kill_hash, int match_index)
{
	sgmnt_addrs		*csa;
	uint4			len;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	char			tmp_str[MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT];
	mstr			trigger_key;

	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	assert(0 != gv_target->root);
	trigger_key.addr = tmp_str;
	if (NULL != strchr(values[CMD_SUB], 'S'))
	{
		trigger_key.len = MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT;
		build_set_cmp_str(trigvn, trigvn_len, values, value_len, &trigger_key);
		SEARCH_AND_KILL_BY_HASH(tmp_str, trigger_key.len, set_hash, match_index);
	}
	if (del_kill_hash)
	{
		trigger_key.len = MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT;
		build_kill_cmp_str(trigvn, trigvn_len, values, value_len, &trigger_key);
		SEARCH_AND_KILL_BY_HASH(tmp_str, trigger_key.len, kill_hash, match_index);
	}
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
}

STATICFNDEF void cleanup_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, int trigger_name_len)
{
	sgmnt_addrs		*csa;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	char			*ptr1;
	int4			result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_altkey;
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	gv_namehead		*save_gvtarget;
	sgm_info		*save_sgm_info_ptr;
	char			trunc_name[MAX_TRIGNAME_LEN + 1];
	uint4			used_trigvn_len;
	mval			val;
	mval			*val_ptr;
	char			val_str[MAX_DIGITS_IN_INT + 1];
	int			var_count;

	used_trigvn_len = MIN(trigvn_len, MAX_AUTO_TRIGNAME_LEN);
	if (trigger_user_name(trigger_name, trigger_name_len) || (MAX_AUTO_TRIGNAME_LEN >= trigvn_len))
	{
		used_trigvn_len = trigger_name_len - 1;
		memcpy(trunc_name, trigger_name, used_trigvn_len);
	} else
		memcpy(trunc_name, trigvn, used_trigvn_len);
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	if (0 != gv_target->root)
	{
		BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trunc_name,
						used_trigvn_len, LITERAL_HASHTNCOUNT, STRLEN(LITERAL_HASHTNCOUNT));
		if (gvcst_get(&val))
		{
			val_ptr = &val;
			var_count = MV_FORCE_INT(val_ptr);
			if (1 == var_count)
			{
				BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trunc_name,
							    used_trigvn_len);
				gvcst_kill(TRUE);
			} else
			{
				var_count--;
				MV_FORCE_MVAL(&val, var_count);
				SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(trunc_name, used_trigvn_len, LITERAL_HASHTNCOUNT,
								STRLEN(LITERAL_HASHTNCOUNT), val, result);
				assert(PUT_SUCCESS == result);		/* The count size can only decrease */
			}
			BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trigger_name,
						    trigger_name_len - 1);
			gvcst_kill(TRUE);
		} else
		{	/* User supplied and short autogenerated names won't have a #TNCOUNT entry, so delete name entry */
			BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trunc_name,
						    used_trigvn_len);
			gvcst_kill(FALSE);
		}
	}
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
}

STATICFNDEF int4 update_trigger_name_value(int trigvn_len, char *trig_name, int trig_name_len, int new_trig_index)
{
	sgmnt_addrs		*csa;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	int			len;
	char			name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	char			new_trig_name[MAX_TRIGNAME_LEN + 1];
	int			num_len;
	char			*ptr;
	int4			result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	mval			trig_gbl;

	if (MAX_AUTO_TRIGNAME_LEN < trigvn_len)
		return PUT_SUCCESS;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	assert(0 != gv_target->root);
	BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trig_name_len - 1);
	if (!gvcst_get(&trig_gbl))
		assert(FALSE);
	len = STRLEN(trig_gbl.str.addr) + 1;
	assert(MAX_MIDENT_LEN >= len);
	memcpy(name_and_index, trig_gbl.str.addr, len);
	ptr = name_and_index + len;
	num_len = 0;
	I2A(ptr, num_len, new_trig_index);
	len += num_len;
	if (!trigger_user_name(trig_name, trig_name_len))
	{
		assert(MAX_TRIGNAME_LEN >= trig_name_len);
		memcpy(new_trig_name, trig_name, trig_name_len);
		ptr = strchr(new_trig_name, TRIGNAME_SEQ_DELIM) + 1;
		assert(NULL != ptr);
		num_len = 0;
		I2A(ptr, num_len, new_trig_index);
		SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), new_trig_name,
			(int)((ptr + num_len) - new_trig_name), name_and_index, len, result);
	} else
	{
		SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trig_name_len - 1,
			name_and_index, len, result);
	}
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
	return result;
}

STATICFNDEF int4 update_trigger_hash_value(char *trigvn, int trigvn_len, char **values, unsigned short *value_len, uint4 set_hash,
					   uint4 kill_hash, int old_trig_index, int new_trig_index)
{
	sgmnt_addrs		*csa;
	int			hash_index;
	mval			key_val;
	uint4			len;
	mval			mv_hash;
	mval			mv_hash_indx;
	int			num_len;
	char			*ptr;
	int4			result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	char			tmp_str[MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT];
	int			trig_index;
	mstr			trigger_key;

	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	assert(0 != gv_target->root);
	if (NULL != strchr(values[CMD_SUB], 'S'))
	{
		trigger_key.addr = tmp_str;
		trigger_key.len = MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT;
		build_set_cmp_str(trigvn, trigvn_len, values, value_len, &trigger_key);
		len = trigger_key.len;
		if (search_triggers(tmp_str, len, set_hash, &hash_index, &trig_index, 0))
		{
			len++;
			assert(trig_index == old_trig_index);
			MV_FORCE_UMVAL(&mv_hash, set_hash);
			MV_FORCE_MVAL(&mv_hash_indx, hash_index);
			BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx);
			if (!gvcst_get(&key_val))
				assert(FALSE);
			ptr = key_val.str.addr + len;
			num_len = 0;
			I2A(ptr, num_len, new_trig_index);
			len += num_len;
			SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx,
				key_val.str.addr, len, result);
			if (PUT_SUCCESS != result)
			{
				TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
				RESTORE_TRIGGER_REGION_INFO;
				return result;
			}
		} else
			assert(FALSE);
	}
	trigger_key.addr = tmp_str;
	trigger_key.len = MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT;
	build_kill_cmp_str(trigvn, trigvn_len, values, value_len, &trigger_key);
	len = trigger_key.len;
	if (search_triggers(tmp_str, len, kill_hash, &hash_index, &trig_index, old_trig_index))
	{
		len++;
		assert(trig_index == old_trig_index);
		MV_FORCE_UMVAL(&mv_hash, kill_hash);
		MV_FORCE_MVAL(&mv_hash_indx, hash_index);
		BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx);
		if (!gvcst_get(&key_val))
			assert(FALSE);
		ptr = key_val.str.addr + len;
		num_len = 0;
		I2A(ptr, num_len, new_trig_index);
		len += num_len;
		SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx,
			key_val.str.addr, len, result);
		if (PUT_SUCCESS != result)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	} else
		assert(FALSE);
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
	return PUT_SUCCESS;
}

boolean_t trigger_delete_name(char *trigger_name, uint4 trigger_name_len, uint4 *trig_stats)
{
	sgmnt_addrs		*csa;
	char			curr_name[MAX_MIDENT_LEN + 1];
	uint4			curr_name_len;
	mstr			gbl_name;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	int			len;
	mval			mv_curr_nam;
	boolean_t		name_found;
	char			*ptr;
	char			*ptr1;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	char			save_name[MAX_MIDENT_LEN + 1];
	sgm_info		*save_sgm_info_ptr;
	mval			trig_gbl;
	mval			trig_value;
	mval			trigger_count;
	char			trigvn[MAX_MIDENT_LEN + 1];
	int			trigvn_len;
	int			trig_indx;
	boolean_t		wildcard;

	ptr1 = trigger_name + trigger_name_len - 1;
	wildcard = ('*' == *ptr1);
	if (wildcard)
	{
		*ptr1 = '\0';
		trigger_name_len--;
	}
	if (!check_name(trigger_name, trigger_name_len))
	{	/* name#<number># is OK here */
		if ((TRIGNAME_SEQ_DELIM == *ptr1)
				&& (ptr1 != (ptr = strchr(trigger_name, TRIGNAME_SEQ_DELIM)))
				&& (ISDIGIT(*(ptr + 1))))
		{
			for (ptr++ ; ptr < ptr1; ptr++)
			{
				if (!ISDIGIT(*ptr))
				{
					util_out_print_gtmio("Invalid trigger NAME string : !AD", FLUSH, trigger_name_len,
						trigger_name);
					return TRIG_FAILURE;
				}
			}
		} else
		{
			util_out_print_gtmio("Invalid trigger NAME string : !AD", FLUSH, trigger_name_len, trigger_name);
			return TRIG_FAILURE;
		}

	}
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 == gv_target->root)
	{
		util_out_print_gtmio("Trigger named !AD does not exist", FLUSH, trigger_name_len, trigger_name);
		return TRIG_FAILURE;
	}
	name_found = FALSE;
	if (!trigger_user_name(trigger_name, trigger_name_len))
		trigger_name_len--;
	memcpy(save_name, trigger_name, trigger_name_len);
	save_name[trigger_name_len] = '\0';
	memcpy(curr_name, trigger_name, trigger_name_len);
	curr_name_len = trigger_name_len;
	STR2MVAL(mv_curr_nam, trigger_name, trigger_name_len);
	while (TRUE)
	{
		if (0 != memcmp(curr_name, save_name, trigger_name_len))
			break;
		BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), curr_name, curr_name_len);
		if (!gvcst_get(&trig_gbl))
		{
			if (wildcard)
			{
				op_gvorder(&mv_curr_nam);
				if (0 == mv_curr_nam.str.len)
					break;
				memcpy(curr_name, mv_curr_nam.str.addr, mv_curr_nam.str.len);
				curr_name_len = mv_curr_nam.str.len;
				continue;
			} else
				util_out_print_gtmio("Trigger named !AD does not exist", FLUSH, trigger_name_len, save_name);
			break;
		}
		ptr = trig_gbl.str.addr;
		trigvn_len = STRLEN(trig_gbl.str.addr);
		assert(MAX_MIDENT_LEN >= trigvn_len);
		memcpy(trigvn, ptr, trigvn_len);
		ptr += trigvn_len + 1;
		A2I(ptr, trig_gbl.str.addr + trig_gbl.str.len, trig_indx);
		gbl_name.addr = trigvn;
		gbl_name.len = trigvn_len;
		gv_bind_name(gd_header, &gbl_name);
		csa = gv_target->gd_csa;
		SETUP_TRIGGER_GLOBAL;
		INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
		if (!gvcst_get(&trigger_count))
			return FALSE;
		trigger_delete(trigvn, trigvn_len, &trigger_count, trig_indx);
		trig_stats[STATS_DELETED]++;
		if (0 == trig_stats[STATS_ERROR])
			util_out_print_gtmio("^!AD trigger deleted", FLUSH, trigvn_len, trigvn);
		SWITCH_TO_DEFAULT_REGION;
		if (wildcard)
		{
			if (!trigger_user_name(curr_name, curr_name_len))
				continue;
			BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), curr_name, curr_name_len);
			op_gvorder(&mv_curr_nam);
			if (0 == mv_curr_nam.str.len)
				break;
			memcpy(curr_name, mv_curr_nam.str.addr, mv_curr_nam.str.len);
			curr_name_len = mv_curr_nam.str.len;
		} else
			break;
	}
	return TRIG_SUCCESS;
}

int4 trigger_delete(char *trigvn, int trigvn_len, mval *trigger_count, int index)
{
	int			count;
	uint4			kill_hash;
	mval			*mv_cnt_ptr;
	mval			mv_val;
	mval			*mv_val_ptr;
	int			num_len;
	char			*ptr1;
	int4			result;
	int4			retval;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	uint4			set_hash;
	int			sub_indx;
	char			tmp_trig_str[MAX_BUFF_SIZE];
	int4			trig_len;
	char			trig_name[MAX_TRIGNAME_LEN];
	int			trig_name_len;
	int			tmp_len;
	char			*tt_val[NUM_SUBS];
	unsigned short		tt_val_len[NUM_SUBS];
	mval			trigger_value;
	mval			trigger_index;
	uint4			used_trigvn_len;
	mval			val;
	char			val_str[MAX_DIGITS_IN_INT + 1];

	mv_val_ptr = &mv_val;
	MV_FORCE_MVAL(&trigger_index, index);
	count = MV_FORCE_INT(trigger_count);
	/* build up array of values - needed for comparison in hash stuff */
	ptr1 = tmp_trig_str;
	memcpy(ptr1, trigvn, trigvn_len);
	ptr1 += trigvn_len;
	*ptr1++ = '\0';
	tmp_len = trigvn_len + 1;
	for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
	{
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[sub_indx],
						 STRLEN(trigger_subs[sub_indx]));
		trig_len = gvcst_get(&trigger_value) ? trigger_value.str.len : 0;
		if (0 == trig_len)
		{
			tt_val[sub_indx] = NULL;
			tt_val_len[sub_indx] = 0;
			continue;
		}
		if (TRIGNAME_SUB == sub_indx)
		{
			trig_name_len = trig_len;
			assert(MAX_TRIGNAME_LEN >= trig_len);
			memcpy(trig_name, trigger_value.str.addr, trig_name_len);
			tt_val[sub_indx] = NULL;
			tt_val_len[sub_indx] = 0;
			continue;
		}
		tt_val[sub_indx] = ptr1;
		tt_val_len[sub_indx] = trig_len;
		tmp_len += trig_len;
		if (0 < trig_len)
		{
			if (MAX_BUFF_SIZE <= tmp_len)
				return VAL_TOO_LONG;
			memcpy(ptr1, trigger_value.str.addr, trig_len);
			ptr1 += trig_len;
		}
		*ptr1++ = '\0';
		tmp_len++;
	}
	/* Get trigger name, set hash value, and kill hash values from trigger before we delete it.
	 * The values will be used in clean ups associated with the deletion
	 */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[LHASH_SUB],
		STRLEN(trigger_subs[LHASH_SUB]));
	if (!gvcst_get(mv_val_ptr))
		return PUT_SUCCESS;
	kill_hash = (uint4)MV_FORCE_INT(mv_val_ptr);
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[BHASH_SUB],
		STRLEN(trigger_subs[BHASH_SUB]));
	if (!gvcst_get(mv_val_ptr))
		return PUT_SUCCESS;
	set_hash = (uint4)MV_FORCE_INT(mv_val_ptr);
	/* Delete the trigger at "index" */
	BUILD_HASHT_SUB_MSUB_CURRKEY(trigvn, trigvn_len, trigger_index);
	gvcst_kill(TRUE);
	if (1 == count)
	{ /* This is the last trigger for "trigvn" - clean up trigger name, remove #LABEL and #COUNT */
		assert(1 == index);
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
		gvcst_kill(TRUE);
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
		gvcst_kill(TRUE);
		cleanup_trigger_name(trigvn, trigvn_len, trig_name, trig_name_len);
		cleanup_trigger_hash(trigvn, trigvn_len, tt_val, tt_val_len, set_hash, kill_hash, TRUE, 0);
	} else
	{
		cleanup_trigger_hash(trigvn, trigvn_len, tt_val, tt_val_len, set_hash, kill_hash, TRUE, index);
		if (index != count)
		{
			/* Shift the last trigger (index is #COUNT value) to the just deleted trigger's index.
			 * This way count is always accurate and can still be used as the index for new triggers.
			 * Note - there is no dependence on the trigger order, or this technique wouldn't work.
			 */
			ptr1 = tmp_trig_str;
			memcpy(ptr1, trigvn, trigvn_len);
			ptr1 += trigvn_len;
			*ptr1++ = '\0';
			for (sub_indx = 0; sub_indx < NUM_TOTAL_SUBS; sub_indx++)
			{
				BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[sub_indx],
								STRLEN(trigger_subs[sub_indx]));
				if (gvcst_get(&trigger_value))
				{
					trig_len = trigger_value.str.len;
					SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, trigger_index,
						trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), trigger_value, result);
					assert(PUT_SUCCESS == result);
				} else
					trig_len = 0;
				if (NUM_SUBS > sub_indx)
				{
					tt_val[sub_indx] = ptr1;
					tt_val_len[sub_indx] = trig_len;
					if (0 < trig_len)
					{
						memcpy(ptr1, trigger_value.str.addr, trig_len);
						ptr1 += trig_len;
					}
					*ptr1++ = '\0';
				}
			}
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[LHASH_SUB],
							 STRLEN(trigger_subs[LHASH_SUB]));
			if (!gvcst_get(mv_val_ptr))
				return PUT_SUCCESS;
			kill_hash = (uint4)MV_FORCE_INT(mv_val_ptr);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[BHASH_SUB],
							 STRLEN(trigger_subs[BHASH_SUB]));
			if (!gvcst_get(mv_val_ptr))
				return PUT_SUCCESS;
			set_hash = (uint4)MV_FORCE_INT(mv_val_ptr);
			if (VAL_TOO_LONG == (retval = update_trigger_hash_value(trigvn, trigvn_len, tt_val, tt_val_len, set_hash,
					kill_hash, count, index)))
				return VAL_TOO_LONG;
			if (VAL_TOO_LONG == (retval = update_trigger_name_value(trigvn_len, tt_val[TRIGNAME_SUB],
					tt_val_len[TRIGNAME_SUB], index)))
				return VAL_TOO_LONG;
			if (trigger_user_name(trig_name, trig_name_len))
			{
				BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name,
					trig_name_len - 1);
				gvcst_kill(TRUE);
			}
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, LITERAL_TRIGNAME,
				STRLEN(LITERAL_TRIGNAME));
			if (gvcst_get(&trigger_value))
			{
				if (!trigger_user_name(trigger_value.str.addr, trigger_value.str.len))
				{	/* If trigger name is auto-generated, change its sequence number to "index" */
					ptr1 = strchr(trigger_value.str.addr, TRIGNAME_SEQ_DELIM) + 1;
					num_len = 0;
					I2A(ptr1, num_len, index);
					ptr1 += num_len;
					*ptr1++ = TRIGNAME_SEQ_DELIM;
					trigger_value.str.len = (int)(ptr1 - trigger_value.str.addr);
					SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, trigger_index,
						LITERAL_TRIGNAME, STRLEN(LITERAL_TRIGNAME), trigger_value, result);
					assert(PUT_SUCCESS == result);
				}
			}
			cleanup_trigger_name(trigvn, trigvn_len, tt_val[TRIGNAME_SUB], tt_val_len[TRIGNAME_SUB]);
			/* Delete the last trigger (at count) which was just shifted to index */
			BUILD_HASHT_SUB_MSUB_CURRKEY(trigvn, trigvn_len, *trigger_count);
			gvcst_kill(TRUE);
		} else
			cleanup_trigger_name(trigvn, trigvn_len, trig_name, trig_name_len);
		/* Update #COUNT */
		count--;
		MV_FORCE_MVAL(trigger_count, count);
		SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT), *trigger_count,
			result);
		assert(PUT_SUCCESS == result);		/* Size of count can only get shorter or stay the same */
	}
	trigger_incr_cycle(trigvn, trigvn_len);
	return PUT_SUCCESS;
}

void trigger_delete_all(void)
{
	int			count;
	char			count_str[MAX_DIGITS_IN_INT + 1];
	sgmnt_addrs		*csa;
	mval			curr_gbl_name;
	int			cycle;
	mstr			gbl_name;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	mval			*mv_count_ptr;
	mval			*mv_cycle_ptr;
	mval			mv_indx;
	gd_region		*reg;
	int			reg_indx;
	int4			result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	int			trig_indx;
	mval			trigger_cycle;
	mval			trigger_count;
	mval			val;

	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 != gv_target->root)
	{
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH));
		gvcst_kill(TRUE);
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME));
		gvcst_kill(TRUE);
	}
	for (reg_indx = 0, reg = gd_header->regions; reg_indx < gd_header->n_regions; reg_indx++, reg++)
	{
		if (!reg->open)
			gv_init_reg(reg);
		if (!reg->read_only)
		{
			gv_cur_region = reg;
			change_reg();
			csa = cs_addrs;
			SETUP_TRIGGER_GLOBAL;
			INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
			/* There might not be any ^#t in this region, so check */
			if (0 != gv_target->root)
			{	/* Kill all descendents of ^#t(trigvn, indx) where trigvn is any global with a trigger,
				 * but skip the "#xxxxx" entries.
				 */
				BUILD_HASHT_SUB_CURRKEY(LITERAL_MAXHASHVAL, STRLEN(LITERAL_MAXHASHVAL));
				while (TRUE)
				{
					op_gvorder(&curr_gbl_name);
					if (0 == curr_gbl_name.str.len)
						break;
					BUILD_HASHT_SUB_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len,
						LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
					if (gvcst_get(&trigger_count))
					{
						mv_count_ptr = &trigger_count;
						count = MV_FORCE_INT(mv_count_ptr);
					} else
					{	/* There's no #COUNT, trigger was deleted - only the #CYCLE should be left */
						BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
						continue;
					}
					BUILD_HASHT_SUB_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len,
						LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE));
					if (gvcst_get(&trigger_cycle))
					{
						mv_cycle_ptr = &trigger_cycle;
						cycle = MV_FORCE_INT(mv_cycle_ptr);
					} else
						return;
					BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
					gvcst_kill(TRUE);
					cycle++;
					MV_FORCE_MVAL(&trigger_cycle, cycle);
					SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(curr_gbl_name.str.addr, curr_gbl_name.str.len,
						LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE), trigger_cycle, result);
					assert(PUT_SUCCESS == result);
					/* get ready for op_gvorder() call for next trigger under ^#t */
					BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
				}
			}
		}
	}
	util_out_print_gtmio("All existing triggers deleted", FLUSH);
}
#endif /* GTM_TRIGGER */
