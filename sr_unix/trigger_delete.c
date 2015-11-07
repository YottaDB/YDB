/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>			/* for gv_trigger.h */
#include "gv_trigger.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "t_retry.h"
#include "gdsblk.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "targ_alloc.h"			/* for SETUP_TRIGGER_GLOBAL & SWITCH_TO_DEFAULT_REGION */
#include "hashtab_str.h"
#include "wbox_test_init.h"
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
#include "zshow.h"			/* for format2zwr() prototype */

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_addr			*gd_header;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gv_namehead		*gv_target;
GBLREF	boolean_t		dollar_ztrigger_invoked;
GBLREF	gv_namehead		*gv_target_list;
GBLREF	trans_num		local_tn;

LITREF	mval			literal_hasht;
LITREF	char 			*trigger_subs[];

error_def(ERR_TEXT);
error_def(ERR_TRIGDEFBAD);
error_def(ERR_TRIGMODINTP);
error_def(ERR_TRIGNAMBAD);
error_def(ERR_TRIGMODREGNOTRW);

#define MAX_CMD_LEN		20	/* Plenty of room for S,K,ZK,ZTK */

/* This error macro is used for all definition errors where the target is ^#t("TRHASH",<HASH>) */
#define TRHASH_DEFINITION_RETRY_OR_ERROR(HASH, CSA)					\
{											\
	if (CDB_STAGNATE > t_tries)							\
		t_retry(cdb_sc_triggermod);						\
	else										\
	{										\
		assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);	\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len,	\
				trigvn, LEN_AND_LIT("\"#TRHASH\""),HASH->str.len,	\
				HASH->str.addr);					\
	}										\
}

#define SEARCH_AND_KILL_BY_HASH(TRIGVN, TRIGVN_LEN, HASH, TRIG_INDEX, CSA)							\
{																\
	mval			mv_hash_indx;											\
	mval			mv_hash_val;											\
	int			hash_index;											\
																\
	if (search_trigger_hash(TRIGVN, TRIGVN_LEN, HASH, TRIG_INDEX, &hash_index)) 						\
	{															\
		MV_FORCE_UMVAL(&mv_hash_val, HASH->hash_code);									\
		MV_FORCE_MVAL(&mv_hash_indx, hash_index);									\
		BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash_val, mv_hash_indx);	\
		gvcst_kill(FALSE);												\
	} else															\
	{	/* There has to be a #TRHASH entry */										\
		TRHASH_DEFINITION_RETRY_OR_ERROR(HASH, CSA);									\
	}															\
}

STATICFNDEF void cleanup_trigger_hash(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *set_hash,
		stringkey *kill_hash, boolean_t del_kill_hash, int match_index)
{
	sgmnt_addrs		*csa;
	uint4			len;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	mstr			trigger_key;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	assert(0 != gv_target->root);
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	if (NULL != strchr(values[CMD_SUB], 'S'))
	{
		SEARCH_AND_KILL_BY_HASH(trigvn, trigvn_len, set_hash, match_index, csa)
	}
	if (del_kill_hash)
	{
		SEARCH_AND_KILL_BY_HASH(trigvn, trigvn_len, kill_hash, match_index, csa);
	}
	RESTORE_TRIGGER_REGION_INFO;
}

