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
#include "rtnhdr.h"
#include "gv_trigger.h"
#include "trigger_delete_protos.h"
#include "trigger.h"
#include "trigger_incr_cycle.h"
#include "trigger_parse_protos.h"
#include "trigger_update_protos.h"
#include "trigger_compare_protos.h"
#include "trigger_user_name.h"
#include "gtm_string.h"
#include "hashtab_str.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "hashtab.h"			/* for STR_HASH (in COMPUTE_HASH_MNAME)*/
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

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_addr			*gd_header;
GBLREF	boolean_t		implicit_tstart;	/* see gbldefs.c for comment */
GBLREF	boolean_t		incr_db_trigger_cycle;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	int			tprestart_state;
GBLREF	gv_namehead		*reset_gv_target;

LITREF	mval			gvtr_cmd_mval[GVTR_CMDTYPES];
LITREF	int4			gvtr_cmd_mask[GVTR_CMDTYPES];
LITREF	mval			literal_hasht;
LITREF	mval			literal_one;

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
	char		*lcl_ptr;										\
														\
	memcpy(lcl_cmds, COMMANDS, STRLEN(COMMANDS) + 1);							\
	BITMAP = 0;												\
	lcl_ptr = strtok(lcl_cmds, ",");									\
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
						BITMAP |= gvtr_cmd_mask[GVTR_CMDTYPE_ZTKILL];			\
						break;								\
					default:								\
						assert(FALSE);	/* Parsing should have found invalid command */ \
						break;								\
				}										\
				break;										\
			default:										\
				assert(FALSE);	/* Parsing should have found invalid command */			\
				break;										\
		}												\
	} while (lcl_ptr = strtok(NULL, ","));									\
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
	char		*lcl_ptr;										\
														\
	memcpy(lcl_options, OPTIONS, STRLEN(OPTIONS) + 1);							\
	BITMAP = 0;												\
	lcl_ptr = strtok(lcl_options, ",");									\
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
							assert(FALSE);	/* Parsing should have found invalid command */ \
							break;							\
					}									\
					break;									\
				default:									\
					assert(FALSE);	/* Parsing should have found invalid command */		\
					break;									\
			}											\
		} while (lcl_ptr = strtok(NULL, ","));								\
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

#define TRIGGER_SAME_NAME_EXISTS_ERROR											\
{															\
	trig_stats[STATS_ERROR]++;												\
	util_out_print_gtmio("a trigger named !AD already exists", FLUSH, value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);	\
	return TRIG_FAILURE;													\
}

error_def(ERR_TPRETRY);

/* This code is modeled around "updproc_ch" in updproc.c */
CONDITION_HANDLER(trigger_item_tpwrap_ch)
{
	int	rc;

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		assert(TPRESTART_STATE_NORMAL == tprestart_state);
		tprestart_state = TPRESTART_STATE_NORMAL;
		assert(NULL != first_sgm_info);
		/* This only happens at the outer-most layer so state should be normal now */
		rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
		assert(0 == rc);
		assert(TPRESTART_STATE_NORMAL == tprestart_state);
		/* "reset_gv_target" might have been set to a non-default value if we are deep inside "gvcst_put"
		 * when the restart occurs. Reset it before unwinding the gvcst_put C stack frame. Normally gv_target would
		 * be set to what is in reset_gv_target (using the RESET_GV_TARGET macro) but that could lead to gv_target
		 * and gv_currkey going out of sync depending on where in gvcst_put we got the restart (e.g. if we got it
		 * in gvcst_root_search before gv_currkey was initialized but after gv_target was). Therefore we instead set
		 * "reset_gv_target" back to its default value leaving "gv_target" untouched. This is ok to do so since as
		 * part of the tp restart, gv_target and gv_currkey are anyways going to be reset to what they were at the
		 * beginning of the TSTART and therefore are guaranteed to be back in sync. Not resetting "reset_gv_target"
		 * would also cause an assert (on this being the invalid) in "gvtr_match_n_invoke" to fail in a restart case.
		 */
		reset_gv_target = INVALID_GV_TARGET;
		UNWIND(NULL, NULL);
	}
	NEXTCH;
}

STATICFNDEF boolean_t validate_label(char *trigvn, int trigvn_len)
{
	mval			trigger_label;

	BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
	if (!gvcst_get(&trigger_label))
	{
		assert(FALSE);
	}
	return ((trigger_label.str.len == STRLEN(HASHT_GBL_CURLABEL))
		&& (0 == memcmp(trigger_label.str.addr, HASHT_GBL_CURLABEL, trigger_label.str.len)));
}

