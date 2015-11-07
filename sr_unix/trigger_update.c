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
#include "dpgbldir.h"
#include "gvcst_protos.h"
#include <rtnhdr.h>
#include "io.h"
#include "iormdef.h"
#include "hashtab_str.h"
#include "wbox_test_init.h"
#include "gv_trigger.h"
#include "trigger_delete_protos.h"
#include "trigger.h"
#include "trigger_incr_cycle.h"
#include "trigger_parse_protos.h"
#include "trigger_update_protos.h"
#include "trigger_compare_protos.h"
#include "trigger_user_name.h"
#include "gtm_string.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "targ_alloc.h"			/* for SETUP_TRIGGER_GLOBAL & SWITCH_TO_DEFAULT_REGION */
#include "gdsblk.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "min_max.h"			/* Needed for MIN */
#include "mvalconv.h"			/* Needed for MV_FORCE_* */
#include "op.h"
#include "util.h"
#include "op_tcommit.h"
#include "tp_restart.h"
#include "error.h"
#include "file_input.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "t_retry.h"
#include "gtmimagename.h"

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_addr			*gd_header;
GBLREF	io_pair			io_curr_device;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	int			tprestart_state;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		dollar_ztrigger_invoked;
GBLREF	trans_num		local_tn;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

error_def(ERR_DBROLLEDBACK);
error_def(ERR_TEXT);
error_def(ERR_TPRETRY);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGDEFBAD);
error_def(ERR_TRIGMODINTP);
error_def(ERR_TRIGMODREGNOTRW);

LITREF	mval			gvtr_cmd_mval[GVTR_CMDTYPES];
LITREF	int4			gvtr_cmd_mask[GVTR_CMDTYPES];
LITREF	mval			literal_hasht;
LITREF	mval			literal_one;
LITREF	char 			*trigger_subs[];

#define	MAX_COMMANDS_LEN	32		/* Need room for S,K,ZK,ZTK + room for expansion */
#define	MAX_OPTIONS_LEN		32		/* Need room for NOI,NOC + room for expansion */
#define	MAX_TRIGNAME_SEQ_NUM	999999
#define	LITERAL_M		"M"
#define	OPTIONS_I		1
#define	OPTIONS_NOI		2
#define	OPTIONS_C		4
#define	OPTIONS_NOC		8

#define	ADD_UPDATE_NOCHANGE	0x00
#define	ADD_UPDATE_NAME		0x01
#define	ADD_UPDATE_CMDS		0x02
#define	ADD_UPDATE_OPTIONS	0x04
#define	SUB_UPDATE_NAME		0x10
#define	SUB_UPDATE_CMDS		0x20
#define	SUB_UPDATE_OPTIONS	0x40
#define	SUB_UPDATE_NOCHANGE	0x00
#define	DELETE_REC		0x80

#define	BUILD_COMMAND_BITMAP(BITMAP, COMMANDS)									\
{														\
	char		lcl_cmds[MAX_COMMANDS_LEN + 1];								\
	char		*lcl_ptr, *strtok_ptr;									\
														\
	memcpy(lcl_cmds, COMMANDS, STRLEN(COMMANDS) + 1);							\
	BITMAP = 0;												\
	lcl_ptr = strtok_r(lcl_cmds, ",", &strtok_ptr);								\
	do													\
	{													\
		switch (*lcl_ptr)										\
		{												\
			case 'S':										\
				BITMAP |= gvtr_cmd_mask[GVTR_CMDTYPE_SET];					\
				break;										\
			case 'K':										\
				BITMAP |= gvtr_cmd_mask[GVTR_CMDTYPE_KILL];					\
				break;										\
			case 'Z':										\
				switch (*(lcl_ptr + 1))								\
				{										\
					case 'K':								\
						BITMAP |= gvtr_cmd_mask[GVTR_CMDTYPE_ZKILL];			\
						break;								\
					case 'T':								\
						switch(*(lcl_ptr + 2))						\
						{								\
							case 'K':						\
								BITMAP |= gvtr_cmd_mask[GVTR_CMDTYPE_ZTKILL];	\
							        break;						\
							case 'R':						\
								BITMAP |= gvtr_cmd_mask[GVTR_CMDTYPE_ZTRIGGER];	\
								break;						\
							default:						\
							        GTMASSERT;					\
								break;						\
						}								\
						break;								\
					default:								\
						GTMASSERT;	/* Parsing should have found invalid command */ \
						break;								\
				}										\
				break;										\
			default:										\
				GTMASSERT;	/* Parsing should have found invalid command */			\
				break;										\
		}												\
	} while (lcl_ptr = strtok_r(NULL, ",", &strtok_ptr));							\
}

#define	COMMAND_BITMAP_TO_STR(COMMANDS, BITMAP, LEN)								\
{														\
	int		count, cmdtype, lcl_len;								\
	char		*lcl_ptr;										\
														\
	count = 0;												\
	lcl_ptr = COMMANDS;											\
	lcl_len = LEN;												\
	for (cmdtype = 0; cmdtype < GVTR_CMDTYPES; cmdtype++)							\
	{													\
		if (gvtr_cmd_mask[cmdtype] & (BITMAP))								\
		{												\
			ADD_COMMA_IF_NEEDED(count, lcl_ptr, lcl_len);						\
			ADD_STRING(count, lcl_ptr, gvtr_cmd_mval[cmdtype].str.len, gvtr_cmd_mval[cmdtype].str.addr, lcl_len); \
		}												\
	}													\
	*lcl_ptr = '\0';											\
	LEN = STRLEN(COMMANDS);											\
}

#define	BUILD_OPTION_BITMAP(BITMAP, OPTIONS)									\
{														\
	char		lcl_options[MAX_OPTIONS_LEN + 1];							\
	char		*lcl_ptr, *strtok_ptr;									\
														\
	memcpy(lcl_options, OPTIONS, STRLEN(OPTIONS) + 1);							\
	BITMAP = 0;												\
	lcl_ptr = strtok_r(lcl_options, ",", &strtok_ptr);							\
	if (NULL != lcl_ptr)											\
		do												\
		{												\
			switch (*lcl_ptr)									\
			{											\
				case 'C':									\
					BITMAP |= OPTIONS_C;							\
					break;									\
				case 'I':									\
					BITMAP |= OPTIONS_I;							\
					break;									\
				case 'N':									\
					assert('O' == *(lcl_ptr + 1));						\
					switch (*(lcl_ptr + 2))							\
					{									\
						case 'C':							\
							BITMAP |= OPTIONS_NOC;					\
							break;							\
						case 'I':							\
							BITMAP |= OPTIONS_NOI;					\
							break;							\
						default:							\
							GTMASSERT;	/* Parsing should have found invalid command */ \
							break;							\
					}									\
					break;									\
				default:									\
					GTMASSERT;	/* Parsing should have found invalid command */		\
					break;									\
			}											\
		} while (lcl_ptr = strtok_r(NULL, ",", &strtok_ptr));						\
}

#define	OPTION_BITMAP_TO_STR(OPTIONS, BITMAP, LEN)								\
{														\
	int		count, optiontype, lcl_len;								\
	char		*lcl_ptr;										\
														\
	count = 0;												\
	lcl_len = LEN;												\
	lcl_ptr = OPTIONS;											\
	if (OPTIONS_I & BITMAP)											\
	{													\
		ADD_COMMA_IF_NEEDED(count, lcl_ptr, lcl_len);							\
		ADD_STRING(count, lcl_ptr, STRLEN(HASHT_OPT_ISOLATION), HASHT_OPT_ISOLATION, lcl_len);		\
	}													\
	if (OPTIONS_NOI & BITMAP)										\
	{													\
		ADD_COMMA_IF_NEEDED(count, lcl_ptr, lcl_len);							\
		ADD_STRING(count, lcl_ptr, STRLEN(HASHT_OPT_NOISOLATION), HASHT_OPT_NOISOLATION, lcl_len);	\
	}													\
	if (OPTIONS_C & BITMAP)											\
	{													\
		ADD_COMMA_IF_NEEDED(count, lcl_ptr, lcl_len);							\
		ADD_STRING(count, lcl_ptr, STRLEN(HASHT_OPT_CONSISTENCY), HASHT_OPT_CONSISTENCY, lcl_len); 	\
	}													\
	if (OPTIONS_NOC & BITMAP)										\
	{													\
		ADD_COMMA_IF_NEEDED(count, lcl_ptr, lcl_len);							\
		ADD_STRING(count, lcl_ptr, STRLEN(HASHT_OPT_NOCONSISTENCY), HASHT_OPT_NOCONSISTENCY, lcl_len);	\
	}													\
	*lcl_ptr = '\0';											\
	LEN = STRLEN(OPTIONS);											\
}

