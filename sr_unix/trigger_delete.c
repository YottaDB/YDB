/****************************************************************
 *								*
 * Copyright (c) 2010-2017 Fidelity National Information	*
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
#include "targ_alloc.h"			/* for SET_GVTARGET_TO_HASHT_GBL */
#include "hashtab_str.h"
#include "wbox_test_init.h"
#include "trigger_delete_protos.h"
#include "trigger.h"
#include "trigger_incr_cycle.h"
#include "trigger_user_name.h"
#include "trigger_compare_protos.h"
#include "trigger_parse_protos.h"
#include "gtm_trigger_trc.h"
#include "min_max.h"
#include "mvalconv.h"			/* Needed for MV_FORCE_* */
#include "change_reg.h"
#include "op.h"
#include "util.h"
#include "zshow.h"			/* for format2zwr() prototype */
#include "hashtab_mname.h"
#include "t_begin.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_reservedDB.h"
#include "is_file_identical.h"
#include "anticipatory_freeze.h"
#include "gtm_repl_multi_inst.h" /* for DISALLOW_MULTIINST_UPDATE_IN_TP */

GBLREF	boolean_t		dollar_ztrigger_invoked;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			dollar_tlevel;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];

LITREF	char 			*trigger_subs[];

error_def(ERR_TEXT);
error_def(ERR_TRIGDEFBAD);
error_def(ERR_TRIGLOADFAIL);
error_def(ERR_TRIGMODREGNOTRW);
error_def(ERR_TRIGNAMBAD);

#define MAX_CMD_LEN		20	/* Plenty of room for S,K,ZK,ZTK */

/* This error macro is used for all definition errors where the target is ^#t("TRHASH",<HASH>) */
#define TRHASH_DEFINITION_RETRY_OR_ERROR(HASH, CSA)					\
{											\
	if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))				\
		t_retry(cdb_sc_triggermod);						\
	assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);		\
	rts_error_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len,		\
			trigvn, LEN_AND_LIT("\"#TRHASH\""),HASH->str.len,		\
			HASH->str.addr);						\
}

#define SEARCH_AND_KILL_BY_HASH(TRIGVN, TRIGVN_LEN, HASH, TRIG_INDEX, CSA)				\
{													\
	mval			mv_hash_indx;								\
	mval			mv_hash_val;								\
	int			hash_index;								\
													\
	if (search_trigger_hash(TRIGVN, TRIGVN_LEN, HASH, TRIG_INDEX, &hash_index)) 			\
	{												\
		MV_FORCE_UMVAL(&mv_hash_val, HASH->hash_code);						\
		MV_FORCE_UMVAL(&mv_hash_indx, hash_index);						\
		BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY(TRIGVN, TRIGVN_LEN,				\
			LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash_val, mv_hash_indx);	\
		gvcst_kill(FALSE);									\
	} else												\
	{	/* There has to be a #TRHASH entry */							\
		TRHASH_DEFINITION_RETRY_OR_ERROR(HASH, CSA);						\
	}												\
}

void cleanup_trigger_hash(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *set_hash,
		stringkey *kill_hash, boolean_t del_kill_hash, int match_index)
{
	sgmnt_addrs		*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	assert(!gv_cur_region->read_only);	/* caller should have already checked this */
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
	if ((NULL != strchr(values[CMD_SUB], 'S')) && (set_hash->hash_code != kill_hash->hash_code))
		SEARCH_AND_KILL_BY_HASH(trigvn, trigvn_len, set_hash, match_index, csa)
	if (del_kill_hash)
		SEARCH_AND_KILL_BY_HASH(trigvn, trigvn_len, kill_hash, match_index, csa);
}