STATICFNDEF void cleanup_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, int trigger_name_len)
{
	sgmnt_addrs		*csa;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
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
	boolean_t		is_auto_name;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* assume user defined name or auto gen name whose GVN < 21 chars */
	is_auto_name = FALSE;
	if (!trigger_user_name(trigger_name, trigger_name_len))
	{	/* auto gen name uses #TNCOUNT and #SEQNO under #TNAME */
		is_auto_name = TRUE;
		used_trigvn_len = MIN(trigvn_len, MAX_AUTO_TRIGNAME_LEN);
		memcpy(trunc_name, trigvn, used_trigvn_len);
	}
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	if (0 != gv_target->root)
	{
		if (gv_cur_region->read_only)
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
		if (is_auto_name)
		{
			/* $get(^#t("#TNAME",<trunc_name>,"#TNCOUNT")) */
			BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trunc_name,
							used_trigvn_len, LITERAL_HASHTNCOUNT, STRLEN(LITERAL_HASHTNCOUNT));
			if (gvcst_get(&val))
			{	 /* only long autogenerated names have a #TNCOUNT entry */
				val_ptr = &val;
				var_count = MV_FORCE_INT(val_ptr);
				if (1 == var_count)
				{
					/* kill ^#t("#TNAME",<trunc_name>) to kill #TNCOUNT and #SEQNO */
					BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trunc_name,
								    used_trigvn_len);
					gvcst_kill(TRUE);
				} else
				{
					var_count--;
					MV_FORCE_MVAL(&val, var_count);
					/* set ^#t("#TNAME",GVN,"#TNCOUNT")=var_count */
					SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME),
									trunc_name, used_trigvn_len, LITERAL_HASHTNCOUNT,
									STRLEN(LITERAL_HASHTNCOUNT), val, result);
					assert(PUT_SUCCESS == result);		/* The count size can only decrease */
				}
			}
		}
		/* kill ^#t("#TNAME",<trigger_name>,:) or zkill ^#t("#TNAME",<trigger_name>) if is_auto_name==FALSE */
		BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trigger_name,
					    trigger_name_len - 1);
		gvcst_kill(is_auto_name);
	}
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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (MAX_AUTO_TRIGNAME_LEN < trigvn_len)
		return PUT_SUCCESS;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	assert(0 != gv_target->root);
	/* $get(^#t("#TNAME",^#t(GVN,index,"#TRIGNAME")) */
	BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trig_name_len - 1);
	if (!gvcst_get(&trig_gbl))
	{	/* There has to be a #TNAME entry */
		if (CDB_STAGNATE > t_tries)
			t_retry(cdb_sc_triggermod);
		else
		{
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TRIGNAMBAD, 4, LEN_AND_LIT("\"#TNAME\""), trig_name_len - 1,
					trig_name);
		}
	}
	len = STRLEN(trig_gbl.str.addr) + 1;
	assert(MAX_MIDENT_LEN >= len);
	memcpy(name_and_index, trig_gbl.str.addr, len);
	ptr = name_and_index + len;
	num_len = 0;
	I2A(ptr, num_len, new_trig_index);
	len += num_len;
	/* set ^#t(GVN,index,"#TRIGNAME")=trig_name $C(0) new_trig_index */
	SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trig_name_len - 1,
		name_and_index, len, result);
	RESTORE_TRIGGER_REGION_INFO;
	return result;
}

STATICFNDEF int4 update_trigger_hash_value(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *set_hash,
					   stringkey *kill_hash, int old_trig_index, int new_trig_index)
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
	char			tmp_str[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	assert(0 != gv_target->root);
	if (NULL != strchr(values[CMD_SUB], 'S'))
	{
		if (!search_trigger_hash(trigvn, trigvn_len, set_hash, old_trig_index, &hash_index))
		{	/* There has to be an entry with the old hash value, we just looked it up */
			TRHASH_DEFINITION_RETRY_OR_ERROR(set_hash, csa);
		}
		MV_FORCE_UMVAL(&mv_hash, set_hash->hash_code);
		MV_FORCE_MVAL(&mv_hash_indx, hash_index);
		BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx);
		if (!gvcst_get(&key_val))
		{	/* There has to be a #TRHASH entry */
			TRHASH_DEFINITION_RETRY_OR_ERROR(set_hash, csa);
		}
		assert((MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT) >= key_val.str.len);
		len = STRLEN(key_val.str.addr);
		memcpy(tmp_str, key_val.str.addr, len);
		ptr = tmp_str + len;
		*ptr++ = '\0';
		num_len = 0;
		I2A(ptr, num_len, new_trig_index);
		len += num_len + 1;
		SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx,
			tmp_str, len, result);
		if (PUT_SUCCESS != result)
		{
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	}
	if (!search_trigger_hash(trigvn, trigvn_len, kill_hash, old_trig_index, &hash_index))
	{	/* There has to be an entry with the old hash value, we just looked it up */
		TRHASH_DEFINITION_RETRY_OR_ERROR(kill_hash, csa);
	}
	MV_FORCE_UMVAL(&mv_hash, kill_hash->hash_code);
	MV_FORCE_MVAL(&mv_hash_indx, hash_index);
	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx);
	if (!gvcst_get(&key_val))
	{	/* There has to be a #TRHASH entry */
		TRHASH_DEFINITION_RETRY_OR_ERROR(kill_hash, csa);
	}
	assert((MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT) >= key_val.str.len);
	len = STRLEN(key_val.str.addr);
	memcpy(tmp_str, key_val.str.addr, len);
	ptr = tmp_str + len;
	*ptr++ = '\0';
	num_len = 0;
	I2A(ptr, num_len, new_trig_index);
	len += num_len + 1;
	SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx,
		tmp_str, len, result);
	if (PUT_SUCCESS != result)
	{
		RESTORE_TRIGGER_REGION_INFO;
		return result;
	}
	RESTORE_TRIGGER_REGION_INFO;
	return PUT_SUCCESS;
}