#define TOO_LONG_REC_KEY_ERROR_MSG										\
{														\
	trig_stats[STATS_ERROR]++;										\
	if (KEY_TOO_LONG == result)										\
		util_out_print_gtmio("^!AD trigger - key larger than max key size", FLUSH, trigvn_len, trigvn);	\
	else													\
		util_out_print_gtmio("^!AD trigger - value larger than max record size", FLUSH, trigvn_len, trigvn);	\
}

#define IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(RESULT)						\
{														\
	if (PUT_SUCCESS != RESULT)										\
	{													\
		TOO_LONG_REC_KEY_ERROR_MSG;									\
		return TRIG_FAILURE;										\
	}													\
}

#define TRIGGER_SAME_NAME_EXISTS_ERROR												\
{																\
	trig_stats[STATS_ERROR]++;												\
	util_out_print_gtmio("a trigger named !AD already exists", FLUSH, value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);	\
	return TRIG_FAILURE;													\
}

/* This error macro is used for all definition errors where the target is ^#t(GVN,<index>,<required subscript>) */
#define HASHT_GVN_DEFINITION_RETRY_OR_ERROR(INDEX,SUBSCRIPT,CSA)				\
{												\
	if (CDB_STAGNATE > t_tries)								\
		t_retry(cdb_sc_triggermod);							\
	else											\
	{											\
		assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);		\
		/* format "INDEX,SUBSCRIPT" of ^#t(GVN,INDEX,SUBSCRIPT) in the error message */	\
		SET_PARAM_STRING(util_buff, util_len, INDEX, SUBSCRIPT);			\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,	\
			trigvn_len, trigvn, util_len, util_buff);				\
	}											\
}

/* This error macro is used for all definition errors where the target is ^#t(GVN,<#LABEL|#COUNT|#CYCLE>) */
#define HASHT_DEFINITION_RETRY_OR_ERROR(SUBSCRIPT,MOREINFO,CSA)	\
{								\
	if (CDB_STAGNATE > t_tries)				\
		t_retry(cdb_sc_triggermod);			\
	else							\
	{							\
		HASHT_DEFINITION_ERROR(SUBSCRIPT,MOREINFO,CSA);	\
	}							\
}

#define HASHT_DEFINITION_ERROR(SUBSCRIPT,MOREINFO,CSA)						\
{												\
	assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);			\
	rts_error_csa(CSA_ARG(CSA) VARLSTCNT(12) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,		\
		trigvn_len, trigvn, LEN_AND_LIT(SUBSCRIPT),					\
		ERR_TEXT, 2, RTS_ERROR_TEXT(MOREINFO));						\
}

STATICFNDEF boolean_t validate_label(char *trigvn, int trigvn_len)
{
	mval			trigger_label;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
	if (!gvcst_get(&trigger_label)) /* There has to be a #LABEL */
		HASHT_DEFINITION_RETRY_OR_ERROR("\"#LABEL\"","#LABEL was not found", REG2CSA(gv_cur_region))
	return ((trigger_label.str.len == STRLEN(HASHT_GBL_CURLABEL))
		&& (0 == memcmp(trigger_label.str.addr, HASHT_GBL_CURLABEL, trigger_label.str.len)));
}

STATICFNDEF int4 update_commands(char *trigvn, int trigvn_len, int trigger_index, char *new_trig_cmds, char *orig_db_cmds)
{
	mval			mv_trig_indx;
	uint4			orig_cmd_bm, new_cmd_bm;
	int4			result;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	BUILD_COMMAND_BITMAP(orig_cmd_bm, orig_db_cmds);
	BUILD_COMMAND_BITMAP(new_cmd_bm, new_trig_cmds);
	i2mval(&mv_trig_indx, trigger_index);
	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[CMD_SUB], STRLEN(trigger_subs[CMD_SUB]),
		new_trig_cmds, STRLEN(new_trig_cmds), result);
	if (PUT_SUCCESS != result)
		return result;
	if ((0 != (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & orig_cmd_bm)) && (0 == (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & new_cmd_bm)))
	{	/* SET was removed from the commands, so delete the SET specific attributes */
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx, LITERAL_DELIM, LITERAL_DELIM_LEN);
		gvcst_kill(TRUE);
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx, LITERAL_ZDELIM, LITERAL_ZDELIM_LEN);
		gvcst_kill(TRUE);
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx, LITERAL_PIECES, LITERAL_PIECES_LEN);
		gvcst_kill(TRUE);
	}
	trigger_incr_cycle(trigvn, trigvn_len);
	return SUB_UPDATE_CMDS;
}

STATICFNDEF int4 update_options(char *trigvn, int trigvn_len, int trigger_index, char *trig_options, char *option_value)
{
	mval			mv_trig_indx;
	int4			result;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	i2mval(&mv_trig_indx, trigger_index);
	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[OPTIONS_SUB],
		STRLEN(trigger_subs[OPTIONS_SUB]), trig_options, STRLEN(trig_options), result);
	if (PUT_SUCCESS != result)
		return result;
	trigger_incr_cycle(trigvn, trigvn_len);
	return SUB_UPDATE_OPTIONS;
}

STATICFNDEF int4 update_trigger_name(char *trigvn, int trigvn_len, int trigger_index, char *db_trig_name, char *tf_trig_name,
				     uint4 tf_trig_name_len)
{
	mval			mv_trig_indx;
	int4			result;
	uint4			retval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	retval = NO_NAME_CHANGE;
	if ((0 != tf_trig_name_len) && (tf_trig_name_len != STRLEN(db_trig_name) - 1)
		|| (0 != memcmp(tf_trig_name, db_trig_name, tf_trig_name_len)))
	{
		if (!validate_label(trigvn, trigvn_len))
			return INVALID_LABEL;
		i2mval(&mv_trig_indx, trigger_index);
		tf_trig_name[tf_trig_name_len++] = TRIGNAME_SEQ_DELIM;
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, LITERAL_TRIGNAME, STRLEN(LITERAL_TRIGNAME),
			tf_trig_name, tf_trig_name_len, result);
		if (PUT_SUCCESS != result)
			return result;
		cleanup_trigger_name(trigvn, trigvn_len, db_trig_name, STRLEN(db_trig_name));
		trigger_incr_cycle(trigvn, trigvn_len);
		retval = ADD_UPDATE_NAME;
	}
	return retval;
}

STATICFNDEF boolean_t check_unique_trigger_name(char *trigvn, int trigvn_len, char *trigger_name, uint4 trigger_name_len)
{
	sgmnt_addrs		*csa;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		status;
	mval			val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* We only check user supplied names for uniqueness (since autogenerated names are unique). */
	if (0 == trigger_name_len)
		return TRUE;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 == gv_target->root)
	{
		RESTORE_TRIGGER_REGION_INFO;
		return TRUE;
	}
	assert((MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT) > gv_cur_region->max_rec_size);
	/* $get(^#t("#TNAME",trigger_name) */
	BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trigger_name, trigger_name_len);
	status = !gvcst_get(&val);
	RESTORE_TRIGGER_REGION_INFO;
	return status;
}

STATICFNDEF int4 add_trigger_hash_entry(char *trigvn, int trigvn_len, char *cmd_value, int trigindx, boolean_t add_kill_hash,
		stringkey *kill_hash, stringkey *set_hash)
{
	sgmnt_addrs		*csa;
	int			hash_indx;
	char			indx_str[MAX_DIGITS_IN_INT];
	uint4			len;
	mval			mv_hash;
	mval			mv_indx, *mv_indx_ptr;
	char			name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	int			num_len;
	char			*ptr;
	int4			result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		set_cmp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
 	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	set_cmp = (NULL != strchr(cmd_value, 'S'));
	mv_indx_ptr = &mv_indx;
	num_len = 0;
	I2A(indx_str, num_len, trigindx);
	assert(MAX_MIDENT_LEN >= trigvn_len);
	memcpy(name_and_index, trigvn, trigvn_len);
	ptr = name_and_index + trigvn_len;
	*ptr++ = '\0';
	memcpy(ptr, indx_str, num_len);
	len = trigvn_len + 1 + num_len;
	if (set_cmp)
	{
		MV_FORCE_UMVAL(&mv_hash, set_hash->hash_code);
		if (0 != gv_target->root)
		{
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
			op_zprevious(&mv_indx);
			hash_indx = (0 == mv_indx.str.len) ? 1 : (mval2i(mv_indx_ptr) + 1);
		} else
			hash_indx = 1;
		i2mval(&mv_indx, hash_indx);
		SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_indx,
			name_and_index,	len, result);
		if (PUT_SUCCESS != result)
		{
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	} else
		set_hash->hash_code = 0;
	if (add_kill_hash)
	{
		MV_FORCE_UMVAL(&mv_hash, kill_hash->hash_code);
		if (0 != gv_target->root)
		{
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
			op_zprevious(&mv_indx);
			hash_indx = (0 == mv_indx.str.len) ? 1 : (mval2i(mv_indx_ptr) + 1);
		} else
			hash_indx = 1;
		i2mval(&mv_indx, hash_indx);
		SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_indx,
			name_and_index,	len, result);
		if (PUT_SUCCESS != result)
		{
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	} else
		kill_hash->hash_code = 0;
	RESTORE_TRIGGER_REGION_INFO;
	return PUT_SUCCESS;
}

