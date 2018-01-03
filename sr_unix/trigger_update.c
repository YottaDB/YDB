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
#include "gtm_trigger_trc.h"
#include "gtm_string.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "targ_alloc.h"			/* for SET_GVTARGET_TO_HASHT_GBL */
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
#include "hashtab_mname.h"
#include "zshow.h"		/* for "format2disp" prototype */
#include "compiler.h"
#include "t_begin.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "change_reg.h"		/* for "change_reg" prototype */
#include "gvnh_spanreg.h"	/* for "gvnh_spanreg_subs_gvt_init" prototype */
#include "mu_interactive.h"	/* for prompt looping */
#include "is_file_identical.h"
#include "anticipatory_freeze.h"
#include "gtm_repl_multi_inst.h" /* for DISALLOW_MULTIINST_UPDATE_IN_TP */

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		dollar_ztrigger_invoked;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_addr			*gd_header;
GBLREF	io_pair			io_curr_device;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	trans_num		local_tn;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	int			tprestart_state;
GBLREF	volatile boolean_t	timer_in_handler;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

error_def(ERR_DBROLLEDBACK);
error_def(ERR_NEEDTRIGUPGRD);
error_def(ERR_REMOTEDBNOTRIG);
error_def(ERR_TEXT);
error_def(ERR_TPRETRY);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGDEFBAD);
error_def(ERR_TRIGLOADFAIL);
error_def(ERR_TRIGMODREGNOTRW);
error_def(ERR_TRIGNAMBAD);

LITREF	mval			gvtr_cmd_mval[GVTR_CMDTYPES];
LITREF	int4			gvtr_cmd_mask[GVTR_CMDTYPES];
LITREF	mval			literal_one;
LITREF	char 			*trigger_subs[];

static	boolean_t		mustprompt   = TRUE;
static	boolean_t		promptanswer = TRUE;

#define	MAX_COMMANDS_LEN	32		/* Need room for S,K,ZK,ZTK + room for expansion */
#define	MAX_OPTIONS_LEN		32		/* Need room for NOI,NOC + room for expansion */
#define	MAX_TRIGNAME_SEQ_NUM	999999
#define	MAX_TRIG_DISPLEN	80		/* maximum length of a trigger that is displayed in case of errors */
#define	LITERAL_M		"M"
#define	OPTIONS_I		1
#define	OPTIONS_NOI		2
#define	OPTIONS_C		4
#define	OPTIONS_NOC		8

#define	NO_NAME_CHANGE		0
#define	NO_CMD_CHANGE		0
#define	NO_OPTIONS_CHANGE	0

#define	ADD_UPDATE_NAME		0x01
#define	ADD_UPDATE_CMDS		0x02
#define	ADD_UPDATE_OPTIONS	0x04

#define	SUB_UPDATE_NAME		0x10
#define	SUB_UPDATE_CMDS		0x20
#define	DELETE_REC		0x80

/* Defines macros for types of triggers; one is SET type triggers, one is Non-SET type triggers */
#define	OPR_KILL		0
#define	OPR_SET			1
#define	NUM_OPRS		2
#define	OPR_SETKILL		2

#define	SEQ_SUCCESS		0

#define	MAX_HASH_LEN		MAX_HASH_INDEX_LEN + 1 + MAX_DIGITS_IN_INT

#define	BUILD_COMMAND_BITMAP(BITMAP, COMMANDS)									\
{														\
	char		lcl_cmds[MAX_COMMANDS_LEN + 1];								\
	char		*lcl_ptr, *strtok_ptr;									\
														\
	memcpy(lcl_cmds, COMMANDS, STRLEN(COMMANDS) + 1);							\
	BITMAP = 0;												\
	lcl_ptr = STRTOK_R(lcl_cmds, ",", &strtok_ptr);								\
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
								/* Parsing should have found invalid command */	\
							        assertpro(FALSE && lcl_ptr[2]);			\
								break;						\
						}								\
						break;								\
					default:								\
						/* Parsing should have found invalid command */			\
						assertpro(FALSE && lcl_ptr[1]);					\
						break;								\
				}										\
				break;										\
			default:										\
				/* Parsing should have found invalid command */					\
				assertpro(FALSE && lcl_ptr[0]);							\
				break;										\
		}												\
	} while (lcl_ptr = STRTOK_R(NULL, ",", &strtok_ptr));							\
}

#define	COMMAND_BITMAP_TO_STR(COMMANDS, BITMAP, LEN)										\
{																\
	int		count, cmdtype, lcl_len;										\
	char		*lcl_ptr;												\
																\
	count = 0;														\
	lcl_ptr = COMMANDS;													\
	lcl_len = LEN;														\
	for (cmdtype = 0; cmdtype < GVTR_CMDTYPES; cmdtype++)									\
	{															\
		if (gvtr_cmd_mask[cmdtype] & (BITMAP))										\
		{														\
			ADD_COMMA_IF_NEEDED(count, lcl_ptr, lcl_len);								\
			ADD_STRING(count, lcl_ptr, gvtr_cmd_mval[cmdtype].str.len, gvtr_cmd_mval[cmdtype].str.addr, lcl_len);	\
		}														\
	}															\
	*lcl_ptr = '\0';													\
	LEN = STRLEN(COMMANDS);													\
}

#define	BUILD_OPTION_BITMAP(BITMAP, OPTIONS)									\
{														\
	char		lcl_options[MAX_OPTIONS_LEN + 1];							\
	char		*lcl_ptr, *strtok_ptr;									\
														\
	memcpy(lcl_options, OPTIONS, STRLEN(OPTIONS) + 1);							\
	BITMAP = 0;												\
	lcl_ptr = STRTOK_R(lcl_options, ",", &strtok_ptr);							\
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
							/* Parsing should have found invalid command */		\
							assertpro(FALSE && lcl_ptr[2]);				\
							break;							\
					}									\
					break;									\
				default:									\
					/* Parsing should have found invalid command */				\
					assertpro(FALSE && lcl_ptr[0]);						\
					break;									\
			}											\
		} while (lcl_ptr = STRTOK_R(NULL, ",", &strtok_ptr));						\
}

#define	OPTION_BITMAP_TO_STR(OPTIONS, BITMAP, LEN)								\
{														\
	int		count, lcl_len;										\
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

#define TOO_LONG_REC_KEY_ERROR_MSG									\
{													\
	UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);				\
	if (KEY_TOO_LONG == result)									\
		util_out_print_gtmio("Error : ^!AD trigger - key larger than max key size",		\
			FLUSH, disp_trigvn_len, disp_trigvn);						\
	else												\
		util_out_print_gtmio("Error : ^!AD trigger - value larger than max record size",	\
			FLUSH, disp_trigvn_len, disp_trigvn);						\
}

#define IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(RESULT)					\
{													\
	if (PUT_SUCCESS != RESULT)									\
	{												\
		TOO_LONG_REC_KEY_ERROR_MSG;								\
		RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);						\
	}												\
}

#define TRIGGER_SAME_NAME_EXISTS_ERROR(OPNAME, DISP_TRIGVN_LEN, DISP_TRIGVN)						\
{															\
	assert(dollar_tlevel);												\
	if (CDB_STAGNATE > t_tries)											\
	{	/* Directly jump to final retry since we cannot issue this error accurately				\
		 * unless we are in the final retry. Dont waste time in intermediate tries.				\
		 * But before then record the fact that the intermediate tries had normal status.			\
		 */													\
	 	for ( ; t_tries < (CDB_STAGNATE - 1); t_tries++)							\
			t_fail_hist[t_tries] = cdb_sc_normal;								\
		t_retry(cdb_sc_triggermod);										\
	}														\
	UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);						\
	util_out_print_gtmio("Error : !AZ trigger on ^!AD not added as another trigger named !AD already exists",	\
			FLUSH, OPNAME, DISP_TRIGVN_LEN, DISP_TRIGVN,							\
			value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);							\
	RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);									\
}

/* This error macro is used for all definition errors where the target is ^#t(GVN,<#LABEL|#COUNT|#CYCLE>) */
#define HASHT_DEFINITION_RETRY_OR_ERROR(SUBSCRIPT, MOREINFO, CSA)	\
{									\
	if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))		\
		t_retry(cdb_sc_triggermod);				\
	else								\
	{								\
		HASHT_DEFINITION_ERROR(SUBSCRIPT, MOREINFO, CSA);	\
	}								\
}

#define HASHT_DEFINITION_ERROR(SUBSCRIPT, MOREINFO, CSA)					\
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
	if ((gvtr_cmd_mask[GVTR_CMDTYPE_SET] & orig_cmd_bm) && !(gvtr_cmd_mask[GVTR_CMDTYPE_SET] & new_cmd_bm))
	{	/* SET was removed from the commands, so delete the SET specific attributes */
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx,
				trigger_subs[DELIM_SUB], STRLEN(trigger_subs[DELIM_SUB]));
		gvcst_kill(TRUE);
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx,
				trigger_subs[ZDELIM_SUB], STRLEN(trigger_subs[ZDELIM_SUB]));
		gvcst_kill(TRUE);
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx,
				trigger_subs[PIECES_SUB], STRLEN(trigger_subs[PIECES_SUB]));
		gvcst_kill(TRUE);
	}
	return SUB_UPDATE_CMDS;
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
	if (tf_trig_name_len && (tf_trig_name_len != STRLEN(db_trig_name) - 1)
		|| memcmp(tf_trig_name, db_trig_name, tf_trig_name_len))
	{
		if (!validate_label(trigvn, trigvn_len))
			return INVALID_LABEL;
		i2mval(&mv_trig_indx, trigger_index);
		tf_trig_name[tf_trig_name_len++] = TRIGNAME_SEQ_DELIM;
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx,
			trigger_subs[TRIGNAME_SUB], STRLEN(trigger_subs[TRIGNAME_SUB]), tf_trig_name, tf_trig_name_len, result);
		if (PUT_SUCCESS != result)
			return result;
		cleanup_trigger_name(trigvn, trigvn_len, db_trig_name, STRLEN(db_trig_name));
		retval = ADD_UPDATE_NAME;
	}
	return retval;
}

/*
 * Input:	trigger_name and trigger_name_length
 * 		[optional] srch_reg (when non-NULL this is the only region to search)
 *
 * Output:	returns TRUE if trigger name is found, false if not.
 * 		srch_reg set to the region the name was found in.
 * 		val is the "<gbl>\0<trigindx>" string to which the name points
 *
 * This function is similar to check_unique_trigger_name_full(), but is only called from
 * trigger_source_read_andor_verify()
 */