boolean_t trigger_delete_name(char *trigger_name, uint4 trigger_name_len, uint4 *trig_stats)
{
	sgmnt_addrs		*csa;
	char			curr_name[MAX_MIDENT_LEN + 1];
	uint4			curr_name_len, orig_name_len;
	mstr			gbl_name;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	int			len;
	mval			mv_curr_nam;
	boolean_t		name_found;
	char			*ptr;
	char			*name_tail_ptr;
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
	int			badpos;
	boolean_t		wildcard;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	badpos = 0;
	orig_name_len = trigger_name_len;
	if ((0 == trigger_name_len) || (trigger_name_len !=
			(badpos = validate_input_trigger_name(trigger_name, trigger_name_len, &wildcard))))
	{	/* is the input name valid */
		CONV_STR_AND_PRINT("Invalid trigger NAME string: ", orig_name_len, trigger_name);
		/* badpos is the string position where the bad character was found, pretty print it */
		return TRIG_FAILURE;
	}
	name_tail_ptr = trigger_name + trigger_name_len - 1;
	if (TRIGNAME_SEQ_DELIM == *name_tail_ptr || wildcard )
		/* drop the trailing # sign or wildcard */
		trigger_name_len--;
	/* $data(^#t) */
	SWITCH_TO_DEFAULT_REGION;
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 == gv_target->root)
	{
		util_out_print_gtmio("Trigger named !AD does not exist", FLUSH, orig_name_len, trigger_name);
		return TRIG_FAILURE;
	}
	name_found = FALSE;
	assert(trigger_name_len < MAX_MIDENT_LEN);
	memcpy(save_name, trigger_name, trigger_name_len);
	save_name[trigger_name_len] = '\0';
	memcpy(curr_name, save_name, trigger_name_len);
	curr_name_len = trigger_name_len;
	STR2MVAL(mv_curr_nam, trigger_name, trigger_name_len);
	do {
		/* GVN = $get(^#t("#TNAME",curr_name) */
		BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), curr_name, curr_name_len);
		if (gvcst_get(&trig_gbl))
		{
			SAVE_TRIGGER_REGION_INFO;
			ptr = trig_gbl.str.addr;
			trigvn_len = STRLEN(trig_gbl.str.addr);
			assert(MAX_MIDENT_LEN >= trigvn_len);
			memcpy(trigvn, ptr, trigvn_len);
			ptr += trigvn_len + 1;
			/* the index is just beyon the length of the GVN string */
			A2I(ptr, trig_gbl.str.addr + trig_gbl.str.len, trig_indx);
			gbl_name.addr = trigvn;
			gbl_name.len = trigvn_len;
			GV_BIND_NAME_ONLY(gd_header, &gbl_name);
			if (gv_cur_region->read_only)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
			csa = gv_target->gd_csa;
			SETUP_TRIGGER_GLOBAL;
			INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
			/* $get(^#t(GVN,"COUNT") */
			BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
			/* if it does not exist, return false */
			if (!gvcst_get(&trigger_count))
			{
				util_out_print_gtmio("Trigger named !AD exists in the lookup table, "
						"but global ^!AD has no triggers",
						FLUSH, curr_name_len, curr_name, trigvn_len, trigvn);
				return TRIG_FAILURE;
			}
			/* kill the target trigger for GVN at index trig_indx */
			if (PUT_SUCCESS != (trigger_delete(trigvn, trigvn_len, &trigger_count, trig_indx)))
			{
				util_out_print_gtmio("Trigger named !AD exists in the lookup table, but was not deleted!",
						FLUSH, orig_name_len, trigger_name);
			} else
			{
				csa->incr_db_trigger_cycle = TRUE;
				if (dollar_ztrigger_invoked)
				{	/* increment db_dztrigger_cycle so that next gvcst_put/gvcst_kill in this transaction,
					 * on this region, will re-read triggers. See trigger_update.c for a comment on why
					 * it is okay for db_dztrigger_cycle to be incremented more than once in the same
					 * transaction
					 */
					csa->db_dztrigger_cycle++;
				}
				trig_stats[STATS_DELETED]++;
				if (0 == trig_stats[STATS_ERROR])
					util_out_print_gtmio("Deleted trigger named '!AD' for global ^!AD",
							FLUSH, curr_name_len, curr_name, trigvn_len, trigvn);
			}
			RESTORE_TRIGGER_REGION_INFO;
			name_found = TRUE;
		} else
		{ /* no names match, if !wildcard report an error */
			if (!wildcard)
			{
				util_out_print_gtmio("Trigger named !AD does not exist",
						FLUSH, orig_name_len, trigger_name);
				return TRIG_FAILURE;
			}
		}
		if (!wildcard)
			/* not a wild card, don't $order for the next match */
			break;
		op_gvorder(&mv_curr_nam);
		if (0 == mv_curr_nam.str.len)
			break;
		assert(mv_curr_nam.str.len < MAX_MIDENT_LEN);
		memcpy(curr_name, mv_curr_nam.str.addr, mv_curr_nam.str.len);
		curr_name_len = mv_curr_nam.str.len;
		if (0 != memcmp(curr_name, save_name, trigger_name_len))
			/* stop when gv_order returns a string that no longer starts save_name */
			break;
	} while (wildcard);
	if (name_found)
		return TRIG_SUCCESS;
	util_out_print_gtmio("Trigger named !AD does not exist", FLUSH, orig_name_len, trigger_name);
	return TRIG_FAILURE;
}