STATICFNDEF int4 update_commands(char *trigvn, int trigvn_len, int trigger_index, char *new_trig_cmds, char *orig_db_cmds)
{
	mval			mv_trig_indx;
	uint4			orig_cmd_bm, new_cmd_bm;
	int4			result;

	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	BUILD_COMMAND_BITMAP(orig_cmd_bm, orig_db_cmds);
	BUILD_COMMAND_BITMAP(new_cmd_bm, new_trig_cmds);
	MV_FORCE_MVAL(&mv_trig_indx, trigger_index);
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

	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	MV_FORCE_MVAL(&mv_trig_indx, trigger_index);
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

	retval = NO_NAME_CHANGE;
	if ((0 != tf_trig_name_len) && (tf_trig_name_len != STRLEN(db_trig_name) - 1)
		|| (0 != memcmp(tf_trig_name, db_trig_name, tf_trig_name_len)))
	{
		if (!validate_label(trigvn, trigvn_len))
			return INVALID_LABEL;
		MV_FORCE_MVAL(&mv_trig_indx, trigger_index);
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
	bool			status;
	mval			val;

	/* We only check user supplied names for uniqueness (since autogenerated names are unique). */
	if (0 == trigger_name_len)
		return TRUE;
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (0 == gv_target->root)
	{
		TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
		RESTORE_TRIGGER_REGION_INFO;
		return TRUE;
	}
	assert((MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT) > gv_cur_region->max_rec_size);
	BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trigger_name, trigger_name_len);
	status = !gvcst_get(&val);
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
	return status;
}

STATICFNDEF int4 add_trigger_hash_entry(char *trigvn, int trigvn_len, char **values, uint4 *value_len, int trigindx,
					boolean_t add_kill_hash, uint4 *kill_hash, uint4 *set_hash)
{
	sgmnt_addrs		*csa;
	int			hash_indx;
	char			indx_str[MAX_DIGITS_IN_INT + 1];
	uint4			len;
	mval			mv_hash;
	mval			mv_indx, *mv_indx_ptr;
	int			num_len;
	char			*ptr;
	int4			result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		set_cmp;
	char			tmp_str[MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT];
	stringkey		trigger_hash;

	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	set_cmp = (NULL != strchr(values[CMD_SUB], 'S'));
	mv_indx_ptr = &mv_indx;
	num_len = 0;
	I2A(indx_str, num_len, trigindx);
	ptr = indx_str + num_len;
	*ptr = '\0';
	if (set_cmp)
	{
		trigger_hash.str.addr = tmp_str;
		trigger_hash.str.len = ARRAYSIZE(tmp_str);
		build_set_cmp_str(trigvn, trigvn_len, values, value_len, &(trigger_hash.str));
		len = trigger_hash.str.len;
		COMPUTE_HASH_STR(&trigger_hash);
		MV_FORCE_UMVAL(&mv_hash, trigger_hash.hash_code);
		*set_hash = trigger_hash.hash_code;
		ptr = tmp_str + len;
		*ptr++ = '\0';
		len++;
		memcpy(ptr, indx_str, STRLEN(indx_str));
		ptr += STRLEN(indx_str);
		len = (int)(ptr - tmp_str);
		if (0 != gv_target->root)
		{
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
			op_zprevious(&mv_indx);
			hash_indx = (0 == mv_indx.str.len) ? 1 : (MV_FORCE_INT(mv_indx_ptr) + 1);
		} else
			hash_indx = 1;
		MV_FORCE_MVAL(&mv_indx, hash_indx);
		SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_indx, tmp_str,
			len, result);
		if (PUT_SUCCESS != result)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	} else
		*set_hash = 0;
	if (add_kill_hash)
	{
		trigger_hash.str.addr = tmp_str;
		trigger_hash.str.len = ARRAYSIZE(tmp_str);
		build_kill_cmp_str(trigvn, trigvn_len, values, value_len, &trigger_hash.str);
		len = trigger_hash.str.len;
		COMPUTE_HASH_STR(&trigger_hash);
		*kill_hash = trigger_hash.hash_code;
		MV_FORCE_UMVAL(&mv_hash, trigger_hash.hash_code);
		ptr = tmp_str + len;
		*ptr++ = '\0';
		len++;
		memcpy(ptr, indx_str, STRLEN(indx_str));
		ptr += STRLEN(indx_str);
		len = (int)(ptr - tmp_str);
		if (0 != gv_target->root)
		{
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
			op_zprevious(&mv_indx);
			hash_indx = (0 == mv_indx.str.len) ? 1 : (MV_FORCE_INT(mv_indx_ptr) + 1);
		} else
			hash_indx = 1;
		MV_FORCE_MVAL(&mv_indx, hash_indx);
		SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_indx, tmp_str,
			len, result);
		if (PUT_SUCCESS != result)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	} else
		*kill_hash = 0;
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
	return PUT_SUCCESS;
}