STATICFNDEF boolean_t trigger_already_exists(char *trigvn, int trigvn_len, char **values, uint4 *value_len, int *set_index,
					     int *kill_index, boolean_t *set_cmp_result, boolean_t *kill_cmp_result,
					     boolean_t *full_match, stringkey *set_trigger_hash, stringkey *kill_trigger_hash,
					     mval *setname, mval *killname)
{
	sgmnt_addrs		*csa;
	boolean_t		db_has_K;
	boolean_t		db_has_S;
	char			*ptr;
	int			hash_indx;
	boolean_t		kill_cmp, kill_found;
	int			kill_indx;
	boolean_t		name_match;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		set_cmp, set_found, set_name_match, kill_name_match;
	int			set_indx;
	mval			trigindx;
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];
	int4			util_len;
	mval			val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	/* test with SET and/or KILL hash */
	set_cmp = (NULL != strchr(values[CMD_SUB], 'S'));
	kill_cmp = ((NULL != strchr(values[CMD_SUB], 'K')) || (NULL != strchr(values[CMD_SUB], 'R')));
	set_found = kill_found = set_name_match = kill_name_match = FALSE;
	if (set_cmp)
	{ /* test for SET hash match if SET command specified */
		set_found = search_triggers(trigvn, trigvn_len, values, value_len, set_trigger_hash, &hash_indx, &set_indx, 0,
				TRUE);
		if (set_found)
		{
			RESTORE_TRIGGER_REGION_INFO;
			i2mval(&trigindx, set_indx);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
					STRLEN(trigger_subs[TRIGNAME_SUB]));
			if (!gvcst_get(setname)) /* There has to be a name value */
				HASHT_GVN_DEFINITION_RETRY_OR_ERROR(set_indx, ",\"TRIGNAME\"", csa);
			set_name_match = ((value_len[TRIGNAME_SUB] == (setname->str.len - 1))
				&& (0 == memcmp(setname->str.addr, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB])));
		}
	}
	*set_cmp_result = set_found;
	if (kill_cmp || !set_found)
	{ /* if SET is not found OR KILL is specified in commands, test for KILL hash match */
		kill_found = search_triggers(trigvn, trigvn_len, values, value_len, kill_trigger_hash, &hash_indx, &kill_indx, 0,
				FALSE);
		if (kill_found)
		{
			RESTORE_TRIGGER_REGION_INFO;
			i2mval(&trigindx, kill_indx);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[CMD_SUB],
							 STRLEN(trigger_subs[CMD_SUB]));
			if (!gvcst_get(&val))	/* There has to be a command string */
				HASHT_GVN_DEFINITION_RETRY_OR_ERROR(kill_indx, ",\"CMD\"", csa);
			db_has_S = (NULL != memchr(val.str.addr, 'S', val.str.len));
			db_has_K = ((NULL != memchr(val.str.addr, 'K', val.str.len)) ||
				    (NULL != memchr(val.str.addr, 'R', val.str.len)));
			/* Below means
			 * NOT ( Matched trigger has SET && New trigger has SET &&
			 * 	NOT ( Matched trigger has SET + KILL && New trigger has SET + KILL )  )
			 *
			 * KILL is found if:
			 * The matched trigger does not have a SET || The new trigger does not have a SET
			 * But not if the matched trigger has a SET or KILL && the new trigger does not have a SET or KILL
			 */
			kill_found = !(db_has_S && set_cmp && !(db_has_S && db_has_K && set_cmp && kill_cmp));
			/* $get(^#t(trigvn,trigindx,"TRIGNAME") */
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
							 STRLEN(trigger_subs[TRIGNAME_SUB]));
			if (!gvcst_get(killname)) /* There has to be a name string */
				HASHT_GVN_DEFINITION_RETRY_OR_ERROR(kill_indx, ",\"TRIGNAME\"", csa);
			kill_name_match = ((value_len[TRIGNAME_SUB] == (killname->str.len - 1))
				&& (0 == memcmp(killname->str.addr, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB])));
		}
	}
	/* Starting from the beginning:
	 *    Matching both set and kill, but for different records -- don't update the kill record, hence the FALSE
	 *    Matching a set implies matching a kill -- hence the ||
	 */
	if (set_found && kill_found && (set_indx != kill_indx))
	{
		*kill_cmp_result = FALSE;
		*kill_index = kill_indx;
	} else
	{
		*kill_cmp_result = (kill_found || set_found);
		if (!set_found)
		{
			setname->mvtype = MV_STR;
			setname->str.addr = killname->str.addr;
			setname->str.len = killname->str.len;
		}
	}
	*set_index = (set_found) ? set_indx : (kill_found) ? kill_indx : 0;
	/* If there is both a set and a kill and the set components don't match, there is no name match no matter if the kill
	 * components match or not.  If there is no set, then the name match is only based on the kill components.
	 */
	*full_match = (set_cmp) ? set_name_match : kill_name_match;
	RESTORE_TRIGGER_REGION_INFO;
	return (set_found || kill_found);
}

STATICFNDEF int4 add_trigger_cmd_attributes(char *trigvn, int trigvn_len,  int trigger_index, char *trig_cmds, char **values,
			uint4 *value_len, boolean_t set_compare, boolean_t kill_compare, stringkey *kill_hash, stringkey *set_hash)
{
	char			cmd_str[MAX_COMMANDS_LEN];
	int			cmd_str_len;
	uint4			db_cmd_bm;
	mval			mv_hash;
	mval			mv_trig_indx;
	int4			result;
	uint4			tf_cmd_bm;
	uint4			tmp_bm;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	BUILD_COMMAND_BITMAP(db_cmd_bm, trig_cmds);
	BUILD_COMMAND_BITMAP(tf_cmd_bm, values[CMD_SUB]);
	/* If the trigger file command string is contained in the database command and either
	 *   1. the trigger file command has no SET components or
	 *   2. the trigger file command matched a database SET component or
	 * then the trigger file command is already in the database, so return.
	 */
	if ((tf_cmd_bm == (db_cmd_bm & tf_cmd_bm))
			&& ((0 == (tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_SET])) || set_compare))
		return CMDS_PRESENT;
	/* If the database command string is contained in the trigger file command and the database is only a "SET"
	 * and the trigger file SET matched the database, but not the KILL (which doesn't make sense until you realize that
	 * trigger_already_exists() returns kill_compare as FALSE when the trigger file record matches both SET and KILL, but
	 * the matches are with two different triggers, then the trigger file command is already in the database so return.
	 */
	if ((db_cmd_bm == (db_cmd_bm & tf_cmd_bm))
			&& ((db_cmd_bm == (db_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_SET])) && set_compare && !kill_compare))
		return CMDS_PRESENT;
	/* If merge would combine K and ZTK, it's an error */
	if (((0 != (db_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_KILL])) && (0 != (tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_ZTKILL])))
	    || ((0 != (db_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_ZTKILL])) && (0 != (tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_KILL]))))
		return K_ZTK_CONFLICT;
	if (!set_compare && kill_compare
		&& (0 != (tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_SET])) && (0 != (db_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_SET])))
	{	/* Subtract common (between triggerfile and DB) "non-S" from tf_cmd_bm */
		tmp_bm = gvtr_cmd_mask[GVTR_CMDTYPE_SET];
		COMMAND_BITMAP_TO_STR(values[CMD_SUB], tmp_bm, value_len[CMD_SUB]);
		/* since the KILL matches, update the corresponding trigger's KILLs */
		tmp_bm = db_cmd_bm | (tf_cmd_bm ^ (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & tf_cmd_bm));
		cmd_str_len = ARRAYSIZE(cmd_str);
		COMMAND_BITMAP_TO_STR(cmd_str, tmp_bm, cmd_str_len);
		i2mval(&mv_trig_indx, trigger_index);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[CMD_SUB],
						    STRLEN(trigger_subs[CMD_SUB]), cmd_str, cmd_str_len, result);
		assert(result == PUT_SUCCESS);
		return (result == PUT_SUCCESS) ? ADD_NEW_TRIGGER : result;
	}
	cmd_str_len = ARRAYSIZE(cmd_str);
	COMMAND_BITMAP_TO_STR(cmd_str, db_cmd_bm | tf_cmd_bm, cmd_str_len);
	i2mval(&mv_trig_indx, trigger_index);
	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[CMD_SUB], STRLEN(trigger_subs[CMD_SUB]),
					    cmd_str, cmd_str_len, result);
	if (PUT_SUCCESS != result)
		return result;
	strcpy(trig_cmds, cmd_str);
	if ((0 != (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & tf_cmd_bm)) && (0 == (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & db_cmd_bm)))
	{	/* need to add SET attributes */
		if (0 < value_len[DELIM_SUB])
		{
			SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[DELIM_SUB],
				STRLEN(trigger_subs[DELIM_SUB]), values[DELIM_SUB], value_len[DELIM_SUB], result);
			if (PUT_SUCCESS != result)
				return result;
		}
		if (0 < value_len[ZDELIM_SUB])
		{
			SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[ZDELIM_SUB],
				STRLEN(trigger_subs[ZDELIM_SUB]), values[ZDELIM_SUB], value_len[ZDELIM_SUB], result);
			if (PUT_SUCCESS != result)
				return result;
		}
		if (0 < value_len[PIECES_SUB])
		{
			SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[PIECES_SUB],
				STRLEN(trigger_subs[PIECES_SUB]), values[PIECES_SUB], value_len[PIECES_SUB], result);
			if (PUT_SUCCESS != result)
				return result;
		}
		if ((0 == (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & db_cmd_bm))
			&& (0 != (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & tf_cmd_bm)))
		{	/* We gained an "S" so we need to add the set hash value */
			result = add_trigger_hash_entry(trigvn, trigvn_len, values[CMD_SUB], trigger_index, FALSE, kill_hash,
					set_hash);
			if (PUT_SUCCESS != result)
				return result;
			MV_FORCE_UMVAL(&mv_hash, set_hash->hash_code);
			SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, mv_trig_indx, trigger_subs[BHASH_SUB],
							     STRLEN(trigger_subs[BHASH_SUB]), mv_hash, result);
			if (PUT_SUCCESS != result)
				return result;
		}
	}
	trigger_incr_cycle(trigvn, trigvn_len);
	return ADD_UPDATE_CMDS;
}