int4 trigger_delete(char *trigvn, int trigvn_len, mval *trigger_count, int index)
{
	int			count;
	mval			*mv_cnt_ptr;
	mval			mv_val;
	mval			*mv_val_ptr;
	int			num_len;
	char			*ptr1;
	int4			result;
	int4			retval;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	stringkey		kill_hash, set_hash;
	int			sub_indx;
	char			tmp_trig_str[MAX_BUFF_SIZE];
	int4			trig_len;
	char			trig_name[MAX_TRIGNAME_LEN];
	int			trig_name_len;
	int			tmp_len;
	char			*tt_val[NUM_SUBS];
	uint4			tt_val_len[NUM_SUBS];
	mval			trigger_value;
	mval			trigger_index;
	mval			xecute_index;
	uint4			xecute_idx;
	uint4			used_trigvn_len;
	mval			val;
	char			val_str[MAX_DIGITS_IN_INT + 1];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
	/* $get(^#t(GVN,trigger_index,"LHASH") for deletion in  cleanup_trigger_hash */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[LHASH_SUB],
		STRLEN(trigger_subs[LHASH_SUB]));
	if (gvcst_get(mv_val_ptr))
		kill_hash.hash_code = (uint4)MV_FORCE_INT(mv_val_ptr);
	else {
		util_out_print_gtmio("The LHASH for global ^!AD does not exist", FLUSH, trigvn_len, trigvn);
		kill_hash.hash_code = 0;
	}
	/* $get(^#t(GVN,trigger_index,"BHASH") for deletion in  cleanup_trigger_hash */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[BHASH_SUB],
		STRLEN(trigger_subs[BHASH_SUB]));
	if (gvcst_get(mv_val_ptr))
		set_hash.hash_code = (uint4)MV_FORCE_INT(mv_val_ptr);
	else {
		util_out_print_gtmio("The BHASH for global ^!AD does not exist", FLUSH, trigvn_len, trigvn);
		set_hash.hash_code = 0;
	}
	/* kill ^#t(GVN,trigger_index) */
	BUILD_HASHT_SUB_MSUB_CURRKEY(trigvn, trigvn_len, trigger_index);
	gvcst_kill(TRUE);
	assert(0 == gvcst_data());
	if (1 == count)
	{ /* This is the last trigger for "trigvn" - clean up trigger name, remove #LABEL and #COUNT */
		assert(1 == index);
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
		gvcst_kill(TRUE);
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
		gvcst_kill(TRUE);
		cleanup_trigger_name(trigvn, trigvn_len, trig_name, trig_name_len);
		cleanup_trigger_hash(trigvn, trigvn_len, tt_val, tt_val_len, &set_hash, &kill_hash, TRUE, 0);
	} else
	{
		cleanup_trigger_hash(trigvn, trigvn_len, tt_val, tt_val_len, &set_hash, &kill_hash, TRUE, index);
		cleanup_trigger_name(trigvn, trigvn_len, trig_name, trig_name_len);
		if (index != count)
		{	/* Shift the last trigger (index is #COUNT value) to the just deleted trigger's index.
			 * This way count is always accurate and can still be used as the index for new triggers.
			 * Note - there is no dependence on the trigger order, or this technique wouldn't work.
			 */
			ptr1 = tmp_trig_str;
			memcpy(ptr1, trigvn, trigvn_len);
			ptr1 += trigvn_len;
			*ptr1++ = '\0';
			for (sub_indx = 0; sub_indx < NUM_TOTAL_SUBS; sub_indx++)
			{
				/* $get(^#t(GVN,trigger_count,sub_indx) */
				BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[sub_indx],
								STRLEN(trigger_subs[sub_indx]));
				if (gvcst_get(&trigger_value))
				{
					trig_len = trigger_value.str.len;
					/* set ^#t(GVN,trigger_index,sub_indx)=^#t(GVN,trigger_count,sub_indx) */
					SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, trigger_index,
						trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), trigger_value, result);
					assert(PUT_SUCCESS == result);
				} else if (XECUTE_SUB == sub_indx)
				{ /* multi line trigger broken up because it exceeds record size */
					for (xecute_idx = 0; ; xecute_idx++)
					{
						i2mval(&xecute_index, xecute_idx);
						BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(trigvn, trigvn_len, *trigger_count,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index);
						if (!gvcst_get(&trigger_value))
							break;
						SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_MVAL(trigvn, trigvn_len, trigger_index,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index,
							trigger_value, result);
						assert(PUT_SUCCESS == result);
					}
					assert (xecute_idx >= 2); /* multi-line trigger, indices 0, 1 and 2 MUST be defined */
				} else
				{
					if (((TRIGNAME_SUB == sub_indx) || (CMD_SUB == sub_indx) ||
						 (CHSET_SUB == sub_indx)))
					{ /* CMD, NAME and CHSET cannot be zero length */
						if (CDB_STAGNATE > t_tries)
							t_retry(cdb_sc_triggermod);
						else
						{
							assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
							rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD,
									6, trigvn_len, trigvn, trigvn_len, trigvn,
									STRLEN(trigger_subs[sub_indx]), trigger_subs[sub_indx]);
						}
					}
					/* OPTIONS, PIECES and DELIM can be zero */
					trig_len = 0;
				}
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
			/* $get(^#t(GVN,trigger_count,"LHASH") for update_trigger_hash_value */
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[LHASH_SUB],
							 STRLEN(trigger_subs[LHASH_SUB]));
			if (!gvcst_get(mv_val_ptr))
				return PUT_SUCCESS;
			kill_hash.hash_code = (uint4)MV_FORCE_INT(mv_val_ptr);
			/* $get(^#t(GVN,trigger_count,"BHASH") for update_trigger_hash_value */
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[BHASH_SUB],
							 STRLEN(trigger_subs[BHASH_SUB]));
			if (!gvcst_get(mv_val_ptr))
				return PUT_SUCCESS;
			set_hash.hash_code = (uint4)MV_FORCE_INT(mv_val_ptr);
			/* update hash values from above */
			if (VAL_TOO_LONG == (retval = update_trigger_hash_value(trigvn, trigvn_len, tt_val, tt_val_len,
					&set_hash, &kill_hash, count, index)))
				return VAL_TOO_LONG;
			/* fix the value ^#t("#TNAME",^#t(GVN,index,"#TRIGNAME")) to point to the correct "index" */
			if (VAL_TOO_LONG == (retval = update_trigger_name_value(trigvn_len, tt_val[TRIGNAME_SUB],
					tt_val_len[TRIGNAME_SUB], index)))
				return VAL_TOO_LONG;
			/* kill ^#t(GVN,COUNT) which was just shifted to trigger_index */
			BUILD_HASHT_SUB_MSUB_CURRKEY(trigvn, trigvn_len, *trigger_count);
			gvcst_kill(TRUE);
		}
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
	gv_namehead		*hasht_tree, *gvt;
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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 < dollar_tlevel);
	/* Before we delete any triggers, verify that none of the triggers have been fired in this transaction. If they have,
	 * this creates an un-commitable transaction that will end in a TPFAIL error. Since that error indicates database
	 * damage, we'd rather detect this avoidable condition and give a descriptive error instead (TRIGMODINTP).
	 */
	for (gvt = gv_target_list; NULL != gvt; gvt = gvt->next_gvnh)
	{
		if (gvt->trig_local_tn == local_tn)
			rts_error_csa(CSA_ARG(gvt->gd_csa) VARLSTCNT(1) ERR_TRIGMODINTP);
	}
	SWITCH_TO_DEFAULT_REGION;
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 != gv_target->root)
	{
		/* kill ^#t("#TRHASH") */
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH));
		gvcst_kill(TRUE);
		/* kill ^#t("#TNAME") */
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME));
		gvcst_kill(TRUE);
	}
	for (reg_indx = 0, reg = gd_header->regions; reg_indx < gd_header->n_regions; reg_indx++, reg++)
	{
		if (!reg->open)
			gv_init_reg(reg);
		gv_cur_region = reg;
		change_reg();
		csa = cs_addrs;
		SETUP_TRIGGER_GLOBAL;
		INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
		/* There might not be any ^#t in this region, so check */
		if (0 != gv_target->root)
		{	/* Give error on region only if it has #t global indicating the presence
			 * of triggers.
			 */
			if (reg->read_only)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, REG_LEN_STR(reg));
			/* Kill all descendents of ^#t(trigvn, indx) where trigvn is any global with a trigger,
			 * but skip the "#XYZ" entries. setup ^#t(trigvn,"$") as the PREV key for op_gvorder
			 */
			BUILD_HASHT_SUB_CURRKEY(LITERAL_MAXHASHVAL, STRLEN(LITERAL_MAXHASHVAL));
			TREF(gv_last_subsc_null) = FALSE; /* We know its not null, but prior state is unreliable */
			while (TRUE)
			{
				op_gvorder(&curr_gbl_name);
				/* quit:$length(curr_gbl_name)=0 */
				if (0 == curr_gbl_name.str.len)
					break;
				/* $get(^#t(curr_gbl_name,#COUNT)) */
					BUILD_HASHT_SUB_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len,
						LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
					if (gvcst_get(&trigger_count))
					{
						mv_count_ptr = &trigger_count;
						count = MV_FORCE_INT(mv_count_ptr);
						/* $get(^#t(curr_gbl_name,#CYCLE)) */
						BUILD_HASHT_SUB_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len,
							LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE));
						if (!gvcst_get(&trigger_cycle))
						{	/* Found #COUNT, there must be #CYCLE */
							if (CDB_STAGNATE > t_tries)
								t_retry(cdb_sc_triggermod);
							else
							{
								assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
								rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_TRIGDEFBAD, 6,
										curr_gbl_name.str.len, curr_gbl_name.str.addr,
										curr_gbl_name.str.len, curr_gbl_name.str.addr,
										LEN_AND_LIT("\"#CYCLE\""),
										ERR_TEXT, 2,
										RTS_ERROR_TEXT("#CYCLE field is missing"));
							}
						}
						mv_cycle_ptr = &trigger_cycle;
						cycle = MV_FORCE_INT(mv_cycle_ptr);
						/* kill ^#t(curr_gbl_name) */
						BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
						gvcst_kill(TRUE);
						cycle++;
						MV_FORCE_MVAL(&trigger_cycle, cycle);
						/* set ^#t(curr_gbl_name,#CYCLE)=trigger_cycle */
						SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(curr_gbl_name.str.addr, curr_gbl_name.str.len,
							LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE), trigger_cycle, result);
						assert(PUT_SUCCESS == result);
					} /* else there is no #COUNT, then no triggers, leave #CYCLE alone */
					/* get ready for op_gvorder() call for next trigger under ^#t */
					BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
			}
			csa->incr_db_trigger_cycle = TRUE;
			if (dollar_ztrigger_invoked)
			{	/* increment db_dztrigger_cycle so that next gvcst_put/gvcst_kill in this transaction,
				 * on this region, will re-read. See trigger_update.c for a comment on why it is okay
				 * for db_dztrigger_cycle to be incremented more than once in the same transaction
				 */
				csa->db_dztrigger_cycle++;
			}
		}
	}
	util_out_print_gtmio("All existing triggers deleted", FLUSH);
}
#endif /* GTM_TRIGGER */