boolean_t trigger_name_search(char *trigger_name, uint4 trigger_name_len, mval *val, gd_region **srch_reg)
{
	boolean_t		name_found;
	char			*ptr, *ptr2;
	gd_region		*reg, *reg_top;
	gd_region		*save_gv_cur_region;
	gv_key			save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gv_namehead		*save_gv_target;
	gvnh_reg_t		*gvnh_reg;
	int			len;
	mname_entry		gvname;
	sgm_info		*save_sgm_info_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool;

	/* Example trigger name could be x#1#A:BREG to indicate trigger on global ^x with an autogenerated name x#1
	 * that exists in multiple regions and hence had a runtime disambiguator of x#1#A. The :BREG is a region-level
	 * disambiguator to indicate we want to focus on BREG region to search for triggers with the name x#1.
	 * We dont expect the input trigger name to contain the runtime and region-level disambiguator but in case both
	 * are present we treat it as if the runtime disambiguator was absent.
	 */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Remove trailing # (if any) in trigger name before searching in ^#t as it is not stored in the ^#t("#TNAME",...) node */
	assert('#' == trigger_name[trigger_name_len - 1]);
	if ('#' == trigger_name[trigger_name_len - 1])
		trigger_name_len--;
	/* We only check user supplied names for uniqueness. With autogenerated names it is possible for
	 * same name to exist in multiple regions (in case two globals with global name > 21 chars map to
	 * different regions and have one trigger per global name installed with auto-generated names.
	 * But even in that case, at most one auto-generated name per region is possible. So we have a limit
	 * on the max # of duplicated auto-generated names.
	 */
	assert(0 < trigger_name_len);
	SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	name_found = FALSE;
	reg = *srch_reg;
	if (NULL != reg)
		reg_top = reg + 1;	/* make sure we dont go in the for loop more than once */
	else
	{
		reg = gd_header->regions;
		reg_top = reg + gd_header->n_regions;
		assert(reg < reg_top);
	}
	for ( ; reg < reg_top; reg++)
	{
		if (IS_STATSDB_REGNAME(reg))
			continue;
		GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
		if (NULL == cs_addrs)	/* not BG or MM access method */
			continue;
		/* gv_target now points to ^#t in region "reg" */
		if (0 == gv_target->root)
			continue;
		/* $get(^#t("#TNAME",trigger_name)) */
		BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), trigger_name, trigger_name_len);
		if (!gvcst_get(val))
			continue;
		ptr = val->str.addr;
		ptr2 = memchr(ptr, '\0', val->str.len);	/* Do it this way since "val" has multiple fields null separated */
		if (NULL == ptr2)
		{	/* We expect $c(0) in the middle of ptr. If we dont find it, this is a restartable situation */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TRIGNAMBAD, 4, LEN_AND_LIT("\"#TNAME\""),
				trigger_name_len, trigger_name);
		}
		len = ptr2 - ptr;
		assert(('\0' == *ptr2) && (val->str.len > len));
		gvname.var_name.addr = val->str.addr;
		gvname.var_name.len = len;
		/* Check if global name is indeed mapped to this region by the gld.
		 * If not treat this case as if the trigger is invisible to us i.e. move on to next region.
		 */
		COMPUTE_HASH_MNAME(&gvname);
		GV_BIND_NAME_ONLY(gd_header, &gvname, gvnh_reg);	/* does tp_set_sgm() */
		if (((NULL == gvnh_reg->gvspan) && (gv_cur_region != reg))
				|| ((NULL != gvnh_reg->gvspan) && !gvnh_spanreg_ismapped(gvnh_reg, gd_header, reg)))
			continue;
		*srch_reg = reg;
		name_found = TRUE;
		break;
	}
	RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	return name_found;
}

/* Returns TRUE if name is NOT found. FALSE if name is found. If name is found, "val" holds the value of the found node */
boolean_t check_unique_trigger_name_full(char **values, uint4 *value_len, mval *val, boolean_t *new_match,
				char *trigvn, int trigvn_len, stringkey *kill_trigger_hash, stringkey *set_trigger_hash)
{
	boolean_t		overall_name_found, this_name_found;
	gd_region		*reg, *reg_top;
	gv_key			save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool;
	int			set_index, kill_index;
	boolean_t		db_matched_set, db_matched_kill, full_match, trigger_exists;
	mval			setname, killname;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(if (WBTEST_HELPOUT_TRIGNAMEUNIQ == gtm_white_box_test_case_number) return TRUE;)
	/* We only check user supplied names for uniqueness. With autogenerated names it is possible for
	 * same name to exist in multiple regions (in case two globals with global name > 21 chars map to
	 * different regions and have one trigger per global name installed with auto-generated names.
	 * But even in that case, at most one auto-generated name per region is possible. So we have a limit
	 * on the max # of duplicated auto-generated names.
	 */
	*new_match = TRUE;
	if (0 == value_len[TRIGNAME_SUB])
		return TRUE;
	SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	overall_name_found = FALSE;
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions; reg < reg_top; reg++)
	{
		if (IS_STATSDB_REGNAME(reg))
			continue;
		GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
		if (NULL == cs_addrs)	/* not BG or MM access method */
			continue;
		/* gv_target now points to ^#t in region "reg" */
		if (0 == gv_target->root)
			continue;
		/* $get(^#t("#TNAME",trigger_name) */
		BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME),
			values[TRIGNAME_SUB], value_len[TRIGNAME_SUB]);
		this_name_found = gvcst_get(val);
		if (this_name_found)
		{
			overall_name_found = TRUE;
			trigger_exists = trigger_already_exists(trigvn, trigvn_len, values, value_len,
								set_trigger_hash, kill_trigger_hash,
								&set_index, &kill_index, &db_matched_set, &db_matched_kill,
								&full_match, &setname, &killname);
			if (!full_match)
			{
				*new_match = FALSE;
				break;
			}
		}
	}
	RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	return !overall_name_found;
}

STATICFNDEF int4 add_trigger_hash_entry(char *trigvn, int trigvn_len, char *cmd_value, int trigindx, boolean_t add_kill_hash,
		stringkey *kill_hash, stringkey *set_hash)
{
	int			hash_indx;
	char			indx_str[MAX_DIGITS_IN_INT];
	uint4			len;
	mval			mv_hash;
	mval			mv_indx, *mv_indx_ptr;
	char			name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	int			num_len;
	char			*ptr;
	int4			result;
	boolean_t		set_cmp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!gv_cur_region->read_only);	/* caller should have already checked this */
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
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
		if (set_hash->hash_code != kill_hash->hash_code)
		{
			MV_FORCE_UMVAL(&mv_hash, set_hash->hash_code);
			if (gv_target->root)
			{
				BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len,
					LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
				op_zprevious(&mv_indx);
				hash_indx = (0 == mv_indx.str.len) ? 1 : (mval2i(mv_indx_ptr) + 1);
			} else
				hash_indx = 1;
			i2mval(&mv_indx, hash_indx);
			SET_TRIGGER_GLOBAL_SUB_SUB_MSUB_MSUB_STR(trigvn, trigvn_len,
				LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_indx, name_and_index, len, result);
			if (PUT_SUCCESS != result)
				return result;
		}
		/* else: the next block of code for kill hash processing will add this hashcode in ^#t("#TRHASH",...) */
	} else
		set_hash->hash_code = 0;
	if (add_kill_hash)
	{
		MV_FORCE_UMVAL(&mv_hash, kill_hash->hash_code);
		if (gv_target->root)
		{
			BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len,
				LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
			op_zprevious(&mv_indx);
			hash_indx = (0 == mv_indx.str.len) ? 1 : (mval2i(mv_indx_ptr) + 1);
		} else
			hash_indx = 1;
		i2mval(&mv_indx, hash_indx);
		SET_TRIGGER_GLOBAL_SUB_SUB_MSUB_MSUB_STR(trigvn, trigvn_len,
			LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_indx, name_and_index, len, result);
		if (PUT_SUCCESS != result)
			return result;
	} else
		kill_hash->hash_code = 0;
	return PUT_SUCCESS;
}

STATICFNDEF boolean_t trigger_already_exists(char *trigvn, int trigvn_len, char **values, uint4 *value_len,	/* input parm */
						stringkey *set_trigger_hash, stringkey *kill_trigger_hash,	/* input parm */
						int *set_index, int *kill_index, boolean_t *set_cmp_result,	/* output parm */
						boolean_t *kill_cmp_result, boolean_t *full_match,		/* output parm */
						mval *setname, mval *killname)					/* output parm */
{
	sgmnt_addrs		*csa;
	boolean_t		db_has_K;
	boolean_t		db_has_S;
	int			hash_indx;
	boolean_t		kill_cmp, kill_found;
	int			kill_indx;
	boolean_t		set_cmp, set_found, set_name_match, kill_name_match;
	int			set_indx;
	mval			trigindx;
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];	/* needed for HASHT_GVN_DEFINITION_RETRY_OR_ERROR macro */
	int4			util_len;
	mval			val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	assert(gv_target->root);			/* should have been ensured by caller */
	/* Test with BHASH or LHASH.
	 *	^#t("GBL",1,"CMD") could contain one or more of "S,K,ZK,ZTK,ZTR".
	 * Out of the 5 commands above, a S type trigger uses the "BHASH" hash value.
	 * Everything else (K, ZK, ZTK, ZTR) uses the "LHASH" value.
	 * An easy check of one of these 4 commands is a chek for the letter K or R.
	 */
	set_cmp = (NULL != strchr(values[CMD_SUB], 'S'));
	kill_cmp = ((NULL != strchr(values[CMD_SUB], 'K')) || (NULL != strchr(values[CMD_SUB], 'R')));
	set_found = kill_found = set_name_match = kill_name_match = FALSE;
	csa = cs_addrs;
	if (set_cmp)
	{	/* test for SET hash match if SET command specified */
		set_found = search_triggers(trigvn, trigvn_len, values, value_len, set_trigger_hash, &hash_indx, &set_indx, TRUE);
		if (set_found)
		{
			i2mval(&trigindx, set_indx);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
					STRLEN(trigger_subs[TRIGNAME_SUB]));
			if (!gvcst_get(setname)) /* There has to be a name value */
				HASHT_GVN_DEFINITION_RETRY_OR_ERROR(set_indx, ",\"TRIGNAME\"", csa);
			setname->str.len--;	/* remove the # at the tail of the trigger name */
			set_name_match = ((value_len[TRIGNAME_SUB] == setname->str.len)
				&& !memcmp(setname->str.addr, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB]));
		}
	} else
		set_indx = 0;
	*set_cmp_result = set_found;
	kill_indx = -1;
	if (kill_cmp || !set_found)
	{	/* if SET is not found OR KILL is specified in commands, test for KILL hash match */
		kill_found = search_triggers(trigvn, trigvn_len, values, value_len, kill_trigger_hash,
										&hash_indx, &kill_indx, FALSE);
		if (kill_found)
		{
			if (!set_found || (kill_indx != set_indx))
			{
				i2mval(&trigindx, kill_indx);
				BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[CMD_SUB],
								 STRLEN(trigger_subs[CMD_SUB]));
				if (!gvcst_get(&val))	/* There has to be a command string */
					HASHT_GVN_DEFINITION_RETRY_OR_ERROR(kill_indx, ",\"CMD\"", csa);
				/* val.str.addr would contain something like the following
				 *	^#t("GBL",1,"CMD")="S,K,ZK,ZTK,ZTR".
				 * Out of the 5 commands above, a S type trigger uses the "BHASH" hash value.
				 * Everything else (K, ZK, ZTK, ZTR) uses the "LHASH" value.
				 */
				db_has_S = (NULL != memchr(val.str.addr, 'S', val.str.len));
				db_has_K = ((NULL != memchr(val.str.addr, 'K', val.str.len))
						|| (NULL != memchr(val.str.addr, 'R', val.str.len)));
				if (!kill_cmp)
					kill_found = (db_has_K && !db_has_S);
				/* $get(^#t(trigvn,trigindx,"TRIGNAME") */
				BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
								 STRLEN(trigger_subs[TRIGNAME_SUB]));
				if (!gvcst_get(killname)) /* There has to be a name string */
					HASHT_GVN_DEFINITION_RETRY_OR_ERROR(kill_indx, ",\"TRIGNAME\"", csa);
				killname->str.len--;	/* remove the # at the tail of the trigger name */
				kill_name_match = ((value_len[TRIGNAME_SUB] == killname->str.len)
					&& !memcmp(killname->str.addr, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB]));
				if (set_cmp && !set_found && !db_has_S)
				{
					*setname = *killname;
					set_indx = kill_indx;
				}
			} else
				*killname = *setname;
		} else
		{
			kill_indx = -1;
			if (!set_found)
				set_indx = 0;
		}
		if (set_cmp && (kill_indx == set_indx))
			kill_indx = -1;
	}
	*kill_index = kill_indx;
	*kill_cmp_result = kill_found ? TRUE : set_found;
	*set_index = set_indx;
	/* If there is both a set and a kill and the set components don't match, there is no name match no matter if the kill
	 * components match or not.  If there is no set, then the name match is only based on the kill components.
	 */
	*full_match = (set_cmp ? set_name_match : kill_name_match);
	return (set_found || kill_found);
}