STATICFNDEF boolean_t trigger_already_exists(char *trigvn, int trigvn_len, char **values, uint4 *value_len, int *trig_indx,
		boolean_t *set_cmp_result, boolean_t *kill_cmp_result, boolean_t *full_match)
{
	sgmnt_addrs		*csa;
	boolean_t		db_has_K;
	boolean_t		db_has_S;
	char			*ptr;
	int			hash_indx;
	boolean_t		kill_cmp, kill_found;
	int			kill_indx;
	uint4			len;
	boolean_t		name_match;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		set_cmp, set_found, set_name_match, kill_name_match;
	int			set_indx;
	char			tmp_str[MAX_HASH_INDEX_LEN];
	stringkey		trigger_hash;
	mval			trigindx;
	mval			val;

	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	set_cmp = (NULL != strchr(values[CMD_SUB], 'S'));
	kill_cmp = (NULL != strchr(values[CMD_SUB], 'K'));
	set_found = kill_found = set_name_match = kill_name_match = FALSE;
	if (set_cmp)
	{
		trigger_hash.str.addr = tmp_str;
		trigger_hash.str.len = ARRAYSIZE(tmp_str);
		build_set_cmp_str(trigvn, trigvn_len, values, value_len, &trigger_hash.str);
		len = trigger_hash.str.len;
		COMPUTE_HASH_STR(&trigger_hash);
		set_found = search_triggers(tmp_str, len, trigger_hash.hash_code, &hash_indx, &set_indx, 0);
		/* if found, need to verify that there is an S command in DB, otherwise not really found */
		if (set_found)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			MV_FORCE_MVAL(&trigindx, set_indx);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
							 STRLEN(trigger_subs[TRIGNAME_SUB]));
			if (!gvcst_get(&val))
				assert(FALSE);	/* There has to be a name string */
			set_name_match = ((value_len[TRIGNAME_SUB] == (val.str.len - 1))
				&& (0 == memcmp(val.str.addr, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB])));
		}
	}
	*set_cmp_result = set_found;
	if (kill_cmp || !set_found)
	{
		trigger_hash.str.addr = tmp_str;
		trigger_hash.str.len = ARRAYSIZE(tmp_str);
		build_kill_cmp_str(trigvn, trigvn_len, values, value_len, &trigger_hash.str);
		len = trigger_hash.str.len;
		COMPUTE_HASH_STR(&trigger_hash);
		kill_found = search_triggers(tmp_str, len, trigger_hash.hash_code, &hash_indx, &kill_indx, 0);
		if (kill_found)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			MV_FORCE_MVAL(&trigindx, kill_indx);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[CMD_SUB],
							 STRLEN(trigger_subs[CMD_SUB]));
			if (gvcst_get(&val))
			{
				db_has_S = (NULL != strchr(val.str.addr, 'S'));
				db_has_K = (NULL != strchr(val.str.addr, 'K'));
				kill_found = !(db_has_S && set_cmp && !(db_has_S && db_has_K && set_cmp && kill_cmp));
			} else
				assert(FALSE);	/* There has to be a command string */
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
							 STRLEN(trigger_subs[TRIGNAME_SUB]));
			if (!gvcst_get(&val))
				assert(FALSE);	/* There has to be a name string */
			kill_name_match = ((value_len[TRIGNAME_SUB] == (val.str.len - 1))
				&& (0 == memcmp(val.str.addr, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB])));
		}
	}
	/* Starting from the beginning:
	 *    Matching both set and kill, but for different records -- don't update the kill record, hence the FALSE
	 *    Matching a set implies matching a kill -- hence the ||
	 */
	*kill_cmp_result = (set_found && kill_found && (set_indx != kill_indx)) ? FALSE : (kill_found || set_found);
	*trig_indx = (set_found) ? set_indx : (kill_found) ? kill_indx : 0;
	/* If there is both a set and a kill and the set components don't match, there is no name match no matter if the kill
	 * components match or not.  If there is no set, then the name match is only based on the kill components.
	 */
	*full_match = (set_cmp) ? set_name_match : kill_name_match;
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
	return (set_found || kill_found);
}