STATICFNDEF int4 add_trigger_options_attributes(char *trigvn, int trigvn_len, int trigger_index, char *trig_options, char **values,
						uint4 *value_len)
{
	uint4			db_option_bm;
	mval			mv_trig_indx;
	char			option_str[MAX_OPTIONS_LEN];
	int			option_str_len;
	int4			result;
	uint4			tf_option_bm;
	uint4			tmp_bm;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	BUILD_OPTION_BITMAP(db_option_bm, trig_options);
	BUILD_OPTION_BITMAP(tf_option_bm, values[OPTIONS_SUB]);
	if (tf_option_bm == (db_option_bm & tf_option_bm))
		/* If trigger file OPTIONS is contained in the DB OPTIONS, then trigger file entry is already in DB, just return */
		return OPTIONS_PRESENT;
	tmp_bm = db_option_bm | tf_option_bm;
	if (((0 != (OPTIONS_C & tmp_bm)) && (0 != (OPTIONS_NOC & tmp_bm)))
			|| ((0 != (OPTIONS_I & tmp_bm)) && (0 != (OPTIONS_NOI & tmp_bm))))
		/* Can't combine incompatible options, so triggers are different */
		return OPTION_CONFLICT;
	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	option_str_len = ARRAYSIZE(option_str);
	OPTION_BITMAP_TO_STR(option_str, tmp_bm, option_str_len);
	i2mval(&mv_trig_indx, trigger_index);
	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[OPTIONS_SUB],
		STRLEN(trigger_subs[OPTIONS_SUB]), option_str, option_str_len, result);
	if (PUT_SUCCESS != result)
		return result;
	strcpy(trig_options, option_str);
	trigger_incr_cycle(trigvn, trigvn_len);
	return ADD_UPDATE_OPTIONS;
}

STATICFNDEF boolean_t subtract_trigger_cmd_attributes(char *trigvn, int trigvn_len, char *trig_cmds, char **values,
		uint4 *value_len, boolean_t set_cmp, stringkey *kill_hash, stringkey *set_hash)
{
	char			cmd_str[MAX_COMMANDS_LEN];
	int			cmd_str_len;
	uint4			db_cmd_bm;
	uint4			len;
	uint4			tf_cmd_bm;
	uint4			restore_set = 0;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	BUILD_COMMAND_BITMAP(db_cmd_bm, trig_cmds);
	BUILD_COMMAND_BITMAP(tf_cmd_bm, values[CMD_SUB]);
	if (!set_cmp && (0 != (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & tf_cmd_bm)))
	{ /* If the set compare failed, we don't want to consider the SET */
		restore_set = gvtr_cmd_mask[GVTR_CMDTYPE_SET];
		tf_cmd_bm &= ~restore_set;
	}
	if (0 == (db_cmd_bm & tf_cmd_bm))
		/* If trigger file CMD does NOT overlap with the DB CMD, then no match. So no delete.  Just return */
		return 0;
	cmd_str_len = ARRAYSIZE(cmd_str);
	if (db_cmd_bm != (db_cmd_bm & tf_cmd_bm))
	{	/* combine cmds - subtract trigger file attributes from db attributes */
		COMMAND_BITMAP_TO_STR(cmd_str, (db_cmd_bm & tf_cmd_bm) ^ db_cmd_bm, cmd_str_len);
		strcpy(trig_cmds, cmd_str);
		if ((0 != (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & db_cmd_bm))
				&& (0 == (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & ((db_cmd_bm & tf_cmd_bm) ^ db_cmd_bm))))
			/* We lost the "S" so we need to delete the set hash value */
			cleanup_trigger_hash(trigvn, trigvn_len, values, value_len, set_hash, kill_hash, FALSE, 0);
	} else
	{	/* Both cmds are the same - candidate for delete */
		trig_cmds[0] = '\0';
		db_cmd_bm |= restore_set;
		COMMAND_BITMAP_TO_STR(cmd_str, db_cmd_bm, cmd_str_len);
		value_len[CMD_SUB] = cmd_str_len;
		strcpy(values[CMD_SUB], cmd_str);

	}
	return SUB_UPDATE_CMDS;
}

STATICFNDEF boolean_t subtract_trigger_options_attributes(char *trigvn, int trigvn_len, char *trig_options, char *option_value)
{
	uint4			db_option_bm;
	char			option_str[MAX_OPTIONS_LEN];
	int			option_str_len;
	uint4			tf_option_bm;
	uint4			tmp_bm;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	BUILD_OPTION_BITMAP(db_option_bm, trig_options);
	BUILD_OPTION_BITMAP(tf_option_bm, option_value);
	if (((0 != db_option_bm) && (0 != tf_option_bm)) && (0 == (db_option_bm & tf_option_bm)))
		/* If trigger file OPTIONS does NOT overlap with the DB OPTIONS, then no match. So no delete.  Just return */
		return 0;
	if (db_option_bm != (db_option_bm & tf_option_bm))
	{
		/* combine options - subtract trigger file attributes from db attributes */
		tmp_bm = (db_option_bm & tf_option_bm) ^ db_option_bm;
		assert((0 == (OPTIONS_C & tmp_bm)) || (0 == (OPTIONS_NOC & tmp_bm)));
		assert((0 == (OPTIONS_I & tmp_bm)) || (0 == (OPTIONS_NOI & tmp_bm)));
		option_str_len = ARRAYSIZE(option_str);
		OPTION_BITMAP_TO_STR(option_str, tmp_bm, option_str_len);
		strcpy(trig_options, option_str);
	} else
		/* Both options are the same - candidate to delete */
		trig_options[0] = '\0';
	return SUB_UPDATE_OPTIONS;
}