void cleanup_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, int trigger_name_len)
{
	int4			result;
	char			trunc_name[MAX_TRIGNAME_LEN + 1];
	uint4			used_trigvn_len;
	mval			val;
	mval			*val_ptr;
	int			var_count;
	boolean_t		is_auto_name;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!gv_cur_region->read_only);	/* caller should have already checked this */
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
	/* assume user defined name or auto gen name whose GVN < 21 chars */
	if (!trigger_user_name(trigger_name, trigger_name_len))
	{	/* auto gen name uses #TNCOUNT and #SEQNO under #TNAME */
		is_auto_name = TRUE;
		used_trigvn_len = MIN(trigvn_len, MAX_AUTO_TRIGNAME_LEN);
		memcpy(trunc_name, trigvn, used_trigvn_len);
		if (is_auto_name)
		{
			/* $get(^#t("#TNAME",<trunc_name>,"#TNCOUNT")) */
			BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trunc_name,
							used_trigvn_len, LITERAL_HASHTNCOUNT, STRLEN(LITERAL_HASHTNCOUNT));
			if (gvcst_get(&val))
			{	 /* each autogenerated name has a #TNCOUNT entry */
				val_ptr = &val;
				var_count = MV_FORCE_UINT(val_ptr);
				if (1 == var_count)
				{
					/* kill ^#t("#TNAME",<trunc_name>) to kill #TNCOUNT and #SEQNO */
					BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trunc_name,
								    used_trigvn_len);
					gvcst_kill(TRUE);
				} else
				{
					var_count--;
					MV_FORCE_UMVAL(&val, var_count);
					/* set ^#t("#TNAME",GVN,"#TNCOUNT")=var_count */
					SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME),
									trunc_name, used_trigvn_len, LITERAL_HASHTNCOUNT,
									STRLEN(LITERAL_HASHTNCOUNT), val, result);
					assert(PUT_SUCCESS == result);		/* The count size can only decrease */
				}
			}
		}
	} else
		is_auto_name = FALSE;
	/* kill ^#t("#TNAME",<trigger_name>,:) or zkill ^#t("#TNAME",<trigger_name>) if is_auto_name==FALSE */
	BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trigger_name,
				    trigger_name_len - 1);
	gvcst_kill(is_auto_name);
}

STATICFNDEF int4 update_trigger_name_value(char *trig_name, int trig_name_len, int new_trig_index)
{
	int			len;
	char			name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	int			num_len;
	char			*ptr;
	int4			result;
	mval			trig_gbl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!gv_cur_region->read_only);		/* caller should have already checked this */
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
	/* $get(^#t("#TNAME",^#t(GVN,index,"#TRIGNAME"))) */
	BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trig_name_len - 1);
	if (!gvcst_get(&trig_gbl))
	{	/* There has to be a #TNAME entry */
		if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
			t_retry(cdb_sc_triggermod);
		assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TRIGNAMBAD, 4, LEN_AND_LIT("\"#TNAME\""),
			trig_name_len - 1, trig_name);
	}
	ptr = trig_gbl.str.addr;
	len = MIN(trig_gbl.str.len, MAX_MIDENT_LEN);
	STRNLEN(ptr, len, len);
	ptr += len;
	if ((trig_gbl.str.len == len) || ('\0' != *ptr))
	{
		if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
			t_retry(cdb_sc_triggermod);
		assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TRIGNAMBAD, 4, LEN_AND_LIT("\"#TNAME\""),
			trig_name_len - 1, trig_name);
	}
	memcpy(name_and_index, trig_gbl.str.addr, ++len); /* inline increment intended */
	ptr = name_and_index + len;
	num_len = 0;
	I2A(ptr, num_len, new_trig_index);
	len += num_len;
	/* set ^#t("#TNAME",<trigname>)=gblname_$C(0)_new_trig_index */
	SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trig_name_len - 1,
		name_and_index, len, result);
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
	char			tmp_str[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!gv_cur_region->read_only);		/* caller should have already checked this */
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
	csa = cs_addrs;
	if ((NULL != strchr(values[CMD_SUB], 'S')) && (set_hash->hash_code != kill_hash->hash_code))
	{
		if (!search_trigger_hash(trigvn, trigvn_len, set_hash, old_trig_index, &hash_index))
		{	/* There has to be an entry with the old hash value, we just looked it up */
			TRHASH_DEFINITION_RETRY_OR_ERROR(set_hash, csa);
		}
		MV_FORCE_UMVAL(&mv_hash, set_hash->hash_code);
		MV_FORCE_UMVAL(&mv_hash_indx, hash_index);
		BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY(trigvn, trigvn_len,
			LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx);
		if (!gvcst_get(&key_val))
		{	/* There has to be a #TRHASH entry */
			TRHASH_DEFINITION_RETRY_OR_ERROR(set_hash, csa);
		}
		assert((MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT) >= key_val.str.len);
		ptr = key_val.str.addr;
		len = MIN(key_val.str.len, MAX_MIDENT_LEN);
		STRNLEN(ptr, len, len);
		ptr += len;
		if ((key_val.str.len == len) || ('\0' != *ptr))
		{	/* We expect $c(0) in the middle of ptr. If we dont find it, this is a restartable situation */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#BHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		memcpy(tmp_str, key_val.str.addr, len);
		ptr = tmp_str + len;
		*ptr++ = '\0';
		num_len = 0;
		I2A(ptr, num_len, new_trig_index);
		len += num_len + 1;
		SET_TRIGGER_GLOBAL_SUB_SUB_MSUB_MSUB_STR(trigvn, trigvn_len,
			LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx, tmp_str, len, result);
		if (PUT_SUCCESS != result)
			return result;
	}
	if (!search_trigger_hash(trigvn, trigvn_len, kill_hash, old_trig_index, &hash_index))
	{	/* There has to be an entry with the old hash value, we just looked it up */
		TRHASH_DEFINITION_RETRY_OR_ERROR(kill_hash, csa);
	}
	MV_FORCE_UMVAL(&mv_hash, kill_hash->hash_code);
	MV_FORCE_UMVAL(&mv_hash_indx, hash_index);
	BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY(trigvn, trigvn_len,
		LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx);
	if (!gvcst_get(&key_val))
	{	/* There has to be a #TRHASH entry */
		TRHASH_DEFINITION_RETRY_OR_ERROR(kill_hash, csa);
	}
	assert((MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT) >= key_val.str.len);
	ptr = key_val.str.addr;
	len = MIN(key_val.str.len, MAX_MIDENT_LEN);
	STRNLEN(ptr, len, len);
	ptr += len;
	if ((key_val.str.len == len) || ('\0' != *ptr))
	{	/* We expect $c(0) in the middle of ptr. If we dont find it, this is a restartable situation */
		if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
			t_retry(cdb_sc_triggermod);
		assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
		rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
				LEN_AND_LIT("\"#LHASH\""), mv_hash.str.len, mv_hash.str.addr);
	}
	memcpy(tmp_str, key_val.str.addr, len);
	ptr = tmp_str + len;
	*ptr++ = '\0';
	num_len = 0;
	I2A(ptr, num_len, new_trig_index);
	len += num_len + 1;
	SET_TRIGGER_GLOBAL_SUB_SUB_MSUB_MSUB_STR(trigvn, trigvn_len,
		LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_hash_indx, tmp_str, len, result);
	return result;
}