STATICFNDEF int4 add_trigger_cmd_attributes(char *trigvn, int trigvn_len,  int trigger_index, char *trig_cmds, char **values,
					    uint4 *value_len, boolean_t set_compare, boolean_t kill_compare)
{
	char			cmd_str[MAX_COMMANDS_LEN];
	int			cmd_str_len;
	uint4			db_cmd_bm;
	uint4			kill_hash;
	mval			mv_hash;
	mval			mv_trig_indx;
	int4			result;
	uint4			set_hash;
	uint4			tf_cmd_bm;
	uint4			tmp_bm;

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
	 * trigger_already_present() returns kill_compare as FALSE when the trigger file record matches both SET and KILL, but
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
		tmp_bm = ((db_cmd_bm & tf_cmd_bm) ^ tf_cmd_bm) | gvtr_cmd_mask[GVTR_CMDTYPE_SET];
		COMMAND_BITMAP_TO_STR(values[CMD_SUB], tmp_bm, value_len[CMD_SUB]);
		return ADD_NEW_TRIGGER;
	}
	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	cmd_str_len = ARRAYSIZE(cmd_str);
	COMMAND_BITMAP_TO_STR(cmd_str, db_cmd_bm | tf_cmd_bm, cmd_str_len);
	MV_FORCE_MVAL(&mv_trig_indx, trigger_index);
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
			result = add_trigger_hash_entry(trigvn, trigvn_len, values, value_len, trigger_index, FALSE, &kill_hash,
				&set_hash);
			if (PUT_SUCCESS != result)
				return result;
			MV_FORCE_UMVAL(&mv_hash, set_hash);
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
	MV_FORCE_MVAL(&mv_trig_indx, trigger_index);
	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[OPTIONS_SUB],
		STRLEN(trigger_subs[OPTIONS_SUB]), option_str, option_str_len, result);
	if (PUT_SUCCESS != result)
		return result;
	strcpy(trig_options, option_str);
	trigger_incr_cycle(trigvn, trigvn_len);
	return ADD_UPDATE_OPTIONS;
}