STATICFNDEF int4 modify_record(char *trigvn, int trigvn_len, char add_delete, int trigger_index, char **values, uint4 *value_len,
		mval *trigger_count, boolean_t set_compare, boolean_t kill_compare, stringkey *kill_hash, stringkey *set_hash)
{
	char			db_cmds[MAX_COMMANDS_LEN + 1];
	boolean_t		name_matches;
	int4			result;
	uint4			retval;
	mval			trigindx;
	char			trig_cmds[MAX_COMMANDS_LEN + 1];
	char			trig_name[MAX_USER_TRIGNAME_LEN + 2];	/* One spot for # delimiter and one for trailing 0 */
	char			trig_options[MAX_OPTIONS_LEN + 1];
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];
	int4			util_len;
	mval			val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	retval = 0;
	i2mval(&trigindx, trigger_index);
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[CMD_SUB], STRLEN(trigger_subs[CMD_SUB]));
	if (!gvcst_get(&val)) /* There has to be a command string */
		HASHT_GVN_DEFINITION_RETRY_OR_ERROR(trigger_index, ",\"CMD\"", REG2CSA(gv_cur_region));
	memcpy(trig_cmds, val.str.addr, val.str.len);
	trig_cmds[val.str.len] = '\0';
	/* get(^#t(GVN,trigindx,"OPTIONS") */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[OPTIONS_SUB],
		STRLEN(trigger_subs[OPTIONS_SUB]));
	if (gvcst_get(&val))
		memcpy(trig_options, val.str.addr, val.str.len);
	else
		val.str.len = 0;
	trig_options[val.str.len] = '\0';
	/* get(^#t(GVN,trigindx,"TRIGNAME") */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
		STRLEN(trigger_subs[TRIGNAME_SUB]));
	if (gvcst_get(&val))
		memcpy(trig_name, val.str.addr, val.str.len);
	else
		val.str.len = 0;
	trig_name[val.str.len] = '\0';
	if ('+' == add_delete)
	{
		result = add_trigger_cmd_attributes(trigvn, trigvn_len, trigger_index, trig_cmds, values, value_len,
						    set_compare, kill_compare, kill_hash, set_hash);
		switch (result)
		{
			case K_ZTK_CONFLICT:
			case INVALID_LABEL:
			case ADD_NEW_TRIGGER:
			case VAL_TOO_LONG:
			case KEY_TOO_LONG:
				return result;
			default:
				retval = result;
		}
		result = update_trigger_name(trigvn, trigvn_len, trigger_index, trig_name, values[TRIGNAME_SUB],
					     value_len[TRIGNAME_SUB]);
		if ((INVALID_LABEL == result) || (VAL_TOO_LONG == result) || (KEY_TOO_LONG == result))
			return result;
		retval |= result;
		result = add_trigger_options_attributes(trigvn, trigvn_len, trigger_index, trig_options, values, value_len);
		if ((INVALID_LABEL == result) || (VAL_TOO_LONG == result) || (KEY_TOO_LONG == result))
			return result;
		retval |= result;
	} else
	{
		name_matches = (0 == value_len[TRIGNAME_SUB])
			|| ((value_len[TRIGNAME_SUB] == (STRLEN(trig_name) - 1))
				&& (0 == memcmp(values[TRIGNAME_SUB], trig_name, value_len[TRIGNAME_SUB])));
		if (name_matches)
		{
			retval = SUB_UPDATE_NAME;
			memcpy(db_cmds, trig_cmds, SIZEOF(trig_cmds));
			retval |= subtract_trigger_cmd_attributes(trigvn, trigvn_len, trig_cmds, values, value_len, set_compare,
					kill_hash, set_hash);
			retval |= subtract_trigger_options_attributes(trigvn, trigvn_len, trig_options, values[OPTIONS_SUB]);
		}
		if ((0 != (retval & SUB_UPDATE_NAME)) && (0 != (retval & SUB_UPDATE_OPTIONS)) && (0 != (retval & SUB_UPDATE_CMDS)))
		{
			if ((0 == trig_cmds[0]) && (0 == trig_options[0]))
			{
				result = trigger_delete(trigvn, trigvn_len, trigger_count, trigger_index);
				if ((VAL_TOO_LONG == result) || (KEY_TOO_LONG == result))
					return result;
				retval = DELETE_REC;
			} else
			{
				retval = 0;
				if (0 != trig_cmds[0])
				{
					result = update_commands(trigvn, trigvn_len, trigger_index, trig_cmds, db_cmds);
					if (SUB_UPDATE_CMDS != result)
						return result;
					retval |= result;
				}
				if (0 != trig_options[0])
				{
					result = update_options(trigvn, trigvn_len, trigger_index, trig_options,
								values[OPTIONS_SUB]);
					if (SUB_UPDATE_OPTIONS != result)
						return result;
					if (0 != value_len[OPTIONS_SUB])
						retval |= result;
				}
			}
		}
		else
			retval = SUB_UPDATE_NOCHANGE;
	}
	return retval;
}

STATICFNDEF int4 gen_trigname_sequence(char *trigvn, int trigvn_len, mval *trigger_count, char *user_trigname_str,
				       uint4 user_trigname_len)
{
	sgmnt_addrs		*csa;
	char			name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	char			*ptr1;
	int			rndm_int;
	int			seq_num;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	int4			result;
	char			*seq_ptr, *uniq_ptr;
	char			trig_name[MAX_USER_TRIGNAME_LEN + 1];
	uint4			trigname_len;
	char			unique_seq_str[NUM_TRIGNAME_SEQ_CHARS + 1];
	mval			val, *val_ptr;
	char			val_str[MAX_DIGITS_IN_INT + 1];
	int			var_count;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(MAX_USER_TRIGNAME_LEN >= user_trigname_len);
	uniq_ptr = unique_seq_str;
	seq_num = 1;
	if (0 == user_trigname_len)
	{	/* autogenerated name  -- might be long */
		trigname_len = MIN(trigvn_len, MAX_AUTO_TRIGNAME_LEN);
		strncpy(trig_name, trigvn, trigname_len);
	} else
	{	/* user supplied name */
		trigname_len = user_trigname_len;
		strncpy(trig_name, user_trigname_str, user_trigname_len);
	}
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 == user_trigname_len)
	{	/* autogenerated name */
		val_ptr = &val;
		if (0 != gv_target->root)
		{
			/* $get(^#t("#TNAME",GVN,"#SEQCOUNT")) */
			BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
				LITERAL_HASHSEQNUM, STRLEN(LITERAL_HASHSEQNUM));
			seq_num = gvcst_get(val_ptr) ? mval2i(val_ptr) + 1 : 1;
			if (MAX_TRIGNAME_SEQ_NUM < seq_num)
			{
				RESTORE_TRIGGER_REGION_INFO;
				return TOO_MANY_TRIGGERS;
			}
		}
		INT2STR(seq_num, uniq_ptr);
		/* set ^#t("#TNAME",GVN,"#SEQCOUNT")++ via unique_seq_str which came from seq_num*/
		SET_TRIGGER_GLOBAL_SUB_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHSEQNUM, STRLEN(LITERAL_HASHSEQNUM), unique_seq_str, STRLEN(unique_seq_str), result);
		if (PUT_SUCCESS != result)
		{
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
		/* set ^#t("#TNAME",GVN,"#TNCOUNT")++ */
		BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHTNCOUNT, STRLEN(LITERAL_HASHTNCOUNT));
		var_count = gvcst_get(val_ptr) ? mval2i(val_ptr) + 1 : 1;
		i2mval(&val, var_count);
		SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHTNCOUNT, STRLEN(LITERAL_HASHTNCOUNT), val, result);
		if (PUT_SUCCESS != result)
		{
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	} else
		*uniq_ptr = '\0';	/* user supplied name */
	seq_ptr = user_trigname_str;
	memcpy(seq_ptr, trig_name, trigname_len);
	seq_ptr += trigname_len;
	if (0 == user_trigname_len)
	{	/* Autogenerated */
		*seq_ptr++ = TRIGNAME_SEQ_DELIM;
		memcpy(seq_ptr, unique_seq_str, STRLEN(unique_seq_str));
		seq_ptr += STRLEN(unique_seq_str);
	}
	*seq_ptr = '\0';
	ptr1 = name_and_index;
	memcpy(ptr1, trigvn, trigvn_len);
	ptr1 += trigvn_len;
	*ptr1++ = '\0';
	MV_FORCE_STR(trigger_count);
	memcpy(ptr1, trigger_count->str.addr, trigger_count->str.len);
	SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), user_trigname_str, STRLEN(user_trigname_str),
		name_and_index, trigvn_len + 1 + trigger_count->str.len, result);
	if (PUT_SUCCESS != result)
	{
		RESTORE_TRIGGER_REGION_INFO;
		return result;
	}
	*seq_ptr++ = TRIGNAME_SEQ_DELIM;
	*seq_ptr = '\0';
	RESTORE_TRIGGER_REGION_INFO;
	return SEQ_SUCCESS;
}