STATICFNDEF int4 add_trigger_cmd_attributes(char *trigvn, int trigvn_len, int trigger_index, char *trig_cmds, char **values,
		uint4 *value_len, boolean_t db_matched_set, boolean_t db_matched_kill, stringkey *kill_hash, stringkey *set_hash,
		uint4 db_cmd_bm, uint4 tf_cmd_bm)
{
	char			cmd_str[MAX_COMMANDS_LEN];
	int			cmd_str_len;
	mval			mv_hash;
	mval			mv_trig_indx;
	int4			result;
	uint4			tmp_bm;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!validate_label(trigvn, trigvn_len))
		return INVALID_LABEL;
	/* If the trigger file command string is contained in the database command and either
	 *   1. the trigger file command has no SET components or
	 *   2. the trigger file command matched a database SET component
	 * then the trigger file command is already in the database, so return.
	 */
	if ((tf_cmd_bm == (db_cmd_bm & tf_cmd_bm)) && (!(tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_SET]) || db_matched_set))
		return NO_CMD_CHANGE;
	assert(!db_matched_set || db_matched_kill);
	/* If merge would combine K and ZTK, it's an error */
	if (((db_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_KILL]) && (tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_ZTKILL]))
			|| ((db_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_ZTKILL]) && (tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_KILL])))
		return K_ZTK_CONFLICT;
	if (!db_matched_set && db_matched_kill
		&& (tf_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_SET]) && (db_cmd_bm & gvtr_cmd_mask[GVTR_CMDTYPE_SET]))
	{
		tmp_bm = (db_cmd_bm | tf_cmd_bm);
		if (tmp_bm == db_cmd_bm)
		{	/* No change to commands in the KILL trigger entry in db.
			 * SET trigger (if it exists and is in a different trigger) to be processed separately.
			 */
			return ADD_SET_NOCHNG_KILL_TRIG;
		}
		/* Commands are being added to the existing KILL trigger entry in db */
		cmd_str_len = ARRAYSIZE(cmd_str);
		COMMAND_BITMAP_TO_STR(cmd_str, tmp_bm, cmd_str_len);
		i2mval(&mv_trig_indx, trigger_index);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[CMD_SUB],
						    STRLEN(trigger_subs[CMD_SUB]), cmd_str, cmd_str_len, result);
		assert(result == PUT_SUCCESS);
		return (result == PUT_SUCCESS) ? ADD_SET_MODIFY_KILL_TRIG : result;
	}
	cmd_str_len = ARRAYSIZE(cmd_str);
	COMMAND_BITMAP_TO_STR(cmd_str, db_cmd_bm | tf_cmd_bm, cmd_str_len);
	i2mval(&mv_trig_indx, trigger_index);
	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, mv_trig_indx, trigger_subs[CMD_SUB], STRLEN(trigger_subs[CMD_SUB]),
					    cmd_str, cmd_str_len, result);
	if (PUT_SUCCESS != result)
		return result;
	strcpy(trig_cmds, cmd_str);
	if ((gvtr_cmd_mask[GVTR_CMDTYPE_SET] & tf_cmd_bm) && !(gvtr_cmd_mask[GVTR_CMDTYPE_SET] & db_cmd_bm))
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
		if (!(gvtr_cmd_mask[GVTR_CMDTYPE_SET] & db_cmd_bm) && (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & tf_cmd_bm))
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
	if (tf_option_bm == db_option_bm)
		/* If trigger file OPTIONS is contained in the DB OPTIONS, then trigger file entry is already in DB, just return */
		return NO_OPTIONS_CHANGE;
	tmp_bm = tf_option_bm;
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
	return ADD_UPDATE_OPTIONS;
}

STATICFNDEF boolean_t subtract_trigger_cmd_attributes(char *trigvn, int trigvn_len, char *trig_cmds, char **values,
		uint4 *value_len, boolean_t db_matched_set, stringkey *kill_hash, stringkey *set_hash, int trigger_index,
		uint4 db_cmd_bm, uint4 tf_cmd_bm)
{
	char			cmd_str[MAX_COMMANDS_LEN];
	int			cmd_str_len;
	uint4			restore_set = 0;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!db_matched_set && (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & tf_cmd_bm))
	{	/* If the set compare failed, we don't want to consider the SET */
		restore_set = gvtr_cmd_mask[GVTR_CMDTYPE_SET];
		tf_cmd_bm &= ~restore_set;
	}
	if (0 == (db_cmd_bm & tf_cmd_bm))
		return 0; /* If trigger file CMD does NOT overlap with the DB CMD, then no match. So no delete. Just return */
	cmd_str_len = ARRAYSIZE(cmd_str);
	if (db_cmd_bm != (db_cmd_bm & tf_cmd_bm))
	{	/* combine cmds - subtract trigger file attributes from db attributes */
		COMMAND_BITMAP_TO_STR(cmd_str, (db_cmd_bm & tf_cmd_bm) ^ db_cmd_bm, cmd_str_len);
		strcpy(trig_cmds, cmd_str);
		/* If we lost the "S", need to delete the set hash value */
		if ((0 != (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & db_cmd_bm))
				&& (0 == (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & ((db_cmd_bm & tf_cmd_bm) ^ db_cmd_bm))))
			cleanup_trigger_hash(trigvn, trigvn_len, values, value_len, set_hash, kill_hash, FALSE, trigger_index);
	} else
	{	/* Both cmds are the same - candidate for delete */
		trig_cmds[0] = '\0';
	}
	return SUB_UPDATE_CMDS;
}