STATICFNDEF boolean_t subtract_trigger_cmd_attributes(char *trigvn, int trigvn_len, char *trig_cmds, char **values,
						      uint4 *value_len, boolean_t set_cmp)
{
	char			cmd_str[MAX_COMMANDS_LEN];
	int			cmd_str_len;
	uint4			db_cmd_bm;
	stringkey		kill_trigger_hash;
	uint4			len;
	stringkey		set_trigger_hash;
	uint4			tf_cmd_bm;
	char			tmp_str[MAX_HASH_INDEX_LEN];

	BUILD_COMMAND_BITMAP(db_cmd_bm, trig_cmds);
	BUILD_COMMAND_BITMAP(tf_cmd_bm, values[CMD_SUB]);
	if (!set_cmp)	/* If the set compare failed, we don't want to consider the SET */
		tf_cmd_bm &= ~gvtr_cmd_mask[GVTR_CMDTYPE_SET];
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
		{	/* We lost the "S" so we need to delete the set hash value */
			set_trigger_hash.str.addr = tmp_str;
			set_trigger_hash.str.len = ARRAYSIZE(tmp_str);
			build_set_cmp_str(trigvn, trigvn_len, values, value_len, &set_trigger_hash.str);
			len = set_trigger_hash.str.len;
			COMPUTE_HASH_STR(&set_trigger_hash);
			kill_trigger_hash.str.addr = tmp_str;
			kill_trigger_hash.str.len = ARRAYSIZE(tmp_str);
			build_kill_cmp_str(trigvn, trigvn_len, values, value_len, &kill_trigger_hash.str);
			len = kill_trigger_hash.str.len;
			COMPUTE_HASH_STR(&kill_trigger_hash);
			cleanup_trigger_hash(trigvn, trigvn_len, values, value_len, set_trigger_hash.hash_code,
					     kill_trigger_hash.hash_code, FALSE, 0);
		}
	} else
	{	/* Both cmds are the same - candidate for delete */
		trig_cmds[0] = '\0';
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

STATICFNDEF int4 modify_record(char *trigvn, int trigvn_len, char add_delete, int trigger_index, char **values,
			       uint4 *value_len, mval trigger_count, boolean_t set_compare, boolean_t kill_compare)
{
	char			db_cmds[MAX_COMMANDS_LEN + 1];
	boolean_t		name_matches;
	int4			result;
	uint4			retval;
	mval			trigindx;
	char			trig_cmds[MAX_COMMANDS_LEN + 1];
	char			trig_name[MAX_USER_TRIGNAME_LEN + 2];	/* One spot for # delimiter and one for trailing 0 */
	char			trig_options[MAX_OPTIONS_LEN + 1];
	mval			val;

	retval = 0;
	MV_FORCE_MVAL(&trigindx, trigger_index);
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[CMD_SUB], STRLEN(trigger_subs[CMD_SUB]));
	if (gvcst_get(&val))
		memcpy(trig_cmds, val.str.addr, val.str.len);
	else
		assert(FALSE);	/* There has to be a command string */
	trig_cmds[val.str.len] = '\0';
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[OPTIONS_SUB],
		STRLEN(trigger_subs[OPTIONS_SUB]));
	if (gvcst_get(&val))
		memcpy(trig_options, val.str.addr, val.str.len);
	else
		val.str.len = 0;
	trig_options[val.str.len] = '\0';
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
						    set_compare, kill_compare);
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
			retval |= subtract_trigger_cmd_attributes(trigvn, trigvn_len, trig_cmds, values, value_len, set_compare);
			retval |= subtract_trigger_options_attributes(trigvn, trigvn_len, trig_options, values[OPTIONS_SUB]);
		}
		if ((0 != (retval & SUB_UPDATE_NAME)) && (0 != (retval & SUB_UPDATE_OPTIONS)) && (0 != (retval & SUB_UPDATE_CMDS)))
		{
			if ((0 == trig_cmds[0]) && (0 == trig_options[0]))
			{
				result = trigger_delete(trigvn, trigvn_len, &trigger_count, trigger_index);
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

STATICFNDEF int4 gen_trigname_sequence(char *trigvn, int trigvn_len, mval *trigger_count, char *trigname_seq_str,
				       uint4 seq_len)
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

	assert(MAX_USER_TRIGNAME_LEN >= seq_len);
	uniq_ptr = unique_seq_str;
	if (0 == seq_len)
	{	/* autogenerated name  -- might be long */
		trigname_len = MIN(trigvn_len, MAX_AUTO_TRIGNAME_LEN);
		strncpy(trig_name, trigvn, trigname_len);
	} else
	{	/* user supplied name */
		trigname_len = seq_len;
		strncpy(trig_name, trigname_seq_str, seq_len);
	}
	SAVE_TRIGGER_REGION_INFO;
	SWITCH_TO_DEFAULT_REGION;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if ((seq_len == 0) && (trigvn_len > trigname_len))
	{	/* autogenerated long name */
		if (0 != gv_target->root)
		{
			val_ptr = &val;
			BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
				LITERAL_HASHSEQNUM, STRLEN(LITERAL_HASHSEQNUM));
			if (gvcst_get(&val))
			{
				seq_num = MV_FORCE_INT(val_ptr);
				if (MAX_TRIGNAME_SEQ_NUM < ++seq_num)
				{
					TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
					RESTORE_TRIGGER_REGION_INFO;
					return TOO_MANY_TRIGGERS;
				}
				INT2STR(seq_num, uniq_ptr);
			} else
			{
				*uniq_ptr++ = '1';
				*uniq_ptr = '\0';
			}
		} else
		{
			*uniq_ptr++ = '1';
			*uniq_ptr = '\0';
		}
		SET_TRIGGER_GLOBAL_SUB_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHSEQNUM, STRLEN(LITERAL_HASHSEQNUM), unique_seq_str, STRLEN(unique_seq_str), result);
		if (PUT_SUCCESS != result)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
		BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHTNCOUNT, STRLEN(LITERAL_HASHTNCOUNT));
		var_count = gvcst_get(val_ptr) ? MV_FORCE_INT(val_ptr) + 1 : 1;
		MV_FORCE_MVAL(&val, var_count);
		SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHTNCOUNT, STRLEN(LITERAL_HASHTNCOUNT), val, result);
		if (PUT_SUCCESS != result)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			return result;
		}
	} else if (0 == seq_len)
	{	/* autogenerated short name */
		/* Use #COUNT value as sequence number */
		seq_num = MV_FORCE_INT(trigger_count);
		if (MAX_TRIGNAME_SEQ_NUM < seq_num)
		{
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			return TOO_MANY_TRIGGERS;
		}
		INT2STR(seq_num, uniq_ptr);
	} else
		*uniq_ptr = '\0';	/* user supplied name */
	seq_ptr = trigname_seq_str;
	memcpy(seq_ptr, trig_name, trigname_len);
	seq_ptr += trigname_len;
	if (0 == seq_len)
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
	SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trigname_seq_str, STRLEN(trigname_seq_str),
		name_and_index, trigvn_len + 1 + trigger_count->str.len, result);
	if (PUT_SUCCESS != result)
	{
		TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
		RESTORE_TRIGGER_REGION_INFO;
		return result;
	}
	*seq_ptr++ = TRIGNAME_SEQ_DELIM;
	*seq_ptr = '\0';
	TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
	RESTORE_TRIGGER_REGION_INFO;
	return SEQ_SUCCESS;
}