boolean_t trigger_update_rec(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats, io_pair *trigfile_device,
		int4 *record_num)
{
	char			add_delete;
	char			ans[2];
	sgmnt_addrs		*csa;
	boolean_t		cmd_modified;
	char			db_trig_name[MAX_USER_TRIGNAME_LEN + 1];
	boolean_t		full_match;
	mstr			gbl_name, var_name;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	boolean_t		kill_cmp;
	int4			max_len;
	boolean_t 		multi_line, multi_line_xecute;
	mval			mv_hash;
	boolean_t		new_name;
	int			num;
	int4			offset;
	char			*ptr1;
	int4			rec_len;
	int4			rec_num;
	boolean_t		result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gv_key			*save_gv_altkey;
	gv_namehead		*save_gvtarget;
	boolean_t		set_cmp;
	boolean_t		skip_set_trigger;
	boolean_t		status;
	int			sub_indx;
	char			tcount[MAX_DIGITS_IN_INT];
	char			tfile_rec_val[MAX_BUFF_SIZE];
	char			trig_cmds[MAX_COMMANDS_LEN + 1];
	mval			*trig_cnt_ptr;
	char			trig_name[MAX_USER_TRIGNAME_LEN + 2];	/* One spot for '#' delimiter and one for trailing '\0' */
	char			trig_options[MAX_OPTIONS_LEN + 1];
	char			trigvn[MAX_MIDENT_LEN + 1];
	mval			trigger_count;
	int			trigvn_len;
	int			trigindx, kill_index = -1;
	int4			updates = 0;
	char			*values[NUM_SUBS];
	uint4			value_len[NUM_SUBS];
	stringkey		kill_trigger_hash, set_trigger_hash;
	char			tmp_str[MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT];
	char			xecute_buffer[MAX_XECUTE_LEN];
	mval			xecute_index, xecute_size;
	mval			reportname, reportnamealt;
	io_pair			io_save_device;
	int4			max_xecute_size;
	boolean_t		no_error;
	boolean_t		newtrigger;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 > memcmp(LITERAL_HASHLABEL, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHLABEL), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHCOUNT, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHCOUNT), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHCYCLE, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHCYCLE), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHTNAME, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHTNAME), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHTNCOUNT, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHTNCOUNT), STRLEN(LITERAL_MAXHASHVAL))));
	rec_num = (NULL == record_num) ? 0 : *record_num;
	gvinit();
	if ((0 == len) || (COMMENT_LITERAL == *trigger_rec))
		return TRIG_SUCCESS;
	if (('-' != *trigger_rec) && ('+' != *trigger_rec))
	{
		trig_stats[STATS_ERROR]++;
		util_out_print_gtmio("missing +/- at start of line: !AD", FLUSH, len, trigger_rec);
		return TRIG_FAILURE;
	}
	add_delete = *trigger_rec++;
	len--;
	if ('-' == add_delete)
	{
		if ((1 == len) && ('*' == *trigger_rec))
		{
			if (!noprompt)
			{
				util_out_print("This operation will delete all triggers.", FLUSH);
				util_out_print("Proceed? [y/n]: ", FLUSH);
				SCANF("%1s", ans);	/* We only need one char, any more would overflow our buffer */
				if ('y' != ans[0] && 'Y' != ans[0])
				{
					util_out_print_gtmio("Triggers NOT deleted", FLUSH);
					return TRIG_SUCCESS;
				}
			}
			trigger_delete_all();
			return TRIG_SUCCESS;
		} else if ((0 == len) || ('^' != *trigger_rec))
		{	/* if the length < 0 let trigger_delete_name respond with the error message */
			if (TRIG_FAILURE == (status = trigger_delete_name(trigger_rec, len, trig_stats)))
				trig_stats[STATS_ERROR]++;
			return status;
		}
	}
	values[GVSUBS_SUB] = tfile_rec_val;	/* GVSUBS will be the first entry set so initialize it */
	max_len = (int4)SIZEOF(tfile_rec_val);
	multi_line_xecute = FALSE;
	no_error = TRUE;
	if (!trigger_parse(trigger_rec, len, trigvn, values, value_len, &max_len, &multi_line_xecute))
	{
		if (multi_line_xecute)
			no_error = FALSE;
		else
		{
			trig_stats[STATS_ERROR]++;
			return TRIG_FAILURE;
		}
	}
	trigvn_len = STRLEN(trigvn);
	set_trigger_hash.str.addr = tmp_str;
	set_trigger_hash.str.len = SIZEOF(tmp_str);
	build_set_cmp_str(trigvn, trigvn_len, values, value_len, &set_trigger_hash.str, multi_line_xecute);
	COMPUTE_HASH_STR(&set_trigger_hash);
	kill_trigger_hash.str.addr = tmp_str;
	kill_trigger_hash.str.len = SIZEOF(tmp_str);
	build_kill_cmp_str(trigvn, trigvn_len, values, value_len, &kill_trigger_hash.str, multi_line_xecute);
	COMPUTE_HASH_STR(&kill_trigger_hash);
	if (multi_line_xecute)
	{
		if (NULL == trigfile_device)
		{
			util_out_print_gtmio("Cannot use multi-line xecute in $ztrigger ITEM", FLUSH);
			return TRIG_FAILURE;
		}
		io_save_device = io_curr_device;
		io_curr_device = *trigfile_device;
		values[XECUTE_SUB] = xecute_buffer;
		value_len[XECUTE_SUB] = 0;
		max_xecute_size = SIZEOF(xecute_buffer);
		multi_line = multi_line_xecute;
		while (multi_line && (0 <= (rec_len = file_input_get(&trigger_rec))))
		{
			rec_num++;
			io_curr_device = io_save_device;	/* In case we have to write an error message */
			no_error &= trigger_parse(trigger_rec, (uint4)rec_len, trigvn, values, value_len, &max_xecute_size,
				&multi_line);
			io_curr_device = *trigfile_device;
		}
		if (NULL != record_num)
			*record_num = rec_num;
		if (!no_error)
		{
			io_curr_device = io_save_device;
			trig_stats[STATS_ERROR]++;
			return TRIG_FAILURE;
		}
		if (0 > rec_len)
		{
			io_curr_device = io_save_device;
			util_out_print_gtmio("Multi-line trigger -XECUTE is not properly terminated", FLUSH);
			trig_stats[STATS_ERROR]++;
			return TRIG_FAILURE;
		}
		STR_HASH(values[XECUTE_SUB], value_len[XECUTE_SUB], set_trigger_hash.hash_code, set_trigger_hash.hash_code);
		STR_HASH(values[XECUTE_SUB], value_len[XECUTE_SUB], kill_trigger_hash.hash_code, kill_trigger_hash.hash_code);
		io_curr_device = io_save_device;
	}
	gbl_name.addr = trigvn;
	gbl_name.len = trigvn_len;
	GV_BIND_NAME_ONLY(gd_header, &gbl_name);
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(gv_target->gd_csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	csa = gv_target->gd_csa;
	/* Now that the gv_target of the global the trigger refers to is setup, check if we are attempting to modify/delete a
	 * trigger for a global that has already had a trigger fire in this transaction. For these single-region (at a time)
	 * checks, we can do them all the time as they are cheap.
	 */
	if (gv_target->trig_local_tn == local_tn)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_TRIGMODINTP);
	csa->incr_db_trigger_cycle = TRUE; /* so that we increment csd->db_trigger_cycle at commit time */
	if (dollar_ztrigger_invoked)
	{	/* increment db_dztrigger_cycle so that next gvcst_put/gvcst_kill in this transaction, on this region, will re-read
		 * triggers. Note that the below increment happens for every record added. So, even if a single trigger file loaded
		 * multiple triggers on the same region, db_dztrigger_cycle will be incremented more than one for same transaction.
		 * This is considered okay since we only need db_dztrigger_cycle to be equal to a different value than
		 * gvt->db_dztrigger_cycle
		 */
		csa->db_dztrigger_cycle++;
	}
	SETUP_TRIGGER_GLOBAL;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	new_name = check_unique_trigger_name(trigvn, trigvn_len, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB]);
	cmd_modified = skip_set_trigger = newtrigger = FALSE;
	trigindx = 1;
	assert(('+' == add_delete) || ('-' == add_delete));	/* Has to be + or - */
	if (0 != gv_target->root)
	{
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
		if (gvcst_get(&trigger_count))
		{
			if (trigger_already_exists(trigvn, trigvn_len, values, value_len, &trigindx, &kill_index, &set_cmp,
						   &kill_cmp, &full_match, &set_trigger_hash, &kill_trigger_hash,
						   &reportname, &reportnamealt))
			{
				if (!new_name && ('+' == add_delete) && (!full_match))
				{
					TRIGGER_SAME_NAME_EXISTS_ERROR;
				}
				if (-1 != kill_index)
				{
					if (0 != value_len[TRIGNAME_SUB])
					{ /* can't match two different trigger with a user defined name */
						trig_stats[STATS_ERROR]++;
						util_out_print_gtmio("Conflicting trigger definition for global ^!AD. Definition " \
						       "matches trigger named !AD and attempts to create a new trigger named !AD",
						       FLUSH, trigvn_len, trigvn, reportnamealt.str.len, reportnamealt.str.addr,
						       value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);
						return TRIG_FAILURE;
					}
					updates = modify_record(trigvn, trigvn_len, add_delete, kill_index, values, value_len,
						&trigger_count, FALSE, TRUE, &kill_trigger_hash, &set_trigger_hash);
					switch (updates)
					{
						case INVALID_LABEL:
							trig_stats[STATS_ERROR]++;
							util_out_print_gtmio("Current trigger format not compatible to update " \
							       "the trigger on ^!AD named !AD", FLUSH, trigvn_len, trigvn,
							       reportnamealt.str.len, reportnamealt.str.addr);
							return TRIG_FAILURE;
						case VAL_TOO_LONG:
							trig_stats[STATS_ERROR]++;
							util_out_print_gtmio("^!AD trigger - value larger than record size", FLUSH,
								trigvn_len, trigvn);
							return TRIG_FAILURE;
						case K_ZTK_CONFLICT:
							trig_stats[STATS_ERROR]++;
							util_out_print_gtmio("Command options !AD incompatible with trigger on " \
								"^!AD named !AD", FLUSH, value_len[CMD_SUB], values[CMD_SUB],
								trigvn_len, trigvn, reportnamealt.str.len, reportnamealt.str.addr);
							return TRIG_FAILURE;
						default:
							if ((0 != (updates & ADD_UPDATE_CMDS)) ||
							    (0 != (updates & SUB_UPDATE_CMDS)))
							{
								if (0 == trig_stats[STATS_ERROR])
									util_out_print_gtmio("Updated trigger on ^!AD named " \
											     "!AD and ", NOFLUSH, trigvn_len,
											     trigvn, reportnamealt.str.len,
											     reportnamealt.str.addr);
								trig_stats[STATS_MODIFIED]++;
							} else if ((0 != (updates & ADD_UPDATE_NAME)) ||
								   (0 != (updates & SUB_UPDATE_NAME)) ||
								   (0 != (updates & SUB_UPDATE_OPTIONS)) ||
								   (0 != (updates & ADD_UPDATE_OPTIONS)))
							{ /* NAME and OPTIONS cannot change on K-type match */
								assertpro(FALSE);
							} else if (0 != (updates & DELETE_REC))
							{
								if (0 == trig_stats[STATS_ERROR])
									util_out_print_gtmio("Deleted trigger on ^!AD named "\
											     "!AD and ", NOFLUSH, trigvn_len,
											     trigvn, reportnamealt.str.len,
											     reportnamealt.str.addr);
								trig_stats[STATS_DELETED]++;
								/* if trigger deleted, search for possible new SET trigger index */
								if(kill_index < trigindx &&
								   !(trigger_already_exists(trigvn, trigvn_len, values, value_len,
											    &trigindx, &kill_index,
											    &set_cmp, &kill_cmp, &full_match,
											    &set_trigger_hash, &kill_trigger_hash,
											    &reportname, &reportnamealt)))
								{ /* SET trigger found previously is not found again */
									if (CDB_STAGNATE > t_tries)
										t_retry(cdb_sc_triggermod);
									else
									{
										assert(WBTEST_HELPOUT_TRIGDEFBAD == \
												gtm_white_box_test_case_number);
										trig_stats[STATS_ERROR]++;
										util_out_print_gtmio("Previously found trigger on" \
												     "^!AD ,named !AD but cannot " \
												     "find it again",
												     FLUSH, trigvn_len, trigvn,
												     reportnamealt.str.len,
												     reportnamealt.str.addr);
									}
									return TRIG_FAILURE;
								}
							} else
							{
								util_out_print_gtmio("Trigger on ^!AD already present " \
										     "-- same as trigger named !AD and ",
										     NOFLUSH, trigvn_len, trigvn,
										     reportname.str.len, reportname.str.addr);
								trig_stats[STATS_UNCHANGED]++;
							}
					}
				}
				updates = modify_record(trigvn, trigvn_len, add_delete, trigindx, values, value_len,
						&trigger_count, set_cmp, kill_cmp, &kill_trigger_hash, &set_trigger_hash);
				switch (updates)
				{
					case ADD_NEW_TRIGGER:
						if (0 != value_len[TRIGNAME_SUB])
						{ /* can't add a new trigger when you already matched on a name */
							trig_stats[STATS_ERROR]++;
							util_out_print_gtmio("Conflicting trigger definition for global ^!AD." \
									     " Definition matches trigger named !AD and attempts" \
									     " to create a new trigger named !AD", FLUSH,
									     trigvn_len, trigvn,
									     reportname.str.len, reportname.str.addr,
									     value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);
							return TRIG_FAILURE;
						}
						cmd_modified = TRUE;
						trig_cnt_ptr = &trigger_count;
						num = mval2i(trig_cnt_ptr);
						trigindx = ++num;
						i2mval(&trigger_count, num);
						break;
					case INVALID_LABEL:
						trig_stats[STATS_ERROR]++;
						util_out_print_gtmio("Current trigger format not compatible to update " \
						       "the trigger on ^!AD named !AD", FLUSH, trigvn_len, trigvn,
						       reportname.str.len, reportname.str.addr);
						return TRIG_FAILURE;
					case VAL_TOO_LONG:
						trig_stats[STATS_ERROR]++;
						util_out_print_gtmio("^!AD trigger - value larger than record size", FLUSH,
							trigvn_len, trigvn);
						return TRIG_FAILURE;
					case K_ZTK_CONFLICT:
						trig_stats[STATS_ERROR]++;
						util_out_print_gtmio("Command options !AD incompatible with trigger on " \
							"^!AD named !AD", FLUSH, value_len[CMD_SUB], values[CMD_SUB],
							trigvn_len, trigvn, reportname.str.len, reportname.str.addr);
						return TRIG_FAILURE;
					default:
						skip_set_trigger = TRUE;
						if ((0 != (updates & ADD_UPDATE_NAME)) || (0 != (updates & ADD_UPDATE_CMDS))
							|| (0 != (updates & ADD_UPDATE_OPTIONS)))
						{
							i2mval(&trigger_count, trigindx);
							trig_stats[STATS_MODIFIED]++;
							if (0 == trig_stats[STATS_ERROR])
								util_out_print_gtmio("Updated trigger on ^!AD named !AD", FLUSH,
										     trigvn_len, trigvn, reportname.str.len,
										     reportname.str.addr);
						} else if (0 != (updates & DELETE_REC))
						{
							trig_stats[STATS_DELETED]++;
							if (0 == trig_stats[STATS_ERROR])
								util_out_print_gtmio("Deleted trigger on ^!AD named !AD",
										     FLUSH, trigvn_len, trigvn,
										     reportname.str.len, reportname.str.addr);
						} else if ((0 != (updates & SUB_UPDATE_NAME)) || (0 != (updates & SUB_UPDATE_CMDS))
							   || (0 != (updates & SUB_UPDATE_OPTIONS)))
						{
							trig_stats[STATS_MODIFIED]++;
							if (0 == trig_stats[STATS_ERROR])
								util_out_print_gtmio("Updated trigger on ^!AD named !AD", FLUSH,
										     trigvn_len, trigvn, reportname.str.len,
										     reportname.str.addr);
						} else if ('+' == add_delete)
						{
							if (0 == trig_stats[STATS_ERROR])
							{
								util_out_print_gtmio("Trigger on ^!AD already present -- same" \
									" as trigger named !AD", FLUSH, trigvn_len, trigvn,
									reportname.str.len, reportname.str.addr);
								trig_stats[STATS_UNCHANGED]++;
							}
						} else
						{
							if (0 == trig_stats[STATS_ERROR])
							{
								util_out_print_gtmio("Trigger on ^!AD does not exist - " \
									"no action taken", FLUSH, trigvn_len, trigvn);
								trig_stats[STATS_UNCHANGED]++;
							}
						}
				}
			} else if ('+' == add_delete)
			{
				assert(0 == trigindx);
				if (!new_name)
				{
					TRIGGER_SAME_NAME_EXISTS_ERROR;
				}
				trig_cnt_ptr = &trigger_count;
				num = mval2i(trig_cnt_ptr);
				trigindx = ++num;
				i2mval(&trigger_count, num);
			} else
			{ /* '-' == add_delete */
				if (0 == trig_stats[STATS_ERROR])
				{
					trig_stats[STATS_UNCHANGED]++;
					util_out_print_gtmio("Trigger on ^!AD does not exist - no action taken", FLUSH,
							     trigvn_len, trigvn);
				}
				skip_set_trigger = TRUE;
			}
		} else
			newtrigger = TRUE;
	} else
		newtrigger = TRUE;
	if (newtrigger)
	{
		if ('-' == add_delete)
		{
			if (0 == trig_stats[STATS_ERROR])
				util_out_print_gtmio("Trigger on ^!AD does not exist - no action taken",
						     FLUSH, trigvn_len, trigvn);
			else
				trig_stats[STATS_DELETED]++;
			skip_set_trigger = TRUE;
		} else
		{
			if (!new_name)
			{
				TRIGGER_SAME_NAME_EXISTS_ERROR;
			}
			trigger_count = literal_one;
		}
	}
	/* Since a specified trigger name will grow by 1, copy it to a long enough array */
	if (((0 != (updates & ADD_UPDATE_NAME)) && ('-' != add_delete)) || !skip_set_trigger)
	{
		memcpy(trig_name, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB] + 1);
		values[TRIGNAME_SUB] = trig_name;
		result = gen_trigname_sequence(trigvn, trigvn_len, &trigger_count, values[TRIGNAME_SUB],
					       value_len[TRIGNAME_SUB]);
		if (SEQ_SUCCESS != result)
		{
			if (TOO_MANY_TRIGGERS == result)
				util_out_print_gtmio("^!AD trigger - Too many triggers", FLUSH, trigvn_len, trigvn);
			else
			{
				TOO_LONG_REC_KEY_ERROR_MSG;
			}
			trig_stats[STATS_ERROR]++;
			return TRIG_FAILURE;
		}
	}
	if (!skip_set_trigger && (0 == trig_stats[STATS_ERROR]))
	{
		value_len[TRIGNAME_SUB] = STRLEN(values[TRIGNAME_SUB]);
		values[CHSET_SUB] = (gtm_utf8_mode) ? UTF8_NAME : LITERAL_M;
		value_len[CHSET_SUB] = STRLEN(values[CHSET_SUB]);
		/* set ^#t(GVN,"#LABEL") = "2" */
		SET_TRIGGER_GLOBAL_SUB_SUB_STR(trigvn, trigvn_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL),
					       HASHT_GBL_CURLABEL, STRLEN(HASHT_GBL_CURLABEL), result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		trigger_incr_cycle(trigvn, trigvn_len);
		/* set ^#t(GVN,"#COUNT") = trigger_count */
		SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT),
						trigger_count, result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
		{
			if (0 >= value_len[sub_indx])	/* subscript index length is zero (no longer used), skip it */
				continue;
			/* set ^#t(GVN,trigger_count,values[sub_indx]) = xecute string */
			SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, trigger_count,
				trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), values[sub_indx],
				value_len[sub_indx], result);
			if (XECUTE_SUB != sub_indx)
			{
				IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
			} else
			{	/* XECUTE_SUB == sub_indx */
				max_len = value_len[XECUTE_SUB];
				assert(0 < max_len);
				if (PUT_SUCCESS != result)
				{	/* xecute string does not fit in one record, break it up */
					i2mval(&xecute_size, max_len);
					num = 0;
					ptr1 = values[XECUTE_SUB];
					i2mval(&xecute_index, num);
					/* set ^#t(GVN,trigger_count,"XECUTE",0) = xecute string length */
					BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(trigvn, trigvn_len, trigger_count,
						trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index);
					SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_MVAL(trigvn, trigvn_len, trigger_count,
						trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index,
						xecute_size, result);
					IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
					while (0 < max_len)
					{
						i2mval(&xecute_index, ++num);
						BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(trigvn, trigvn_len, trigger_count,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index);
						offset = MIN(gv_cur_region->max_rec_size - (gv_currkey->end + 1 + SIZEOF(rec_hdr)),
							     max_len);
						/* set ^#t(GVN,trigger_count,"XECUTE",num) = xecute string[offset] */
						SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_STR(trigvn, trigvn_len, trigger_count,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index,
							ptr1, offset, result);
						IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
						ptr1 += offset;
						max_len -= offset;
					}
				}
			}
		}
		result = add_trigger_hash_entry(trigvn, trigvn_len, values[CMD_SUB], trigindx, TRUE, &kill_trigger_hash,
				&set_trigger_hash);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		MV_FORCE_UMVAL(&mv_hash, kill_trigger_hash.hash_code);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, trigger_count,
			trigger_subs[LHASH_SUB], STRLEN(trigger_subs[LHASH_SUB]), mv_hash, result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		MV_FORCE_UMVAL(&mv_hash, set_trigger_hash.hash_code);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, trigger_count, trigger_subs[BHASH_SUB],
			STRLEN(trigger_subs[BHASH_SUB]), mv_hash, result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		trig_stats[STATS_ADDED]++;
		if (cmd_modified)
			util_out_print_gtmio("Modified commands of the trigger on ^!AD named !AD", FLUSH, trigvn_len, trigvn,
					     value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);
		else
			util_out_print_gtmio("Added trigger on ^!AD named !AD", FLUSH, trigvn_len, trigvn,
					     value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);
	} else if (0 != trig_stats[STATS_ERROR])
	{
		if ('+' == add_delete)
			trig_stats[STATS_ADDED]++;
		util_out_print_gtmio("No errors", FLUSH);
	}
	return TRIG_SUCCESS;
}