STATICFNDEF int4 modify_record(char *trigvn, int trigvn_len, char add_delete, int trigger_index, char **values, uint4 *value_len,
		mval *trigger_count, boolean_t db_matched_set, boolean_t db_matched_kill,
		stringkey *kill_hash, stringkey *set_hash, int set_kill_bitmask)
{
	char			db_cmds[MAX_COMMANDS_LEN + 1];
	boolean_t		name_matches, sub_cmds;
	int4			result;
	uint4			retval;
	mval			trigindx;
	char			trig_cmds[MAX_COMMANDS_LEN + 1];
	char			trig_name[MAX_USER_TRIGNAME_LEN + 2];	/* One spot for # delimiter and one for trailing 0 */
	char			trig_options[MAX_OPTIONS_LEN + 1];
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];
	int4			util_len;
	int			trig_cmds_len;
	int			trig_name_len;
	int			trig_options_len;
	mval			val;
	uint4			db_cmd_bm, tf_cmd_bm;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	i2mval(&trigindx, trigger_index);
	/* get(^#t(GVN,trigindx,"CMD") */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[CMD_SUB], STRLEN(trigger_subs[CMD_SUB]));
	if (!gvcst_get(&val)) /* There has to be a command string */
		HASHT_GVN_DEFINITION_RETRY_OR_ERROR(trigger_index, ",\"CMD\"", REG2CSA(gv_cur_region));
	trig_cmds_len = MIN(val.str.len, MAX_COMMANDS_LEN);
	memcpy(trig_cmds, val.str.addr, trig_cmds_len);
	trig_cmds[trig_cmds_len] = '\0';
	BUILD_COMMAND_BITMAP(db_cmd_bm, trig_cmds);
	BUILD_COMMAND_BITMAP(tf_cmd_bm, values[CMD_SUB]);
	/* If trigger file has specified SET and/or KILL triggers and each of them matched to different triggers in database,
	 * filter out only the respective category of triggers to go forward with the command addition/deletion.
	 */
	if (OPR_KILL == set_kill_bitmask)
		tf_cmd_bm &= ~gvtr_cmd_mask[GVTR_CMDTYPE_SET];
	else if (OPR_SET == set_kill_bitmask)
		tf_cmd_bm &= gvtr_cmd_mask[GVTR_CMDTYPE_SET];
	/* get(^#t(GVN,trigindx,"OPTIONS") */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[OPTIONS_SUB],
		STRLEN(trigger_subs[OPTIONS_SUB]));
	if (gvcst_get(&val))
	{
		trig_options_len = MIN(val.str.len, MAX_OPTIONS_LEN);
		memcpy(trig_options, val.str.addr, trig_options_len);
	} else
		trig_options_len = 0;
	trig_options[trig_options_len] = '\0';
	/* get(^#t(GVN,trigindx,"TRIGNAME") */
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, trigindx, trigger_subs[TRIGNAME_SUB],
		STRLEN(trigger_subs[TRIGNAME_SUB]));
	if (gvcst_get(&val))
	{
		trig_name_len = MIN(val.str.len, ARRAYSIZE(trig_name));
		memcpy(trig_name, val.str.addr, trig_name_len);
	} else
		trig_name_len = 0;
	trig_name[trig_name_len] = '\0';
	if ('+' == add_delete)
	{
		/* Process -OPTIONS */
		result = add_trigger_options_attributes(trigvn, trigvn_len, trigger_index, trig_options, values, value_len);
		assert((NO_OPTIONS_CHANGE == result) || (ADD_UPDATE_OPTIONS == result)				  /* 0 or 0x04 */
			|| (INVALID_LABEL == result) || (VAL_TOO_LONG == result) || (KEY_TOO_LONG == result));	  /* < 0 */
		if (0 > result)
			return result;
		assert((NO_OPTIONS_CHANGE == result) || (ADD_UPDATE_OPTIONS == result));
		if (NO_OPTIONS_CHANGE != result)
		{	/* Check if specified list of commands matches commands in database. If not cannot proceed */
			if (tf_cmd_bm != db_cmd_bm)
				return OPTIONS_CMDS_CONFLICT;
		}
		retval = result;
		/* Process -NAME */
		result = update_trigger_name(trigvn, trigvn_len, trigger_index, trig_name, values[TRIGNAME_SUB],
					     value_len[TRIGNAME_SUB]);
		assert((NO_NAME_CHANGE == result) || (ADD_UPDATE_NAME == result)				  /* 0 or 0x01 */
			|| (INVALID_LABEL == result) || (VAL_TOO_LONG == result) || (KEY_TOO_LONG == result));	  /* < 0 */
		if (0 > result)
			return result;
		assert((NO_NAME_CHANGE == result) || (ADD_UPDATE_NAME == result));
		if (NO_NAME_CHANGE != result)
		{	/* Check if specified list of commands contains commands in database. If not cannot proceed */
			if ((tf_cmd_bm &db_cmd_bm) != db_cmd_bm)
				return NAME_CMDS_CONFLICT;
		}
		retval |= result;
		/* Process -CMD */
		result = add_trigger_cmd_attributes(trigvn, trigvn_len, trigger_index, trig_cmds, values, value_len,
						    db_matched_set, db_matched_kill, kill_hash, set_hash, db_cmd_bm, tf_cmd_bm);
		assert((NO_CMD_CHANGE == result) || (ADD_UPDATE_CMDS == result)				 /* 0 or 0x02 */
			|| (ADD_SET_MODIFY_KILL_TRIG == result) || (ADD_SET_NOCHNG_KILL_TRIG == result)	 /* < 0 */
			|| (INVALID_LABEL == result) || (K_ZTK_CONFLICT == result)			 /* < 0 */
			|| (VAL_TOO_LONG == result) || (KEY_TOO_LONG == result));			 /* < 0 */
		if (0 > result)
			return result;
		assert((NO_CMD_CHANGE == result) || (ADD_UPDATE_CMDS == result));
		retval |= result;
	} else
	{
		name_matches = (0 == value_len[TRIGNAME_SUB])
					|| ((value_len[TRIGNAME_SUB] == (STRLEN(trig_name) - 1))
						&& (0 == memcmp(values[TRIGNAME_SUB], trig_name, value_len[TRIGNAME_SUB])));
		if (!name_matches)
			return 0;
		memcpy(db_cmds, trig_cmds, SIZEOF(trig_cmds));
		sub_cmds = subtract_trigger_cmd_attributes(trigvn, trigvn_len, trig_cmds, values, value_len,
							db_matched_set, kill_hash, set_hash, trigger_index, db_cmd_bm, tf_cmd_bm);
		/* options are ignored in case of deletes so no need for "subtract_trigger_options_attributes()" */
		if (!sub_cmds)
			return 0;
		if (0 == trig_cmds[0])
		{
			result = trigger_delete(trigvn, trigvn_len, trigger_count, trigger_index);
			assert((VAL_TOO_LONG == result) || (KEY_TOO_LONG == result)			/*  < 0 */
					|| (PUT_SUCCESS == result));					/* == 0 */
			if (0 > result)
				return result;
			return DELETE_REC;
		}
		retval = 0;
		if (sub_cmds)
		{
			result = update_commands(trigvn, trigvn_len, trigger_index, trig_cmds, db_cmds);
			if (SUB_UPDATE_CMDS != result)
				return result;
			retval |= result;
		}
	}
	return retval;
}

STATICFNDEF int4 gen_trigname_sequence(char *trigvn, int trigvn_len, mval *trigger_count, char *user_trigname_str,
				       uint4 user_trigname_len)
{
	char			name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	char			*ptr1;
	int			seq_num;
	int4			result;
	char			*seq_ptr, *uniq_ptr;
	char			trig_name[MAX_USER_TRIGNAME_LEN + 1];
	uint4			trigname_len, uniq_ptr_len;
	char			unique_seq_str[NUM_TRIGNAME_SEQ_CHARS + 1];
	mval			val, *val_ptr;
	int			var_count, max_seq_num;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(MAX_USER_TRIGNAME_LEN >= user_trigname_len);
	assert(!gv_cur_region->read_only);	/* caller should have already checked this */
	assert(cs_addrs->hasht_tree == gv_target);	/* should have been set up by caller */
	if (0 == user_trigname_len)
	{	/* autogenerated name  -- might be long so take MIN */
		trigname_len = MIN(trigvn_len, MAX_AUTO_TRIGNAME_LEN);
		strncpy(trig_name, trigvn, trigname_len);
		val_ptr = &val;
		if (gv_target->root)
		{	/* $get(^#t("#TNAME",GVN,"#SEQNUM")) */
			BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME), trig_name, trigname_len,
				LITERAL_HASHSEQNUM, STR_LIT_LEN(LITERAL_HASHSEQNUM));
			seq_num = gvcst_get(val_ptr) ? mval2i(val_ptr) + 1 : 1;
			max_seq_num = MAX_TRIGNAME_SEQ_NUM;
			/* If dbg & white-box test then reduce limit to 1000 (instead of 1 million) auto-generated trigger names */
			if (WBTEST_ENABLED(WBTEST_MAX_TRIGNAME_SEQ_NUM))
				max_seq_num = 999;
			if (max_seq_num < seq_num)
				return TOO_MANY_TRIGGERS;
		} else
			seq_num = 1;
		uniq_ptr = unique_seq_str;
		INT2STR(seq_num, uniq_ptr);
		uniq_ptr_len = STRLEN(uniq_ptr);
		/* set ^#t("#TNAME",GVN,"#SEQNUM")++ */
		SET_TRIGGER_GLOBAL_SUB_SUB_SUB_STR(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHSEQNUM, STR_LIT_LEN(LITERAL_HASHSEQNUM), uniq_ptr, uniq_ptr_len, result);
		if (PUT_SUCCESS != result)
			return result;
		/* set ^#t("#TNAME",GVN,"#TNCOUNT")++ */
		BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHTNCOUNT, STR_LIT_LEN(LITERAL_HASHTNCOUNT));
		var_count = gvcst_get(val_ptr) ? mval2i(val_ptr) + 1 : 1;
		i2mval(&val, var_count);
		SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME), trig_name, trigname_len,
			LITERAL_HASHTNCOUNT, STR_LIT_LEN(LITERAL_HASHTNCOUNT), val, result);
		if (PUT_SUCCESS != result)
			return result;
		seq_ptr = user_trigname_str;
		memcpy(seq_ptr, trig_name, trigname_len);
		seq_ptr += trigname_len;
		*seq_ptr++ = TRIGNAME_SEQ_DELIM;
		memcpy(seq_ptr, uniq_ptr, uniq_ptr_len);
		seq_ptr += uniq_ptr_len;
		user_trigname_len = trigname_len + 1 + uniq_ptr_len;
	} else
		seq_ptr = user_trigname_str + user_trigname_len;
	ptr1 = name_and_index;
	memcpy(ptr1, trigvn, trigvn_len);
	ptr1 += trigvn_len;
	*ptr1++ = '\0';
	MV_FORCE_STR(trigger_count);
	memcpy(ptr1, trigger_count->str.addr, trigger_count->str.len);
	SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME), user_trigname_str, user_trigname_len,
		name_and_index, trigvn_len + 1 + trigger_count->str.len, result);
	if (PUT_SUCCESS != result)
		return result;
	*seq_ptr++ = TRIGNAME_SEQ_DELIM; /* all trigger names end with a hash mark, so append one */
	*seq_ptr = '\0';
	return SEQ_SUCCESS;
}