boolean_t trigger_update_rec(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats)
{
	char			add_delete;
	char			ans[2];
	sgmnt_addrs		*csa;
	boolean_t		cmd_modified;
	char			db_trig_name[MAX_USER_TRIGNAME_LEN + 1];
	boolean_t		full_match;
	mstr			gbl_name;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	boolean_t		kill_cmp;
	uint4			kill_hash;
	mval			mv_hash;
	boolean_t		new_name;
	int			num;
	boolean_t		result;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gv_key			*save_gv_altkey;
	gv_namehead		*save_gvtarget;
	boolean_t		set_cmp;
	uint4			set_hash;
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
	int			trigindx;
	int4			updates;
	char			*values[NUM_SUBS];
	uint4			value_len[NUM_SUBS];

	assert(0 > memcmp(LITERAL_HASHLABEL, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHLABEL), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHCOUNT, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHCOUNT), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHCYCLE, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHCYCLE), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHTNAME, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHTNAME), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHTNCOUNT, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHTNCOUNT), STRLEN(LITERAL_MAXHASHVAL))));
	incr_db_trigger_cycle = TRUE;
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
		if ('*' == *trigger_rec)
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
		} else if ('^' != *trigger_rec)
		{
			if (TRIG_FAILURE == (status = trigger_delete_name(trigger_rec, len, trig_stats)))
				trig_stats[STATS_ERROR]++;
			return status;
		}
	}
	values[GVSUBS_SUB] = tfile_rec_val;	/* GVSUBS will be the first entry set so initialize it */
	if (!trigger_parse(trigger_rec, len, trigvn, values, value_len, (int4)SIZEOF(tfile_rec_val)))
	{
		trig_stats[STATS_ERROR]++;
		return TRIG_FAILURE;
	}
	trigvn_len = STRLEN(trigvn);
	gbl_name.addr = trigvn;
	gbl_name.len = trigvn_len;
	GV_BIND_NAME_ONLY(gd_header, &gbl_name);
	csa = gv_target->gd_csa;
	SETUP_TRIGGER_GLOBAL;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	new_name = check_unique_trigger_name(trigvn, trigvn_len, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB]);
	cmd_modified = skip_set_trigger = FALSE;
	trigindx = 1;
	if (0 != gv_target->root)
	{
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
		if (gvcst_get(&trigger_count))
		{
			if (trigger_already_exists(trigvn, trigvn_len, values, value_len, &trigindx, &set_cmp, &kill_cmp,
						   &full_match))
			{
				if (!new_name && ('+' == add_delete) && (!full_match))
				{
					TRIGGER_SAME_NAME_EXISTS_ERROR;
				}
				assert(('+' == add_delete) || ('-' == add_delete));	/* Has to be + or - */
				updates = modify_record(trigvn, trigvn_len, add_delete, trigindx, values, value_len,
							trigger_count, set_cmp, kill_cmp);
				switch (updates)
				{
					case ADD_NEW_TRIGGER:
						cmd_modified = TRUE;
						trig_cnt_ptr = &trigger_count;
						num = MV_FORCE_INT(trig_cnt_ptr);
						trigindx = ++num;
						MV_FORCE_MVAL(&trigger_count, num);
						break;
					case INVALID_LABEL:
						trig_stats[STATS_ERROR]++;
						util_out_print_gtmio("Current trigger format not compatible with ^!AD trigger " \
						       "being updated at index !UL", FLUSH, trigvn_len, trigvn, trigindx);
						return TRIG_FAILURE;
					case VAL_TOO_LONG:
						trig_stats[STATS_ERROR]++;
						util_out_print_gtmio("^!AD trigger - value larger than record size", FLUSH,
							trigvn_len, trigvn);
						return TRIG_FAILURE;
					case K_ZTK_CONFLICT:
						trig_stats[STATS_ERROR]++;
						util_out_print_gtmio("Command options !AD incompatible with ^!AD trigger at " \
							"index !UL", FLUSH, value_len[CMD_SUB], values[CMD_SUB], trigvn_len,
							trigvn, trigindx);
						return TRIG_FAILURE;
					default:
						if ((0 != (updates & ADD_UPDATE_NAME)) || (0 != (updates & ADD_UPDATE_CMDS))
							|| (0 != (updates & ADD_UPDATE_OPTIONS)))
						{
							MV_FORCE_MVAL(&trigger_count, trigindx);
							trig_stats[STATS_MODIFIED]++;
							if (0 == trig_stats[STATS_ERROR])
								util_out_print_gtmio("^!AD trigger updated at index !UL", FLUSH,
									trigvn_len, trigvn, trigindx);
							skip_set_trigger = TRUE;
						} else if (0 != (updates & DELETE_REC))
						{
							trig_stats[STATS_DELETED]++;
							if (0 == trig_stats[STATS_ERROR])
								util_out_print_gtmio("^!AD trigger deleted", FLUSH, trigvn_len,
									trigvn);
							skip_set_trigger = TRUE;
						} else if ((0 != (updates & SUB_UPDATE_NAME)) || (0 != (updates & SUB_UPDATE_CMDS))
							   || (0 != (updates & SUB_UPDATE_OPTIONS)))
						{
							trig_stats[STATS_MODIFIED]++;
							if (0 == trig_stats[STATS_ERROR])
								util_out_print_gtmio("^!AD trigger updated", FLUSH, trigvn_len,
									trigvn);
							skip_set_trigger = TRUE;
						} else if ('+' == add_delete)
						{
							if (0 == trig_stats[STATS_ERROR])
							{
								util_out_print_gtmio("^!AD trigger already present -- same as " \
									"trigger at index !UL", FLUSH, trigvn_len, trigvn,
									trigindx);
								trig_stats[STATS_UNCHANGED]++;
							}
							skip_set_trigger = TRUE;
						} else
						{
							if (0 == trig_stats[STATS_ERROR])
							{
								util_out_print_gtmio("^!AD trigger does not exist - no action " \
									"taken", FLUSH, trigvn_len, trigvn);
								trig_stats[STATS_UNCHANGED]++;
							}
							skip_set_trigger = TRUE;
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
				num = MV_FORCE_INT(trig_cnt_ptr);
				trigindx = ++num;
				MV_FORCE_MVAL(&trigger_count, num);
			} else
			{
				if (0 == trig_stats[STATS_ERROR])
				{
					trig_stats[STATS_UNCHANGED]++;
					util_out_print_gtmio("^!AD trigger does not exist - no action taken", FLUSH,
							     trigvn_len, trigvn);
				}
				skip_set_trigger = TRUE;

			}
		} else
		{
			if ('-' == add_delete)
			{
				if (0 == trig_stats[STATS_ERROR])
				{
					util_out_print_gtmio("^!AD trigger does not exist - no action taken", FLUSH,
							     trigvn_len, trigvn);
					trig_stats[STATS_UNCHANGED]++;
				}
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
	} else
	{
		if ('-' == add_delete)
		{
			if (0 == trig_stats[STATS_ERROR])
				util_out_print_gtmio("^!AD trigger does not exist - no action taken", FLUSH, trigvn_len, trigvn);
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
		SET_TRIGGER_GLOBAL_SUB_SUB_STR(trigvn, trigvn_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL),
					       HASHT_GBL_CURLABEL, STRLEN(HASHT_GBL_CURLABEL), result);
		if (PUT_SUCCESS != result)
		{
			TOO_LONG_REC_KEY_ERROR_MSG;
			return TRIG_FAILURE;
		}
		trigger_incr_cycle(trigvn, trigvn_len);
		SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT),
						trigger_count, result);
		if (PUT_SUCCESS != result)
		{
			TOO_LONG_REC_KEY_ERROR_MSG;
			return TRIG_FAILURE;
		}
		for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
		{
			if (0 < value_len[sub_indx])
			{
				SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, trigger_count,
					trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), values[sub_indx],
					value_len[sub_indx], result);
				if (PUT_SUCCESS != result)
				{
					TOO_LONG_REC_KEY_ERROR_MSG;
					return TRIG_FAILURE;
				}
			}
		}
		result = add_trigger_hash_entry(trigvn, trigvn_len, values, value_len, trigindx, TRUE, &kill_hash, &set_hash);
		if (PUT_SUCCESS != result)
		{
			TOO_LONG_REC_KEY_ERROR_MSG;
			return TRIG_FAILURE;
		}
		MV_FORCE_UMVAL(&mv_hash, kill_hash);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, trigger_count,
			trigger_subs[LHASH_SUB], STRLEN(trigger_subs[LHASH_SUB]), mv_hash, result);
		if (PUT_SUCCESS != result)
		{
			TOO_LONG_REC_KEY_ERROR_MSG;
			return TRIG_FAILURE;
		}
		MV_FORCE_UMVAL(&mv_hash, set_hash);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, trigger_count, trigger_subs[BHASH_SUB],
			STRLEN(trigger_subs[BHASH_SUB]), mv_hash, result);
		if (PUT_SUCCESS != result)
		{
			TOO_LONG_REC_KEY_ERROR_MSG;
			return TRIG_FAILURE;
		}
		trig_stats[STATS_ADDED]++;
		if (cmd_modified)
			util_out_print_gtmio("^!AD trigger with modified commands added with index !UL", FLUSH, trigvn_len, trigvn,
				       trigindx);
		else
			util_out_print_gtmio("^!AD trigger added with index !UL", FLUSH, trigvn_len, trigvn, trigindx);
	}
	return TRIG_SUCCESS;
}