STATICFNDEF boolean_t trigger_update_rec_helper(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats)
{
	enum cdb_sc		cdb_status;
	boolean_t		trigger_status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_RET(trigger_tpwrap_ch, TRIG_FAILURE);
	trigger_status = trigger_update_rec(trigger_rec, len, TRUE, trig_stats, NULL, NULL);
	if (TRIG_SUCCESS == trigger_status)
	{
		GVTR_OP_TCOMMIT(cdb_status);
		if (cdb_sc_normal != cdb_status)
			t_retry(cdb_status);	/* won't return */
	} else
	{	/* Record cannot be committed - undo everything */
		assert(donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);
		OP_TROLLBACK(-1);		/* returns but kills implicit transaction */
	}
	REVERT;
	return trigger_status;
}

boolean_t trigger_update(char *trigger_rec, uint4 len)
{
	uint4			i;
	uint4			trig_stats[NUM_STATS];
	boolean_t		trigger_status = TRIG_FAILURE;
	mval			ts_mv;
	int			loopcnt;
	DEBUG_ONLY(unsigned int	lcl_t_tries;)
	enum cdb_sc		failure;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	for (i = 0; NUM_STATS > i; i++)
		trig_stats[i] = 0;
	ts_mv.mvtype = MV_STR;
	ts_mv.str.len = 0;
	ts_mv.str.addr = NULL;
	if (0 == dollar_tlevel)
	{
		assert(!donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
		/* Note down dollar_tlevel before op_tstart. This is needed to determine if we need to break from the for-loop
		 * below after a successful op_tcommit of the $ZTRIGGER operation. We cannot check that dollar_tlevel is zero
		 * since the op_tstart done below can be a nested sub-transaction
		 */
		op_tstart((IMPLICIT_TSTART + IMPLICIT_TRIGGER_TSTART), TRUE, &ts_mv, 0); /* 0 ==> save no locals but RESTART OK */
		/* The following for loop structure is similar to that in module trigger_trgfile.c (function
		 * "trigger_trgfile_tpwrap") and module gv_trigger.c (function gvtr_db_tpwrap) so any changes here
		 * might need to be reflected there as well.
		 */
		for (loopcnt = 0; ; loopcnt++)
		{
			assert(donot_INVOKE_MUMTSTART);	/* Make sure still set */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			trigger_status = trigger_update_rec_helper(trigger_rec, len, TRUE, trig_stats);
			if (0 == dollar_tlevel)
				break;
			assert(0 < t_tries);
			assert((CDB_STAGNATE == t_tries) || (lcl_t_tries == t_tries - 1));
			failure = LAST_RESTART_CODE;
			assert(((cdb_sc_onln_rlbk1 != failure) && (cdb_sc_onln_rlbk2 != failure))
				|| !gv_target || !gv_target->root);
			assert((cdb_sc_onln_rlbk2 != failure) || !IS_GTM_IMAGE || TREF(dollar_zonlnrlbk));
			if (cdb_sc_onln_rlbk2 == failure)
				rts_error_csa(CSA_ARG(gv_target->gd_csa) VARLSTCNT(1) ERR_DBROLLEDBACK);
			/* else if (cdb_sc_onln_rlbk1 == status) we don't need to do anything other than trying again. Since this
			 * is ^#t global, we don't need to GVCST_ROOT_SEARCH before continuing with the next restart because the
			 * trigger load logic already takes care of doing INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED before doing the
			 * actual trigger load
			 */
			util_out_print_gtmio("RESTART has invalidated this transaction's previous output.  New output follows.",
					     FLUSH);
			/* We expect the above function to return with either op_tcommit or a tp_restart invoked.
			 * In the case of op_tcommit, we expect dollar_tlevel to be 0 and if so we break out of the loop.
			 * In the tp_restart case, we expect a maximum of 4 tries/retries and much lesser usually.
			 * Additionally we also want to avoid an infinite loop so limit the loop to what is considered
			 * a huge iteration count and GTMASSERT if that is reached as it suggests an out-of-design situation.
			 */
			if (TPWRAP_HELPER_MAX_ATTEMPTS < loopcnt)
				GTMASSERT;
		}
	} else
	{
		trigger_status = trigger_update_rec(trigger_rec, len, TRUE, trig_stats, NULL, NULL);
		assert(0 < dollar_tlevel);
	}
	return (TRIG_FAILURE == trigger_status);
}
#endif /* GTM_TRIGGER */