boolean_t trigger_delete_name(char *trigger_name, uint4 trigger_name_len, uint4 *trig_stats)
{
	sgmnt_addrs		*csa;
	char			curr_name[MAX_MIDENT_LEN + 1];
	uint4			curr_name_len, orig_name_len;
	mval			mv_curr_nam;
	char			*ptr;
	char			*name_tail_ptr;
	char			save_name[MAX_MIDENT_LEN + 1];
	gv_key			save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gd_region		*save_gv_cur_region, *lgtrig_reg;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool;
	mval			trig_gbl;
	mval			*trigger_count;
	char			trigvn[MAX_MIDENT_LEN + 1];
	int			trigvn_len;
	int			trig_indx;
	int			badpos;
	boolean_t		wildcard;
	char			utilprefix[1024];
	int			utilprefixlen;
	boolean_t		first_gtmio;
	uint4			triggers_deleted;
	mval			trigjrec;
	boolean_t		jnl_format_done;
	gd_region		*reg, *reg_top;
	char			disp_trigvn[MAX_MIDENT_LEN + SPANREG_REGION_LITLEN + MAX_RN_LEN + 1 + 1];
					/* SPANREG_REGION_LITLEN for " (region ", MAX_RN_LEN for region name,
					 * 1 for ")" and 1 for trailing '\0'.
					 */
	int			disp_trigvn_len;
	int			trig_protected_mval_push_count;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	badpos = 0;
	trigjrec.mvtype = MV_STR;
	trigjrec.str.len = trigger_name_len--;
	trigjrec.str.addr = trigger_name++;
	orig_name_len = trigger_name_len;
	if ((0 == trigger_name_len)
		|| (trigger_name_len != (badpos = validate_input_trigger_name(trigger_name, trigger_name_len, &wildcard))))
	{	/* is the input name valid */
		CONV_STR_AND_PRINT("Invalid trigger NAME string: ", orig_name_len, trigger_name);
		/* badpos is the string position where the bad character was found, pretty print it */
		trig_stats[STATS_ERROR_TRIGFILE]++;
		return TRIG_FAILURE;
	}
	if (trig_stats[STATS_ERROR_TRIGFILE])
	{	/* If an error has occurred during trigger processing the above validity check is all we need */
		CONV_STR_AND_PRINT("No errors processing trigger delete by name: ", orig_name_len, trigger_name);
		/* Return success because there was no error with the name to delete */
		return TRIG_SUCCESS;
	}
	name_tail_ptr = trigger_name + trigger_name_len - 1;
	if ((TRIGNAME_SEQ_DELIM == *name_tail_ptr) || wildcard)
		trigger_name_len--; /* drop the trailing # sign for wildcard */
	jnl_format_done = FALSE;
	lgtrig_reg = NULL;
	first_gtmio = TRUE;
	triggers_deleted = 0;
	assert(trigger_name_len < MAX_MIDENT_LEN);
	memcpy(save_name, trigger_name, trigger_name_len);
	save_name[trigger_name_len] = '\0';
	utilprefixlen = ARRAYSIZE(utilprefix);
	trig_protected_mval_push_count = 0;
	INCR_AND_PUSH_MV_STENT(trigger_count); /* Protect trigger_count from garbage collection */
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions; reg < reg_top; reg++)
	{
		if (IS_STATSDB_REGNAME(reg))
			continue;
		GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
		csa = cs_addrs;
		if (NULL == csa)	/* not BG or MM access method */
			continue;
		/* gv_target now points to ^#t in region "reg" */
		DISALLOW_MULTIINST_UPDATE_IN_TP(dollar_tlevel, jnlpool_head, csa, first_sgm_info, TRUE);
		/* To write the LGTRIG logical jnl record, choose some region that has journaling enabled */
		if (!reg->read_only && !jnl_format_done && JNL_WRITE_LOGICAL_RECS(csa))
			lgtrig_reg = reg;
		if (!gv_target->root)
			continue;
		memcpy(curr_name, save_name, trigger_name_len);
		curr_name_len = trigger_name_len;
		do {
			/* GVN = $get(^#t("#TNAME",curr_name)) */
			BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), curr_name, curr_name_len);
			if (gvcst_get(&trig_gbl))
			{
				if (reg->read_only)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(reg));
				SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
				ptr = trig_gbl.str.addr;
				trigvn_len = MIN(trig_gbl.str.len, MAX_MIDENT_LEN);
				STRNLEN(ptr, trigvn_len, trigvn_len);
				ptr += trigvn_len;
				if ((trig_gbl.str.len == trigvn_len) || ('\0' != *ptr))
				{	/* We expect $c(0) in the middle of ptr. If not found, this is a restartable situation */
					if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
						t_retry(cdb_sc_triggermod);
					assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TRIGNAMBAD, 4, LEN_AND_LIT("\"#TNAME\""),
							curr_name_len, curr_name);
				}
				memcpy(trigvn, trig_gbl.str.addr, trigvn_len);
				/* the index is just beyond the length of the GVN string */
				ptr++;
				A2I(ptr, trig_gbl.str.addr + trig_gbl.str.len, trig_indx);
				if (1 > trig_indx)
				{	/* Trigger indexes start from 1 */
					if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
						t_retry(cdb_sc_triggermod);
					assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TRIGNAMBAD, 4, LEN_AND_LIT("\"#TNAME\""),
							curr_name_len, curr_name);
				}
				SET_DISP_TRIGVN(reg, disp_trigvn, disp_trigvn_len, trigvn, trigvn_len);
				/* $get(^#t(GVN,"COUNT") */
				BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
				if (!gvcst_get(trigger_count))
				{	/* We just looked this up, if it doesn't exist then assume a concurrent update occurred */
					if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
						t_retry(cdb_sc_triggermod);
					assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
					rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6,
							trigvn_len, trigvn, trigvn_len, trigvn, LEN_AND_LIT("\"#COUNT\""));
				}
				if (!jnl_format_done && JNL_WRITE_LOGICAL_RECS(csa))
				{
					jnl_format(JNL_LGTRIG, NULL, &trigjrec, 0);
					jnl_format_done = TRUE;
				}
				/* kill the target trigger for GVN at index trig_indx */
				if (PUT_SUCCESS != (trigger_delete(trigvn, trigvn_len, trigger_count, trig_indx)))
				{
					UTIL_PRINT_PREFIX_IF_NEEDED(first_gtmio, utilprefix, &utilprefixlen);
					util_out_print_gtmio("Trigger named !AD exists in the lookup table for global ^!AD,"	\
								" but was not deleted!", FLUSH, orig_name_len, trigger_name,
								disp_trigvn_len, disp_trigvn);
					trig_stats[STATS_ERROR_TRIGFILE]++;
					RETURN_AND_POP_MVALS(TRIG_FAILURE);
				} else
				{
					csa->incr_db_trigger_cycle = TRUE;
					trigger_incr_cycle(trigvn, trigvn_len);	/* ^#t records changed, increment cycle */
					if (dollar_ztrigger_invoked)
					{	/* Increment db_dztrigger_cycle so that next gvcst_put/gvcst_kill in this
						 * transaction, on this region, will re-read triggers. See trigger_update.c
						 * for a comment on why it is okay for db_dztrigger_cycle to be incremented
						 * more than once in the same transaction.
						 */
						DBGTRIGR((stderr, "trigger_delete_name: CSA->db_dztrigger_cycle=%d\n",
									csa->db_dztrigger_cycle));
						csa->db_dztrigger_cycle++;
					}
					trig_stats[STATS_DELETED]++;
					if (0 == trig_stats[STATS_ERROR_TRIGFILE])
					{
						UTIL_PRINT_PREFIX_IF_NEEDED(first_gtmio, utilprefix, &utilprefixlen);
						util_out_print_gtmio("Deleted trigger named '!AD' for global ^!AD",
								FLUSH, curr_name_len, curr_name, disp_trigvn_len, disp_trigvn);
					}
				}
				trigger_count->mvtype = 0; /* allow stp_gcol to release the current contents if necessary */
				RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr,
							save_jnlpool);
				triggers_deleted++;
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
		} while (TRUE);
	}
	DECR_AND_POP_MV_STENT();
	if (!jnl_format_done && (NULL != lgtrig_reg))
	{	/* There was no journaled region that had a ^#t update, but found at least one journaled region
		 * so write a LGTRIG logical jnl record there.
		 */
		GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(lgtrig_reg);
		csa = cs_addrs;
		/* Attach to jnlpool. Normally SET or KILL of the ^#t records take care of this but in
		 * case this is a NO-OP trigger operation that wont update any ^#t records and we still
		 * want to write a TLGTRIG/ULGTRIG journal record. Hence the need to do this.
		 */
		JNLPOOL_INIT_IF_NEEDED(csa, csa->hdr, csa->nl, SCNDDBNOUPD_CHECK_TRUE);
		assert(dollar_tlevel);
		/* below is needed to set update_trans TRUE on this region even if NO db updates happen to ^#t nodes */
		T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_TRIGLOADFAIL);
		jnl_format(JNL_LGTRIG, NULL, &trigjrec, 0);
		jnl_format_done = TRUE;
	}
	if (wildcard)
	{
		UTIL_PRINT_PREFIX_IF_NEEDED(first_gtmio, utilprefix, &utilprefixlen);
		if (triggers_deleted)
		{
			trig_stats[STATS_NOERROR_TRIGFILE]++;
			util_out_print_gtmio("All existing triggers named !AD (count = !UL) now deleted",
				FLUSH, orig_name_len, trigger_name, triggers_deleted);
		} else
		{
			trig_stats[STATS_UNCHANGED_TRIGFILE]++;
			util_out_print_gtmio("No matching triggers of the form !AD found for deletion",
				FLUSH, orig_name_len, trigger_name);
		}
	} else if (triggers_deleted)
	{
		/* util_out_print_gtmio of "Deleted trigger named ..." already done so no need to do it again */
		trig_stats[STATS_NOERROR_TRIGFILE]++;
	} else
	{	/* No names match. But treat it as a no-op (i.e. success). */
		UTIL_PRINT_PREFIX_IF_NEEDED(first_gtmio, utilprefix, &utilprefixlen);
		util_out_print_gtmio("Trigger named !AD does not exist", FLUSH, orig_name_len, trigger_name);
		trig_stats[STATS_UNCHANGED_TRIGFILE]++;
	}
	return TRIG_SUCCESS;
}