boolean_t trigger_update_rec(char *trigger_rec, uint4 len, boolean_t noprompt, uint4 *trig_stats, io_pair *trigfile_device,
		int4 *record_num)
{
	char			add_delete;
	char			ans[2];
	boolean_t		multiline_parse_fail;
	mname_entry		gvname;
	int4			max_len;
	boolean_t 		multi_line, multi_line_xecute;
	int4			rec_len;
	int4			rec_num;
	boolean_t		status;
	char			tfile_rec_val[MAX_BUFF_SIZE];
	char			trigvn[MAX_MIDENT_LEN + 1];
	char			disp_trigvn[MAX_MIDENT_LEN + SPANREG_REGION_LITLEN + MAX_RN_LEN + 1 + 1];
					/* SPANREG_REGION_LITLEN for " (region ", MAX_RN_LEN for region name,
					 * 1 for ")" and 1 for trailing '\0'.
					 */
	int			disp_trigvn_len;
	int			trigvn_len;
	char			*values[NUM_SUBS], *save_values[NUM_SUBS];
	uint4			value_len[NUM_SUBS], save_value_len[NUM_SUBS];
	stringkey		kill_trigger_hash, set_trigger_hash;
	char			tmp_str[MAX_HASH_LEN + 1];
	char			xecute_buffer[MAX_BUFF_SIZE + MAX_XECUTE_LEN], dispbuff[MAX_TRIG_DISPLEN];
	mval			trigjrec;
	char			*trigjrecptr;
	int			trigjreclen;
	io_pair			io_save_device;
	int4			max_xecute_size;
	boolean_t		no_error;
	gvnh_reg_t		*gvnh_reg;
	char			utilprefix[1024];
	int			utilprefixlen, displen;
	int			reg_index, min_reg_index, max_reg_index;
	boolean_t		first_gtmio;
	boolean_t		jnl_format_done, new_name_check_done, new_name, first_error;
	trig_stats_t		this_trig_status, overall_trig_status;
	gv_namehead		*gvt;
	gvnh_spanreg_t		*gvspan;
	hash128_state_t		set_hash_state, kill_hash_state;
	uint4			set_hash_totlen, kill_hash_totlen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* We are going to operate on the ^#t global which does not span regions. Reset gd_targ_gvnh_reg
	 * leftover from previous GV_BIND_SUBSNAME_IF_GVSPAN call to not affect any op_zprevious etc.
	 * (e.g. invocation trigger_update_rec -> trigupdrec_reg -> add_trigger_hash_entry -> op_zprevious)
	 * so it focuses on gvcst_zprevious instead of gvcst_spr_zprevious for the ^#t global.
	 * It is okay not to restore TREF(gd_targ_gvnh_reg) since it will be initialized as part of the
	 * next op_gv* call done by the caller (be it mumps or mupip).
	 */
	TREF(gd_targ_gvnh_reg) = NULL;
	assert(dollar_tlevel);
	assert(0 > memcmp(LITERAL_HASHLABEL, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHLABEL), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHCOUNT, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHCOUNT), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHCYCLE, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHCYCLE), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHTNAME, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHTNAME), STRLEN(LITERAL_MAXHASHVAL))));
	assert(0 > memcmp(LITERAL_HASHTNCOUNT, LITERAL_MAXHASHVAL, MIN(STRLEN(LITERAL_HASHTNCOUNT), STRLEN(LITERAL_MAXHASHVAL))));
	rec_num = (NULL == record_num) ? 0 : *record_num;
	gvinit();
	trigjrec.mvtype = MV_STR;
	trigjrec.str.len = len;
	trigjrec.str.addr = trigger_rec;
	if (NULL == trigfile_device)
	{	/* Check if this is a multi-line trigger. In that case, use just the first line for the below processing.
		 * The rest of the lines will later be copied over into values[XECUTE_SUB].
		 */
		trigjrecptr = memchr(trigger_rec, '\n', len);
		if (NULL != trigjrecptr)
			len = trigjrecptr - trigger_rec;
	} else
		assert(NULL == memchr(trigger_rec, '\n', len));
	if ((0 == len) || (COMMENT_LITERAL == *trigger_rec))
		return TRIG_SUCCESS;
	if (('-' != *trigger_rec) && ('+' != *trigger_rec))
	{
		trig_stats[STATS_ERROR_TRIGFILE]++;
		displen = MAX_TRIG_DISPLEN;
		format2disp(trigger_rec, len, dispbuff, &displen);	/* returns displayable string in "dispbuff" */
		util_out_print_gtmio("Error : missing +/- at start of line: !AD", FLUSH, displen, dispbuff);
		return TRIG_FAILURE;
	}
	add_delete = *trigger_rec++;
	len--;
	if ('-' == add_delete)
	{
		if ((1 == len) && ('*' == *trigger_rec))
		{
			if ((NULL == trigfile_device) && (NULL != trigjrecptr))
			{
				util_out_print_gtmio("Error : Newline not allowed in trigger name for delete operation", FLUSH);
				trig_stats[STATS_ERROR_TRIGFILE]++;
				return TRIG_FAILURE;
			}
			mustprompt = (noprompt)? FALSE : mustprompt;
			if (mustprompt)
			{
				util_out_print("This operation will delete all triggers.", FLUSH);
				promptanswer = mu_interactive("Triggers NOT deleted");
				mustprompt = FALSE;
			}
			if (FALSE == promptanswer)
				return TRIG_SUCCESS;
			trigger_delete_all(--trigger_rec, len + 1, trig_stats);	/* updates trig_stats[] appropriately */
			return TRIG_SUCCESS;
		} else if ((0 == len) || ('^' != *trigger_rec))
		{	/* if the length < 0 let trigger_delete_name respond with the error message */
			if ((NULL == trigfile_device) && (NULL != trigjrecptr))
			{
				util_out_print_gtmio("Error : Newline not allowed in trigger name for delete operation", FLUSH);
				trig_stats[STATS_ERROR_TRIGFILE]++;
				return TRIG_FAILURE;
			}
			status = trigger_delete_name(--trigger_rec, len + 1, trig_stats); /* updates trig_stats[] appropriately */
			return status;
		}
	}
	values[GVSUBS_SUB] = tfile_rec_val;	/* GVSUBS will be the first entry set so initialize it */
	max_len = (int4)SIZEOF(tfile_rec_val);
	multi_line_xecute = FALSE;
	if (!trigger_parse(trigger_rec, len, trigvn, values, value_len, &max_len, &multi_line_xecute))
	{
		trig_stats[STATS_ERROR_TRIGFILE]++;
		return TRIG_FAILURE;
	}
	trigvn_len = STRLEN(trigvn);
	set_trigger_hash.str.addr = &tmp_str[0];
	set_trigger_hash.str.len = MAX_HASH_LEN;
	build_set_cmp_str(trigvn, trigvn_len, values, value_len, &set_trigger_hash.str, multi_line_xecute);
	/* Note that we are going to compute the hash of the trigger string in bits and pieces.
	 * So use the STR_PHASH* macros (the progressive variants), not the STR_HASH macros.
	 */
	STR_PHASH_INIT(set_hash_state, set_hash_totlen);
	STR_PHASH_PROCESS(set_hash_state, set_hash_totlen, set_trigger_hash.str.addr, set_trigger_hash.str.len);
	kill_trigger_hash.str.addr = &tmp_str[0];
	kill_trigger_hash.str.len = MAX_HASH_LEN;
	build_kill_cmp_str(trigvn, trigvn_len, values, value_len, &kill_trigger_hash.str, multi_line_xecute);
	STR_PHASH_INIT(kill_hash_state, kill_hash_totlen);
	STR_PHASH_PROCESS(kill_hash_state, kill_hash_totlen, kill_trigger_hash.str.addr, kill_trigger_hash.str.len);
	first_gtmio = TRUE;
	utilprefixlen = ARRAYSIZE(utilprefix);
	if (multi_line_xecute)
	{
		if (NULL != trigfile_device)
		{
			assert(SIZEOF(xecute_buffer) == MAX_BUFF_SIZE + MAX_XECUTE_LEN);
			/* Leave MAX_BUFF_SIZE to store other (excluding XECUTE) parts of the trigger definition.
			 * This way we can copy over these once the multi-line XECUTE string is constructed and
			 * use this to write the TLGTRIG/ULGTRIG journal record in case we do "jnl_format" later.
			 */
			values[XECUTE_SUB] = &xecute_buffer[MAX_BUFF_SIZE];
			trigjreclen = trigjrec.str.len + 1;	/* 1 for newline */
			assert(trigjreclen < MAX_BUFF_SIZE);
			trigjrecptr = &xecute_buffer[MAX_BUFF_SIZE - trigjreclen];
			memcpy(trigjrecptr, trigjrec.str.addr, trigjreclen - 1);
			trigjrecptr[trigjreclen - 1] = '\n';
			trigjrec.str.addr = trigjrecptr;
			value_len[XECUTE_SUB] = 0;
			max_xecute_size = MAX_XECUTE_LEN;
			io_save_device = io_curr_device;
			io_curr_device = *trigfile_device;
			multi_line = multi_line_xecute;
			/* We are in a multi-line trigger definition. Each trigger line should now correspond to an M source line.
			 * The GT.M compiler does not accept any M source line > MAX_SRCLINE bytes. So issue error right away in
			 * case source line is > MAX_SRCLINE. No point reading the full line and then issuing the error.
			 * Note that MAX_SRCLINE also includes the newline at end of line hence the MAX_SRCLINE - 1 usage below.
			 */
			multiline_parse_fail = FALSE;
			while (multi_line)
			{
				rec_len = file_input_get(&trigger_rec, MAX_SRCLINE - 1);
				if (!io_curr_device.in->dollar.x)
					rec_num++;
				if (0 > rec_len)
				{
					assert((FILE_INPUT_GET_LINE2LONG == rec_len) || (FILE_INPUT_GET_ERROR == rec_len));
					if (FILE_INPUT_GET_ERROR == rec_len)
						break;
					do
					{	/* Read the remainder of the long line in as many MAX_SRCLINE chunks as needed */
						rec_len = file_input_get(&trigger_rec, MAX_SRCLINE - 1);
						if (!io_curr_device.in->dollar.x)
							rec_num++;
						if (0 <= rec_len)
							break;	/* reached end of line */
						assert((FILE_INPUT_GET_LINE2LONG == rec_len) || (FILE_INPUT_GET_ERROR == rec_len));
						if (FILE_INPUT_GET_ERROR == rec_len)
							break;
					} while (TRUE);
					if (FILE_INPUT_GET_ERROR == rec_len)
						break;
					if (!multiline_parse_fail)
					{
						UTIL_PRINT_PREFIX_IF_NEEDED(first_gtmio, utilprefix, &utilprefixlen);
						util_out_print_gtmio("Error : Multi-line trigger -XECUTE exceeds maximum "
									"M source line length of !UL", FLUSH, MAX_SRCLINE);
						value_len[XECUTE_SUB] = 1;
						values[XECUTE_SUB][0] = ' ';
						max_xecute_size = SIZEOF(xecute_buffer);
					}
					multiline_parse_fail = TRUE;
				}
				io_curr_device = io_save_device;	/* In case we have to write an error message */
				no_error = trigger_parse(trigger_rec, (uint4)rec_len, trigvn, values, value_len,
											&max_xecute_size, &multi_line);
				io_curr_device = *trigfile_device;
				if (!no_error)
				{	/* An error occurred (e.g. Trigger definition too long etc.).
					 * Consume remainder of multi-line trigger definition before moving on.
					 * But before that replace XECUTE string constructed till now with a dummy one.
					 */
					assert(!multi_line);
					multi_line = TRUE;
					multiline_parse_fail = TRUE;
				}
				if (multiline_parse_fail)
				{
					value_len[XECUTE_SUB] = 1;
					values[XECUTE_SUB][0] = ' ';
					max_xecute_size = SIZEOF(xecute_buffer);
				}
			}
			trigjrec.str.len = trigjreclen + value_len[XECUTE_SUB];
			if (NULL != record_num)
				*record_num = rec_num;
			if (0 > rec_len)
			{
				assert(FILE_INPUT_GET_ERROR == rec_len);
				io_curr_device = io_save_device;
				util_out_print_gtmio("Error : Multi-line trigger -XECUTE is not properly terminated", FLUSH);
				trig_stats[STATS_ERROR_TRIGFILE]++;
				return TRIG_FAILURE;
			}
			if (multiline_parse_fail)
			{	/* error message has already been issued */
				io_curr_device = io_save_device;
				trig_stats[STATS_ERROR_TRIGFILE]++;
				return TRIG_FAILURE;
			}
			io_curr_device = io_save_device;
		} else
		{
			values[XECUTE_SUB] = trigjrecptr + 1;
			value_len[XECUTE_SUB] = trigjrec.str.addr + trigjrec.str.len - (trigjrecptr + 1);
			if ('\n' != values[XECUTE_SUB][value_len[XECUTE_SUB] - 1])
			{
				util_out_print_gtmio("Error : Multi-line xecute in $ztrigger ITEM must end in newline", FLUSH);
				trig_stats[STATS_ERROR_TRIGFILE]++;
				return TRIG_FAILURE;
			}
			if (!process_xecute(values[XECUTE_SUB], &value_len[XECUTE_SUB], TRUE))
			{
				CONV_STR_AND_PRINT("Error : Parsing XECUTE string: ", value_len[XECUTE_SUB], values[XECUTE_SUB]);
				trig_stats[STATS_ERROR_TRIGFILE]++;
				return TRIG_FAILURE;
			}
			/* trigjrec is already properly set up */
		}
		STR_PHASH_PROCESS(kill_hash_state, kill_hash_totlen, values[XECUTE_SUB], value_len[XECUTE_SUB]);
		STR_PHASH_PROCESS(set_hash_state, set_hash_totlen, values[XECUTE_SUB], value_len[XECUTE_SUB]);
	} else if ((NULL == trigfile_device) && (NULL != trigjrecptr))
	{	/* If this is a not a multi-line xecute string, we dont expect newlines. The only exception is if it is
		 * the last byte in the string.
		 */
		*trigjrecptr++;
		if (trigjrecptr != (trigjrec.str.addr + trigjrec.str.len))
		{
			util_out_print_gtmio("Error : Newline allowed only inside multi-line xecute in $ztrigger ITEM", FLUSH);
			trig_stats[STATS_ERROR_TRIGFILE]++;
			return TRIG_FAILURE;
		}
	}
	STR_PHASH_RESULT(set_hash_state, set_hash_totlen, set_trigger_hash.hash_code);
	STR_PHASH_RESULT(kill_hash_state, kill_hash_totlen, kill_trigger_hash.hash_code);
	gvname.var_name.addr = trigvn;
	gvname.var_name.len = trigvn_len;
	COMPUTE_HASH_MNAME(&gvname);
	GV_BIND_NAME_ONLY(gd_header, &gvname, gvnh_reg);
	gvspan = gvnh_reg->gvspan;
	if (NULL != gvspan)
	{
		gvnh_spanreg_subs_gvt_init(gvnh_reg, gd_header, NULL);
		reg_index = gvspan->min_reg_index;
		min_reg_index = gvspan->min_reg_index;
		max_reg_index = gvspan->max_reg_index;
		assert(0 <= reg_index);
		assert(reg_index < gd_header->n_regions);
		gvt = GET_REAL_GVT(gvspan->gvt_array[reg_index - min_reg_index]);
		assert(NULL != gvt);
		gv_target = gvt;
		gv_cur_region = gd_header->regions + reg_index;
		change_reg();
		SET_DISP_TRIGVN(gv_cur_region, disp_trigvn, disp_trigvn_len, trigvn, trigvn_len);
		/* Save values[] and value_len[] arrays since they might be overwritten inside "trigupdrec_reg"
		 * but we need the unmodified values for each call to that function.
		 */
		assert(SIZEOF(value_len) == SIZEOF(save_value_len));
		memcpy(save_value_len, value_len, SIZEOF(value_len));
		assert(SIZEOF(values) == SIZEOF(save_values));
		memcpy(save_values, values, SIZEOF(values));
	} else
	{
		memcpy(disp_trigvn, trigvn, trigvn_len);
		disp_trigvn_len = trigvn_len;
		disp_trigvn[disp_trigvn_len] = '\0';	/* null terminate just in case */
	}
	jnl_format_done = FALSE;
	new_name_check_done = FALSE;
	first_error = TRUE;
	overall_trig_status = STATS_UNCHANGED_TRIGFILE;
	do
	{	/* At this point gv_cur_region/cs_addrs/gv_target already point to the correct region.
		 * For a spanning global, they point to one of the spanned regions in each iteration of the do-while loop below.
		 */
		this_trig_status = trigupdrec_reg(trigvn, trigvn_len, &jnl_format_done, &trigjrec,
						&new_name_check_done, &new_name, &values[0], &value_len[0], add_delete,
						&kill_trigger_hash, &set_trigger_hash, &disp_trigvn[0], disp_trigvn_len, trig_stats,
						&first_gtmio, utilprefix, &utilprefixlen);
		assert((STATS_UNCHANGED_TRIGFILE == this_trig_status) || (STATS_NOERROR_TRIGFILE == this_trig_status)
			|| (STATS_ERROR_TRIGFILE == this_trig_status));
		if (STATS_ERROR_TRIGFILE == this_trig_status)
		{
			if (first_error)
			{
				trig_stats[STATS_ERROR_TRIGFILE]++;
				first_error = FALSE;
			}
			overall_trig_status = STATS_ERROR_TRIGFILE;
		} else if (STATS_UNCHANGED_TRIGFILE == overall_trig_status)
			overall_trig_status = this_trig_status;
		/* else if (STATS_NOERROR_TRIGFILE == overall_trig_status) : it is already what it should be */
		/* else if (STATS_ERROR_TRIGFILE == overall_trig_status)   : it is already what it should be */
		if (NULL == gvspan)
			break;
		if (reg_index >= max_reg_index)
			break;
		do
		{
			reg_index++;
			assert(reg_index <= max_reg_index);
			assert(reg_index < gd_header->n_regions);
			gvt = GET_REAL_GVT(gvspan->gvt_array[reg_index - min_reg_index]);
			if (NULL == gvt)
			{
				assert(reg_index < max_reg_index);
				continue;
			}
			gv_target = gvt;
			gv_cur_region = gd_header->regions + reg_index;
			change_reg();
			SET_DISP_TRIGVN(gv_cur_region, disp_trigvn, disp_trigvn_len, trigvn, trigvn_len);
			/* Restore values[] and value_len[] before next call to "trigupdrec_reg" */
			assert(SIZEOF(value_len) == SIZEOF(save_value_len));
			memcpy(value_len, save_value_len, SIZEOF(save_value_len));
			assert(SIZEOF(values) == SIZEOF(save_values));
			memcpy(values, save_values, SIZEOF(save_values));
			break;
		} while (TRUE);
	} while (TRUE);
	if ((STATS_UNCHANGED_TRIGFILE == overall_trig_status) || (STATS_NOERROR_TRIGFILE == overall_trig_status))
	{
		trig_stats[overall_trig_status]++;
		return TRIG_SUCCESS;
	} else
	{
		assert(STATS_ERROR_TRIGFILE == overall_trig_status);
		return TRIG_FAILURE;
	}
}