STATICFNDEF boolean_t trigger_update_rec_helper(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats)
{
	enum cdb_sc		status;
	boolean_t		trigger_error = FALSE;

	ESTABLISH_RET(trigger_item_tpwrap_ch, trigger_error);
	trigger_error = trigger_update_rec(trigger_rec, len, TRUE, trig_stats);
	if (TRIG_SUCCESS == trigger_error)
	{
		status = op_tcommit();
		assert(cdb_sc_normal == status);
	} else
		OP_TROLLBACK(0);
	REVERT;
	return trigger_error;
}

boolean_t trigger_update(char *trigger_rec, uint4 len)
{
	uint4			i;
	uint4			trig_stats[NUM_STATS];
	boolean_t		trigger_error;
	mval			ts_mv;
	int			loopcnt;

	for (i = 0; NUM_STATS > i; i++)
		trig_stats[i] = 0;
	ts_mv.mvtype = MV_STR;
	ts_mv.str.len = 0;
	ts_mv.str.addr = NULL;
	implicit_tstart = TRUE;
	op_tstart(TRUE, TRUE, &ts_mv, 0); 	/* 0 ==> save no locals but RESTART OK */
	assert(FALSE == implicit_tstart);	/* should have been reset by op_tstart at very beginning */
	/* The following for loop structure is similar to that in module trigger_trgfile.c (function "trigger_trgfile_tpwrap")
	 * and module gv_trigger.c (function gvtr_db_tpwrap) so any changes here might need to be reflected there as well.
	 */
	for ( loopcnt = 0; ; loopcnt++)
	{
		trigger_error = trigger_update_rec_helper(trigger_rec, len, TRUE, trig_stats);
		if (!dollar_tlevel)
			break;
		/* We expect the above function to return with either op_tcommit or a tp_restart invoked.
		 * In the case of op_tcommit, we expect dollar_tlevel to be 0 and if so we break out of the loop.
		 * In the tp_restart case, we expect a maximum of 4 tries/retries and much lesser usually.
		 * Additionally we also want to avoid an infinite loop so limit the loop to what is considered
		 * a huge iteration count and GTMASSERT if that is reached as it suggests an out-of-design situation.
		 */
		if (TPWRAP_HELPER_MAX_ATTEMPTS < loopcnt)
			GTMASSERT;
	}
	return (TRIG_FAILURE == trigger_error);
}
#endif /* GTM_TRIGGER */