int4 trigger_delete(char *trigvn, int trigvn_len, mval *trigger_count, int index)
{
	int			count;
	mval			mv_val;
	mval			*mv_val_ptr;
	char			*ptr1;
	int4			result;
	int4			retval;
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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!gv_cur_region->read_only);		/* caller should have already checked this */
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
	mv_val_ptr = &mv_val;
	MV_FORCE_UMVAL(&trigger_index, index);
	count = MV_FORCE_UINT(trigger_count);
	/* build up array of values - needed for comparison in hash stuff */
	ptr1 = tmp_trig_str;
	memcpy(ptr1, trigvn, trigvn_len);
	ptr1 += trigvn_len;
	*ptr1++ = '\0';
	tmp_len = trigvn_len + 1;
	/* Assert that BHASH and LHASH are not part of NUM_SUBS calculation (confirms the -2 done in the #define of NUM_SUBS) */
	assert(BHASH_SUB == NUM_SUBS);
	assert(LHASH_SUB == (NUM_SUBS + 1));
	for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
	{
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[sub_indx],
						 STRLEN(trigger_subs[sub_indx]));
		trig_len = gvcst_get(&trigger_value) ? trigger_value.str.len : 0;
		if (0 == trig_len)
		{
			if (((TRIGNAME_SUB == sub_indx) || (CMD_SUB == sub_indx) || (CHSET_SUB == sub_indx)))
			{ /* CMD, NAME and CHSET cannot be zero length */
				if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
					t_retry(cdb_sc_triggermod);
				assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
						trigvn_len, trigvn, STRLEN(trigger_subs[sub_indx]), trigger_subs[sub_indx]);
			}
			tt_val[sub_indx] = NULL;
			tt_val_len[sub_indx] = 0;
			continue;
		}
		if (TRIGNAME_SUB == sub_indx)
		{
			trig_name_len = MIN(trig_len, MAX_TRIGNAME_LEN);
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
	/* $get(^#t(GVN,trigger_index,"LHASH") for deletion in cleanup_trigger_hash */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[LHASH_SUB],
		STRLEN(trigger_subs[LHASH_SUB]));
	if (gvcst_get(mv_val_ptr))
		kill_hash.hash_code = (uint4)MV_FORCE_UINT(mv_val_ptr);
	else
	{
		util_out_print_gtmio("The LHASH for global ^!AD does not exist", FLUSH, trigvn_len, trigvn);
		kill_hash.hash_code = 0;
	}
	/* $get(^#t(GVN,trigger_index,"BHASH") for deletion in cleanup_trigger_hash */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigger_index, trigger_subs[BHASH_SUB],
		STRLEN(trigger_subs[BHASH_SUB]));
	if (gvcst_get(mv_val_ptr))
		set_hash.hash_code = (uint4)MV_FORCE_UINT(mv_val_ptr);
	else
	{
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
		cleanup_trigger_hash(trigvn, trigvn_len, tt_val, tt_val_len, &set_hash, &kill_hash, TRUE, index);
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
			tmp_len = trigvn_len + 1;
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
				{	/* multi line trigger broken up because it exceeds record size */
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
						if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
							t_retry(cdb_sc_triggermod);
						assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
						rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD,
								6, trigvn_len, trigvn, trigvn_len, trigvn,
								STRLEN(trigger_subs[sub_indx]), trigger_subs[sub_indx]);
					}
					/* OPTIONS, PIECES and DELIM can be zero */
					trig_len = 0;
				}
				if (NUM_SUBS > sub_indx)
				{
					tt_val[sub_indx] = ptr1;
					tt_val_len[sub_indx] = trig_len;
					tmp_len += trig_len;
					if (0 < trig_len)
					{
						if (MAX_BUFF_SIZE <= tmp_len)
						{ /* Exceeding the temporary buffer is impossible, restart*/
							if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
								t_retry(cdb_sc_triggermod);
							assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
							rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD,
									6, trigvn_len, trigvn, trigvn_len, trigvn,
									STRLEN(trigger_subs[sub_indx]), trigger_subs[sub_indx]);
						}
						memcpy(ptr1, trigger_value.str.addr, trig_len);
						ptr1 += trig_len;
					}
					*ptr1++ = '\0';
					tmp_len++;
				}
			}
			/* $get(^#t(GVN,trigger_count,"LHASH") for update_trigger_hash_value */
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[LHASH_SUB],
							 STRLEN(trigger_subs[LHASH_SUB]));
			if (!gvcst_get(mv_val_ptr))
				return PUT_SUCCESS;
			kill_hash.hash_code = (uint4)MV_FORCE_UINT(mv_val_ptr);
			/* $get(^#t(GVN,trigger_count,"BHASH") for update_trigger_hash_value */
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, *trigger_count, trigger_subs[BHASH_SUB],
							 STRLEN(trigger_subs[BHASH_SUB]));
			if (!gvcst_get(mv_val_ptr))
				return PUT_SUCCESS;
			set_hash.hash_code = (uint4)MV_FORCE_UINT(mv_val_ptr);
			/* update hash values from above */
			if (VAL_TOO_LONG == (retval = update_trigger_hash_value(trigvn, trigvn_len, tt_val, tt_val_len,
					&set_hash, &kill_hash, count, index)))
				return VAL_TOO_LONG;
			/* fix the value ^#t("#TNAME",^#t(GVN,index,"#TRIGNAME")) to point to the correct "index" */
			if (VAL_TOO_LONG == (retval = update_trigger_name_value(tt_val[TRIGNAME_SUB],
					tt_val_len[TRIGNAME_SUB], index)))
				return VAL_TOO_LONG;
			/* kill ^#t(GVN,COUNT) which was just shifted to trigger_index */
			BUILD_HASHT_SUB_MSUB_CURRKEY(trigvn, trigvn_len, *trigger_count);
			gvcst_kill(TRUE);
		}
		/* Update #COUNT */
		count--;
		MV_FORCE_UMVAL(trigger_count, count);
		SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT), *trigger_count,
			result);
		assert(PUT_SUCCESS == result);		/* Size of count can only get shorter or stay the same */
	}
	return PUT_SUCCESS;
}