STATICFNDEF trig_stats_t trigupdrec_reg(char *trigvn, int trigvn_len, boolean_t *jnl_format_done, mval *trigjrec,
	boolean_t *new_name_check_done, boolean_t *new_name_ptr, char **values, uint4 *value_len, char add_delete,
	stringkey *kill_trigger_hash, stringkey *set_trigger_hash, char *disp_trigvn, int disp_trigvn_len, uint4 *trig_stats,
	boolean_t *first_gtmio, char *utilprefix, int *utilprefixlen)
{
	mval			*trigname[NUM_OPRS]; /* names of matching kill and/or set trigger */
	boolean_t		new_name;
	sgmnt_addrs		*csa;
	mval			dummymval;
	boolean_t		skip_set_trigger, trigger_exists;
	mval			*trigger_count;
	boolean_t		newtrigger;
	int			set_index, kill_index, tmp_index;
	boolean_t		db_matched_kill, db_matched_set, tmp_matched_kill, tmp_matched_set;
	boolean_t		full_match, new_match;
	boolean_t		kill_cmp, set_cmp;
	boolean_t		is_set;
	int			oprtype, oprstart, oprend, set_kill_bitmask;
	char			*oprname[] = { "Non-SET", "SET", "SET and/or Non-SET"};	/* index 0 corresponds to OPR_KILL,
											 * 1 to OPR_SET,
											 * 2 if OPR_SETKILL
											 */
	char			*opname;
	int4			updates;
	uint4			trigload_status;
	int			num;
	boolean_t		result;
	int			sub_indx;
	int4			max_len;
	mval			xecute_index, xecute_size;
	int4			offset;
	char			*ptr1;
	mval			mv_hash;
	char			trig_name[MAX_USER_TRIGNAME_LEN + 2];	/* One spot for '#' delimiter and one for trailing '\0' */
	int			trig_protected_mval_push_count;

	csa = cs_addrs;
	if (NULL == csa)	/* Remote region */
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_REMOTEDBNOTRIG, 4, trigvn_len, trigvn, REG_LEN_STR(gv_cur_region));
	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	assert(cs_addrs == gv_target->gd_csa);
	DISALLOW_MULTIINST_UPDATE_IN_TP(dollar_tlevel, jnlpool_head, csa, first_sgm_info, TRUE);
	if (!*jnl_format_done && JNL_WRITE_LOGICAL_RECS(csa))
	{	/* Attach to jnlpool if replication is turned on. Normally SET or KILL of the ^#t records take care of this
		 * but in case this is a NO-OP trigger operation that wont happen and we still want to write a
		 * TLGTRIG/ULGTRIG journal record. Hence the need to do this. Also write a LGTRIG record in just one region
		 * in case this is a global spanning multiple regions.
		 */
		JNLPOOL_INIT_IF_NEEDED(csa, csa->hdr, csa->nl, SCNDDBNOUPD_CHECK_TRUE);
		assert(dollar_tlevel);
		T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_TRIGLOADFAIL);	/* needed to set update_trans TRUE on this region
									 * even if NO db updates happen to ^#t nodes. */
		jnl_format(JNL_LGTRIG, NULL, trigjrec, 0);
		*jnl_format_done = TRUE;
	}
	SET_GVTARGET_TO_HASHT_GBL(csa);
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	if (csa->hdr->hasht_upgrade_needed)
	{	/* ^#t needs to be upgraded first before reading/updating it. Cannot proceed. */
		if (0 == gv_target->root)
		{
			csa->hdr->hasht_upgrade_needed = FALSE;	/* Reset now that we know there is no ^#t global in this db.
								 * Note: It is safe to do so even if we dont hold crit.
								 */
		} else
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_NEEDTRIGUPGRD, 2, DB_LEN_STR(gv_cur_region));
	}
	if (!*new_name_check_done)
	{	/* Make sure below call is done only ONCE for a global spanning multiple regions since this call goes
		 * through all regions in the gld to figure out if a user-defined trigger name is unique.
		 */
		new_name = check_unique_trigger_name_full(values, value_len, &dummymval, &new_match,
						trigvn, trigvn_len, set_trigger_hash, kill_trigger_hash);
		*new_name_ptr = new_name;
		*new_name_check_done = TRUE;
	} else
		new_name = *new_name_ptr;
	skip_set_trigger = FALSE;
	trig_protected_mval_push_count = 0;
	/* Protect MVALs from garabage collection - NOTE: trigger_count is done last as that needs to be popped in one case below */
	INCR_AND_PUSH_MV_STENT(trigname[OPR_SET]);
	INCR_AND_PUSH_MV_STENT(trigname[OPR_KILL]);
	INCR_AND_PUSH_MV_STENT(trigger_count);
	assert(('+' == add_delete) || ('-' == add_delete));	/* Has to be + or - */
	if (gv_target->root)
	{
		BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
		if (gvcst_get(trigger_count))
		{
			trigger_exists = trigger_already_exists(trigvn, trigvn_len, values, value_len,
							set_trigger_hash, kill_trigger_hash,
							&set_index, &kill_index,
							&db_matched_set, &db_matched_kill, &full_match,
							trigname[OPR_SET], trigname[OPR_KILL]);
			newtrigger = FALSE;
		} else
		{
			newtrigger = TRUE;
			trigger_exists = FALSE;
		}
	} else
	{
		newtrigger = TRUE;
		trigger_exists = FALSE;
	}
	set_cmp = (NULL != strchr(values[CMD_SUB], 'S'));
	kill_cmp = ((NULL != strchr(values[CMD_SUB], 'K')) || (NULL != strchr(values[CMD_SUB], 'R')));
	updates = 0;
	trigload_status = STATS_UNCHANGED_TRIGFILE;
	if (trigger_exists)
	{
		if ((-1 != kill_index) && (set_index || set_cmp) && value_len[TRIGNAME_SUB])
		{	/* Cannot match two different triggers (corresponding to "kill_index" and "set_index")
			 * with the same user defined name. Note that it is possible if set_index==0 that the
			 * set type trigger does not exist yet but will be created by this call to trigger_update_rec.
			 * Treat that case too as if the separate set trigger existed.
			 */
			UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
			if (set_index)
				util_out_print_gtmio("Error : Input trigger on ^!AD with trigger name !AD"		\
				       " cannot match two different triggers named !AD and !AD at the same time",
				       FLUSH, disp_trigvn_len, disp_trigvn,
				       value_len[TRIGNAME_SUB], values[TRIGNAME_SUB],
				       trigname[OPR_KILL]->str.len, trigname[OPR_KILL]->str.addr,
				       trigname[OPR_SET]->str.len, trigname[OPR_SET]->str.addr);
			else
				util_out_print_gtmio("Error : Input trigger on ^!AD with trigger name !AD"	\
				       " cannot match a trigger named !AD and a to-be-created SET trigger"	\
				       " at the same time", FLUSH, disp_trigvn_len, disp_trigvn,
				       value_len[TRIGNAME_SUB], values[TRIGNAME_SUB],
				       trigname[OPR_KILL]->str.len, trigname[OPR_KILL]->str.addr);
			RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
		}
		assert(new_name || !new_match || full_match);
		if (!new_name && ('+' == add_delete) && !full_match)
		{
			opname = (!set_cmp ? oprname[OPR_KILL] : (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
			TRIGGER_SAME_NAME_EXISTS_ERROR(opname, disp_trigvn_len, disp_trigvn);
		}
		oprstart = (-1 != kill_index) ? OPR_KILL : (OPR_KILL + 1);
		oprend = (0 != set_index) ? (OPR_SET + 1) : OPR_SET;
		assert(NUM_OPRS == (OPR_SET + 1));
		assert(ARRAYSIZE(oprname) == (OPR_SET + 2));
		set_kill_bitmask = OPR_SETKILL;
		for (oprtype = oprstart; oprtype < oprend; oprtype++)
		{
			assert((OPR_KILL == oprtype) || (OPR_SET == oprtype));
			if (OPR_KILL == oprtype)
			{
				tmp_matched_set = FALSE;
				tmp_matched_kill = TRUE;
				tmp_index = kill_index;
				is_set = FALSE;
				if (0 != set_index)
				{	/* SET & KILL triggers are separate. This is the KILL trigger only invocation */
					assert(set_cmp);
					assert(kill_cmp);
					set_kill_bitmask = OPR_KILL;
				}
				opname = oprname[OPR_KILL];
			} else
			{
				tmp_matched_set = db_matched_set;
				tmp_matched_kill = db_matched_kill;
				tmp_index = set_index;
				is_set = TRUE;
				if (OPR_KILL == oprstart)
				{
					assert(set_cmp);
					assert(kill_cmp);
					set_kill_bitmask = OPR_SET;
				}
				opname = oprname[OPR_SET];
			}
			updates = modify_record(trigvn, trigvn_len, add_delete, tmp_index, values, value_len,
					trigger_count, tmp_matched_set, tmp_matched_kill,
					kill_trigger_hash, set_trigger_hash, set_kill_bitmask);
			if (0 > updates)
			{
				switch (updates)
				{
				case INVALID_LABEL:
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("Error : Current trigger format not compatible to update " \
					       "the trigger on ^!AD named !AD", FLUSH, disp_trigvn_len, disp_trigvn,
					       trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
				case KEY_TOO_LONG:
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("Error : ^!AD trigger - key larger than max key size", FLUSH,
						disp_trigvn_len, disp_trigvn);
					RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
				case VAL_TOO_LONG:
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("Error : ^!AD trigger - value larger than record size", FLUSH,
						disp_trigvn_len, disp_trigvn);
					RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
				case K_ZTK_CONFLICT:
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("Error : Command options !AD incompatible with trigger on " \
						"^!AD named !AD", FLUSH, value_len[CMD_SUB], values[CMD_SUB],
						disp_trigvn_len, disp_trigvn,
						trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
				case ADD_SET_NOCHNG_KILL_TRIG:
					assert(!is_set);
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("!AZ trigger on ^!AD already present in trigger named !AD" \
								" - no action taken", FLUSH, opname,
								disp_trigvn_len, disp_trigvn,
								trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					/* kill trigger is unchanged but set trigger (if present in a different trigger)
					 * needs to be processed separately.
					 */
					break;
				case ADD_SET_MODIFY_KILL_TRIG:
					assert(!is_set);
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("Modified !AZ trigger on ^!AD named !AD",
							FLUSH, opname, disp_trigvn_len, disp_trigvn,
							trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					trig_stats[STATS_MODIFIED]++;
					trigload_status = STATS_NOERROR_TRIGFILE;
					break;
				case OPTIONS_CMDS_CONFLICT:
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("Error : Specified options and commands cannot both be different" \
						" from those in trigger on ^!AD named !AD", FLUSH, disp_trigvn_len, disp_trigvn,
							trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
				case NAME_CMDS_CONFLICT:
					UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
					util_out_print_gtmio("Error : Specified name !AD different from that of trigger" \
						" on ^!AD named !AD but specified commands do not contain those in trigger",
							FLUSH, value_len[TRIGNAME_SUB], values[TRIGNAME_SUB],
							disp_trigvn_len, disp_trigvn,
							trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
				default:
					assertpro(FALSE && updates);
					break;
				}
			} else
			{
				skip_set_trigger = is_set;
				if ((updates & (ADD_UPDATE_NAME | ADD_UPDATE_CMDS | ADD_UPDATE_OPTIONS))
					|| (updates & (SUB_UPDATE_NAME | SUB_UPDATE_CMDS)))
				{
					trig_stats[STATS_MODIFIED]++;
					trigload_status = STATS_NOERROR_TRIGFILE;
					if (0 == trig_stats[STATS_ERROR_TRIGFILE])
					{
						if (-1 == kill_index)
							opname = (!set_cmp ? oprname[OPR_KILL]
									: (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
						UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
						util_out_print_gtmio("Modified !AZ trigger on ^!AD named !AD", FLUSH,
									opname, disp_trigvn_len, disp_trigvn,
									trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					}
				} else if (updates & DELETE_REC)
				{
					trig_stats[STATS_DELETED]++;
					trigload_status = STATS_NOERROR_TRIGFILE;
					if (0 == trig_stats[STATS_ERROR_TRIGFILE])
					{
						if (-1 == kill_index)
							opname = (!set_cmp ? oprname[OPR_KILL]
									: (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
						UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
						util_out_print_gtmio("Deleted !AZ trigger on ^!AD named !AD",
									FLUSH, opname, disp_trigvn_len, disp_trigvn,
									trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					}
					/* if KILL trigger deleted, search for possible new SET trigger index */
					if (!is_set && (kill_index < set_index)
						&& !(trigger_already_exists(trigvn, trigvn_len, values, value_len,
								set_trigger_hash, kill_trigger_hash, &set_index,
								&tmp_index, &db_matched_set, &db_matched_kill,
								&full_match, trigname[oprtype], trigname[oprtype])))
					{	/* SET trigger found previously is not found again */
						if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
							t_retry(cdb_sc_triggermod);
						assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
						util_out_print_gtmio("Error : Previously found SET trigger " \
							"on ^!AD, named !AD but cannot find it again",
							FLUSH, disp_trigvn_len, disp_trigvn,
							trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
						RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
					}
				} else if ('+' == add_delete)
				{
					assert(0 == updates);
					if (0 == trig_stats[STATS_ERROR_TRIGFILE])
					{
						if (-1 == kill_index)
							opname = (!set_cmp ? oprname[OPR_KILL]
									: (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
						UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
						util_out_print_gtmio("!AZ trigger on ^!AD already present " \
							"in trigger named !AD - no action taken",
							FLUSH, opname, disp_trigvn_len, disp_trigvn,
							trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
					}
				} else
				{
					assert(0 == updates);
					if (0 == trig_stats[STATS_ERROR_TRIGFILE])
					{
						if (-1 == kill_index)
							opname = (!set_cmp ? oprname[OPR_KILL]
									: (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
						UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
						if (!value_len[TRIGNAME_SUB]
							|| ((trigname[oprtype]->str.len == value_len[TRIGNAME_SUB])
								&& !memcmp(values[TRIGNAME_SUB], trigname[oprtype]->str.addr,
									value_len[TRIGNAME_SUB])))
						{	/* Trigger name matches input name or name was not specified (in which
							 * case name is considered to match). So the command specified
							 * does not exist for deletion.
							 */
							util_out_print_gtmio("!AZ trigger on ^!AD not present in trigger "	\
								"named !AD - no action taken", FLUSH, opname,
								disp_trigvn_len, disp_trigvn,
								trigname[oprtype]->str.len, trigname[oprtype]->str.addr);
						} else
						{
							util_out_print_gtmio("!AZ trigger on ^!AD matches trigger "	\
								"named !AD but not with specified name !AD "		\
								"- no action taken", FLUSH, opname,
								disp_trigvn_len, disp_trigvn,
								trigname[oprtype]->str.len, trigname[oprtype]->str.addr,
								value_len[TRIGNAME_SUB], values[TRIGNAME_SUB]);
						}
					}
				}
			}
		}
		if (0 == set_index)
		{
			if (set_cmp)
			{	/* SET was specified in the CMD list but no trigger was found, so treat this
				 * as a trigger not existing case for both '+' and '-' cases of add_delete.
				 */
				trigger_exists = FALSE;
				assert(!newtrigger);
			} else if (-1 != kill_index)
				skip_set_trigger = TRUE;
		}
	}
	if (newtrigger || !trigger_exists)
	{
		if ('-' == add_delete)
		{
			if (0 == trig_stats[STATS_ERROR_TRIGFILE])
			{
				if (newtrigger)
					opname = (!set_cmp ? oprname[OPR_KILL]
								: (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
				else if (-1 == kill_index)
					opname = (!set_cmp ? oprname[OPR_KILL]
								: (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
				else
					opname = oprname[OPR_SET];
				UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
				/* At this point SET or KILL or both triggers specified might not exist hence the "and/or" */
				util_out_print_gtmio("!AZ trigger on ^!AD does not exist - no action taken",
						     FLUSH, opname, disp_trigvn_len, disp_trigvn);
			}
			skip_set_trigger = TRUE;
		} else
		{
			if (!new_name && !new_match)
			{
				opname = (!set_cmp ? oprname[OPR_KILL] : (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
				TRIGGER_SAME_NAME_EXISTS_ERROR(opname, disp_trigvn_len, disp_trigvn);
			}
			if (newtrigger)
			{
				POP_MV_STENT();
				trig_protected_mval_push_count--;
				trigger_count = (mval *)&literal_one;
				set_index = 1;
			} else
			{
				assert(!trigger_exists);
				assert(0 == set_index);
				num = mval2i(trigger_count);
				set_index = ++num;
				i2mval(trigger_count, num);
			}
		}
	}
	/* Since a specified trigger name will grow by 1, copy it to a long enough array */
	if (((updates & ADD_UPDATE_NAME) && ('+' == add_delete)) || !skip_set_trigger)
	{
		memcpy(trig_name, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB] + 1);
		values[TRIGNAME_SUB] = trig_name;
		result = gen_trigname_sequence(trigvn, trigvn_len, trigger_count, values[TRIGNAME_SUB], value_len[TRIGNAME_SUB]);
		if (SEQ_SUCCESS != result)
		{
			if (TOO_MANY_TRIGGERS == result)
			{
				UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
				util_out_print_gtmio("Error : ^!AD trigger - Too many triggers", FLUSH,
					disp_trigvn_len, disp_trigvn);
			} else
			{
				TOO_LONG_REC_KEY_ERROR_MSG;
			}
			RETURN_AND_POP_MVALS(STATS_ERROR_TRIGFILE);
		}
	}
	if (trig_stats[STATS_ERROR_TRIGFILE])
	{
		if ('+' == add_delete)
		{
			trig_stats[STATS_ADDED]++;
			trigload_status = STATS_NOERROR_TRIGFILE;
		}
		UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
		util_out_print_gtmio("No errors processing trigger for global ^!AD", FLUSH, disp_trigvn_len, disp_trigvn);
	} else if (!skip_set_trigger)
	{
		if (!newtrigger && (-1 != kill_index))
		{	/* KILL commands were separately processed in KILL trigger. So consider only SET as being specified */
			value_len[CMD_SUB] = 1;
			values[CMD_SUB][1] = '\0';
			assert('S' == values[CMD_SUB][0]);
		}
		value_len[TRIGNAME_SUB] = STRLEN(values[TRIGNAME_SUB]);
		values[CHSET_SUB] = (gtm_utf8_mode) ? UTF8_NAME : LITERAL_M;
		value_len[CHSET_SUB] = STRLEN(values[CHSET_SUB]);
		/* set ^#t(GVN,"#LABEL") = HASHT_GBL_CURLABEL */
		SET_TRIGGER_GLOBAL_SUB_SUB_STR(trigvn, trigvn_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL),
					       HASHT_GBL_CURLABEL, STRLEN(HASHT_GBL_CURLABEL), result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		/* set ^#t(GVN,"#COUNT") = trigger_count */
		SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(trigvn, trigvn_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT),
						*trigger_count, result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		/* Assert that BHASH and LHASH are not part of NUM_SUBS calculation (confirms the -2 done in #define of NUM_SUBS) */
		assert(BHASH_SUB == NUM_SUBS);
		assert(LHASH_SUB == (NUM_SUBS + 1));
		for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
		{
			if (0 >= value_len[sub_indx])	/* subscript index length is zero (no longer used), skip it */
				continue;
			/* set ^#t(GVN,trigger_count,values[sub_indx]) = xecute string */
			SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(trigvn, trigvn_len, *trigger_count,
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
					BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(trigvn, trigvn_len, *trigger_count,
						trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index);
					SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_MVAL(trigvn, trigvn_len, *trigger_count,
						trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index,
						xecute_size, result);
					IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
					while (0 < max_len)
					{
						i2mval(&xecute_index, ++num);
						BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(trigvn, trigvn_len, *trigger_count,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index);
						offset = MIN(gv_cur_region->max_rec_size, max_len);
						/* set ^#t(GVN,trigger_count,"XECUTE",num) = xecute string[offset] */
						SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_STR(trigvn, trigvn_len, *trigger_count,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), xecute_index,
							ptr1, offset, result);
						IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
						ptr1 += offset;
						max_len -= offset;
					}
				}
			}
		}
		result = add_trigger_hash_entry(trigvn, trigvn_len, values[CMD_SUB], set_index, TRUE, kill_trigger_hash,
				set_trigger_hash);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		MV_FORCE_UMVAL(&mv_hash, kill_trigger_hash->hash_code);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, *trigger_count,
			trigger_subs[LHASH_SUB], STRLEN(trigger_subs[LHASH_SUB]), mv_hash, result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		MV_FORCE_UMVAL(&mv_hash, set_trigger_hash->hash_code);
		SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(trigvn, trigvn_len, *trigger_count, trigger_subs[BHASH_SUB],
			STRLEN(trigger_subs[BHASH_SUB]), mv_hash, result);
		IF_ERROR_THEN_TOO_LONG_ERROR_MSG_AND_RETURN_FAILURE(result);
		trigload_status = STATS_NOERROR_TRIGFILE;
		trig_stats[STATS_ADDED]++;
		UTIL_PRINT_PREFIX_IF_NEEDED(*first_gtmio, utilprefix, utilprefixlen);
		/* Recompute set_cmp and kill_cmp in case values[CMD_SUB] was modified above */
		set_cmp = (NULL != strchr(values[CMD_SUB], 'S'));
		kill_cmp = ((NULL != strchr(values[CMD_SUB], 'K')) || (NULL != strchr(values[CMD_SUB], 'R')));
		opname = (!set_cmp ? oprname[OPR_KILL] : (!kill_cmp ? oprname[OPR_SET] : oprname[OPR_SETKILL]));
		util_out_print_gtmio("Added !AZ trigger on ^!AD named !AD", FLUSH, opname, disp_trigvn_len, disp_trigvn,
				     value_len[TRIGNAME_SUB] - 1, values[TRIGNAME_SUB]); /* -1 to remove # from tail of name */
	}
	assert((STATS_UNCHANGED_TRIGFILE == trigload_status) || (STATS_NOERROR_TRIGFILE == trigload_status));
	if ((0 == trig_stats[STATS_ERROR_TRIGFILE]) && (STATS_NOERROR_TRIGFILE == trigload_status))
	{
		trigger_incr_cycle(trigvn, trigvn_len);	/* ^#t records changed in this function, increment cycle */
		csa->incr_db_trigger_cycle = TRUE; /* so that we increment csd->db_trigger_cycle at commit time */
		if (dollar_ztrigger_invoked)
		{	/* increment db_dztrigger_cycle so that next gvcst_put/gvcst_kill in this transaction, on this region,
			 * will re-read triggers. Note that the below increment happens for every record added. So, even if a
			 * single trigger file loaded multiple triggers on the same region, db_dztrigger_cycle will be incremented
			 * more than one for same transaction. This is considered okay since we only need db_dztrigger_cycle to
			 * be equal to a different value than gvt->db_dztrigger_cycle.
			 */
			csa->db_dztrigger_cycle++;
			DBGTRIGR((stderr, "trigupdrec_reg: dollar_ztrigger_invoked CSA->db_dztrigger_cycle=%d\n",
						csa->db_dztrigger_cycle));
		}
	}
	RETURN_AND_POP_MVALS(trigload_status);
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
		/* Print $ztrigger/mupip-trigger output before rolling back TP */
		TP_ZTRIGBUFF_PRINT;
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
		op_tstart(IMPLICIT_TSTART, TRUE, &ts_mv, 0); /* 0 ==> save no locals but RESTART OK */
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
			/* We expect the above function to return with either op_tcommit or a tp_restart invoked.
			 * In the case of op_tcommit, we expect dollar_tlevel to be 0 and if so we break out of the loop.
			 * In the tp_restart case, we expect a maximum of 4 tries/retries and much lesser usually.
			 * Additionally we also want to avoid an infinite loop so limit the loop to what is considered
			 * a huge iteration count and assertpro if that is reached as it suggests an out-of-design situation.
			 */
			assertpro(TPWRAP_HELPER_MAX_ATTEMPTS >= loopcnt);
		}
	} else
	{
		trigger_status = trigger_update_rec(trigger_rec, len, TRUE, trig_stats, NULL, NULL);
		assert(0 < dollar_tlevel);
	}
	return (TRIG_FAILURE == trigger_status);
}
#endif /* GTM_TRIGGER */