void trigger_delete_all(char *trigger_rec, uint4 len, uint4 *trig_stats)
{
	int			count;
	sgmnt_addrs		*csa;
	mval			curr_gbl_name;
	mval			after_hash_cycle;
	int			cycle;
	mval			*mv_count_ptr;
	mval			*mv_cycle_ptr;
	gd_region		*reg, *reg_top;
	int4			result;
	gd_region		*lgtrig_reg;
	mval			trigger_cycle;
	mval			trigger_count;
	boolean_t		this_db_updated;
	uint4			triggers_deleted;
	mval			trigjrec;
	boolean_t		jnl_format_done;
	boolean_t		delete_required;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 < dollar_tlevel);
	jnl_format_done = FALSE;
	lgtrig_reg = NULL;
	trigjrec.mvtype = MV_STR;
	trigjrec.str.len = len;
	trigjrec.str.addr = trigger_rec;
	triggers_deleted = 0;
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions; reg < reg_top; reg++)
	{
		if (IS_STATSDB_REGNAME(reg))
			continue;
		GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
		csa = cs_addrs;
		if (NULL == csa)	/* not BG or MM access method */
			continue;
		/* gv_target now points to ^#t in region "reg" */
		DISALLOW_MULTIINST_UPDATE_IN_TP(dollar_tlevel, jnlpool_head, csa, first_sgm_info, TRUE);
		/* To write the LGTRIG logical jnl record, choose some region that has journaling enabled */
		if (!reg->read_only && !jnl_format_done && JNL_WRITE_LOGICAL_RECS(csa))
			lgtrig_reg = reg;
		if (!gv_target->root)
			continue;
		/* kill ^#t("#TNAME") */
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME));
		if (0 != gvcst_data())
		{	/* Issue error if we dont have permissions to touch ^#t global */
			if (reg->read_only)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(reg));
			gvcst_kill(TRUE);
		}
		/* Kill all descendents of ^#t(trigvn, ...) where trigvn is any global with a trigger,
		 * but skip the ^#t("#...",...) entries. Setup ^#t("$") as the key for op_gvorder
		 */
		BUILD_HASHT_SUB_CURRKEY(LITERAL_MAXHASHVAL, STRLEN(LITERAL_MAXHASHVAL));
		TREF(gv_last_subsc_null) = FALSE; /* We know its not null, but prior state is unreliable */
		this_db_updated = FALSE;
		while (TRUE)
		{
			op_gvorder(&curr_gbl_name);
			/* quit:$length(curr_gbl_name)=0 */
			if (0 == curr_gbl_name.str.len)
				break;
			count = cycle = 0;
			/* $get(^#t(curr_gbl_name,#COUNT)) */
			BUILD_HASHT_SUB_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len,
							LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
			if (TRUE == (delete_required = gvcst_get(&trigger_count))) /* inline assignment */
			{
				mv_count_ptr = &trigger_count;
				count = MV_FORCE_UINT(mv_count_ptr);
			}
			/* $get(^#t(curr_gbl_name,#CYCLE)) */
			BUILD_HASHT_SUB_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len,
				LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE));
			if (!gvcst_get(&trigger_cycle))
			{	/* Found hasht record, there must be #CYCLE */
				if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
					t_retry(cdb_sc_triggermod);
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(12) MAKE_MSG_WARNING(ERR_TRIGDEFBAD), 6,
						curr_gbl_name.str.len, curr_gbl_name.str.addr,
						curr_gbl_name.str.len, curr_gbl_name.str.addr, LEN_AND_LIT("\"#CYCLE\""),
						ERR_TEXT, 2, RTS_ERROR_TEXT("#CYCLE field is missing"));
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(12) MAKE_MSG_WARNING(ERR_TRIGDEFBAD), 6,
						curr_gbl_name.str.len, curr_gbl_name.str.addr,
						curr_gbl_name.str.len, curr_gbl_name.str.addr, LEN_AND_LIT("\"#CYCLE\""),
						ERR_TEXT, 2, RTS_ERROR_TEXT("#CYCLE field is missing"));
				assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			} else
			{
				mv_cycle_ptr = &trigger_cycle;
				cycle = MV_FORCE_UINT(mv_cycle_ptr);
				cycle++;
				MV_FORCE_MVAL(&trigger_cycle, cycle);
			}
			if (!delete_required)
			{	/* $order(^#t(curr_gbl_name,#LABEL)); should be the NULL string if #COUNT not found
				 * Use #LABEL vs #CYCLE because MUPIP TRIGGER -UPGRADE unconditionally inserts it */
				BUILD_HASHT_SUB_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len,
					LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
				op_gvorder(&after_hash_cycle);
				/* quit:$length(after_hash_cycle)=0 */
				if (0 != after_hash_cycle.str.len)
				{	/* Found hasht record after #LABEL, but #COUNT is not defined; Force removal */
					if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
						t_retry(cdb_sc_triggermod);
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(12) MAKE_MSG_WARNING(ERR_TRIGDEFBAD), 6,
							curr_gbl_name.str.len, curr_gbl_name.str.addr,
							curr_gbl_name.str.len, curr_gbl_name.str.addr, LEN_AND_LIT("\"#COUNT\""),
							ERR_TEXT, 2, RTS_ERROR_TEXT("#COUNT field is missing. Skipped in results"));
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(12) MAKE_MSG_WARNING(ERR_TRIGDEFBAD), 6,
							curr_gbl_name.str.len, curr_gbl_name.str.addr,
							curr_gbl_name.str.len, curr_gbl_name.str.addr, LEN_AND_LIT("\"#COUNT\""),
							ERR_TEXT, 2, RTS_ERROR_TEXT("#COUNT field is missing. Skipped in results"));
					assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
					delete_required = TRUE;
				}
			}
			if (delete_required)
			{
				/* Now that we know there is something to kill, check if we have permissions to touch ^#t global */
				if (reg->read_only)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(reg));
				if (!jnl_format_done && JNL_WRITE_LOGICAL_RECS(csa))
				{
					jnl_format(JNL_LGTRIG, NULL, &trigjrec, 0);
					jnl_format_done = TRUE;
				}
				/* kill ^#t(curr_gbl_name); kills ^#t(curr_gbl_name,"#TRHASH") as well */
				BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
				gvcst_kill(TRUE);
				if (0 < cycle)
				{
					/* set ^#t(curr_gbl_name,#CYCLE)=trigger_cycle */
					SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(curr_gbl_name.str.addr, curr_gbl_name.str.len,
						LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE), trigger_cycle, result);
					assert(PUT_SUCCESS == result);
				}
				this_db_updated = TRUE;
				triggers_deleted += count;
			} /* else there is nothing under the hasht record, leave #CYCLE alone */
			/* get ready for op_gvorder() call for next trigger under ^#t */
			BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
		}
		if (this_db_updated)
		{
			csa->incr_db_trigger_cycle = TRUE;
			if (dollar_ztrigger_invoked)
			{	/* increment db_dztrigger_cycle so that next gvcst_put/gvcst_kill in this transaction,
				 * on this region, will re-read. See trigger_update.c for a comment on why it is okay
				 * for db_dztrigger_cycle to be incremented more than once in the same transaction
				 */
				DBGTRIGR((stderr, "trigger_delete_all: CSA->db_dztrigger_cycle=%d\n",
					  csa->db_dztrigger_cycle));
				csa->db_dztrigger_cycle++;
			}
		}
	}
	if (!jnl_format_done && (NULL != lgtrig_reg))
	{	/* There was no journaled region that had a ^#t update, but found at least one journaled region
		 * so write a LGTRIG logical jnl record there.
		 */
		GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(lgtrig_reg);
		csa = cs_addrs;
		JNLPOOL_INIT_IF_NEEDED(csa, csa->hdr, csa->nl, SCNDDBNOUPD_CHECK_TRUE);	/* see previous use for why it is needed */
		assert(dollar_tlevel);
		T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_TRIGLOADFAIL);	/* needed to set update_trans TRUE on this region
									 * even if NO db updates happen to ^#t nodes. */
		jnl_format(JNL_LGTRIG, NULL, &trigjrec, 0);
		jnl_format_done = TRUE;
	}
	if (triggers_deleted)
	{
		util_out_print_gtmio("All existing triggers (count = !UL) deleted", FLUSH, triggers_deleted);
		trig_stats[STATS_DELETED] += triggers_deleted;
		trig_stats[STATS_NOERROR_TRIGFILE]++;
	} else
	{
		util_out_print_gtmio("No matching triggers found for deletion", FLUSH);
		trig_stats[STATS_UNCHANGED_TRIGFILE]++;
	}
}
#endif /* GTM_TRIGGER */
