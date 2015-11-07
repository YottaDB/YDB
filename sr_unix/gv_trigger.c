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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include <rtnhdr.h>		/* for rtn_tabent in gv_trigger.h */
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "error.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "targ_alloc.h"
#include "hashtab_mname.h"	/* for COMPUTE_HASH_MNAME */
#include "tp_set_sgm.h"
#include "t_begin.h"
#include "t_retry.h"
#include "tp_restart.h"
#include "gvcst_protos.h"
#include "mv_stent.h"		/* for COPY_SUBS_TO_GVCURRKEY macro & PUSH_MV_STENT macros */
#include "gvsub2str.h"		/* for COPY_SUBS_TO_GVCURRKEY macro (gvsub2str prototype) */
#include "format_targ_key.h"	/* for COPY_SUBS_TO_GVCURRKEY macro (format_targ_key prototype) */
#include "mvalconv.h"		/* for mval2i */
#include "op.h"			/* for op_tstart */
#include "toktyp.h"
#include "patcode.h"
#include "compiler.h"		/* for MAX_SRCLINE */
#include "min_max.h"
#include "stringpool.h"
#include "subscript.h"
#include "op_tcommit.h"		/* for op_tcommit prototype */
#include "cdb_sc.h"
#include "gv_trigger_protos.h"
#include "trigger_fill_xecute_buffer.h"
#include "strpiecediff.h"
#include "gtm_utf8.h"		/* for CHSET_M_STR and CHSET_UTF8_STR */
#include "trigger.h"		/* for MAX_TRIGNAME_LEN */
#include "wbox_test_init.h"
#include "have_crit.h"

#ifdef GTM_TRIGGER
GBLREF	boolean_t		is_dollar_incr;
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
GBLREF	uint4			update_trans;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			t_err;
GBLREF	int			tprestart_state;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	pid_t			process_id;
GBLREF	trans_num		local_tn;
#ifdef DEBUG
GBLREF	uint4			dollar_trestart;
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVKILLFAIL);
error_def(ERR_GVPUTFAIL);
error_def(ERR_GVZTRIGFAIL);
error_def(ERR_INDRMAXLEN);
error_def(ERR_PATMAXLEN);
error_def(ERR_TEXT);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGDEFBAD);
error_def(ERR_TRIGINVCHSET);
error_def(ERR_TRIGIS);
error_def(ERR_TRIGSUBSCRANGE);

LITREF	char	ctypetab[NUM_CHARS];
LITREF	int4	gvtr_cmd_mask[GVTR_CMDTYPES];
LITREF	mval	gvtr_cmd_mval[GVTR_CMDTYPES];
LITREF	mval	literal_cmd;
LITREF	mval	literal_delim;
LITREF	mval	literal_gvsubs;
LITREF	mval	literal_hashcount;
LITREF	mval	literal_hashcycle;
LITREF	mval	literal_hashlabel;
LITREF	mval	literal_hasht;
LITREF	mval	literal_options;
LITREF	mval	literal_pieces;
LITREF	mval	literal_xecute;
LITREF	mval	literal_trigname;
LITREF	mval	literal_chset;
LITREF	mval	literal_zdelim;
LITREF	mval	literal_zero;

#define	GVTR_PARSE_POINT	1
#define	GVTR_PARSE_LEFT		2
#define	GVTR_PARSE_RIGHT	3

#define SAVE_RTN_NAME(SAVE_RTN_NAME, SAVE_RTN_NAME_LEN, TRIGDSC)			\
{											\
	SAVE_RTN_NAME_LEN = MIN(MAX_TRIGNAME_LEN, TRIGDSC->rtn_desc.rt_name.len);	\
	assert(SAVE_RTN_NAME_LEN <= SIZEOF(SAVE_RTN_NAME));				\
	memcpy(SAVE_RTN_NAME, TRIGDSC->rtn_desc.rt_name.addr, SAVE_RTN_NAME_LEN);	\
}

#define SAVE_VAR_NAME(SAVE_VAR_NAME, SAVE_VAR_NAME_LEN, GVT)				\
{											\
	SAVE_VAR_NAME_LEN = MIN(MAX_MIDENT_LEN, GVT->gvname.var_name.len);		\
	assert(SAVE_VAR_NAME_LEN <= SIZEOF(SAVE_VAR_NAME));				\
	memcpy(SAVE_VAR_NAME, GVT->gvname.var_name.addr, SAVE_VAR_NAME_LEN);		\
}

#define	GVTR_HASHTGBL_READ_CLEANUP(do_gvtr_cleanup)					\
{											\
	/* Restore gv_target, gv_currkey & gv_altkey */					\
	gv_target = save_gvtarget;							\
	if (do_gvtr_cleanup)								\
		gvtr_free(gv_target);							\
	/* check no keysize expansion occurred inside gvcst_root_search */		\
	assert(gv_currkey->top == save_gv_currkey->top);				\
	memcpy(gv_currkey, save_gv_currkey, SIZEOF(gv_key) + save_gv_currkey->end);	\
	/* check no keysize expansion occurred inside gvcst_root_search */		\
	assert(gv_altkey->top == save_gv_altkey->top);					\
	memcpy(gv_altkey, save_gv_altkey, SIZEOF(gv_key) + save_gv_altkey->end);	\
	TREF(gv_last_subsc_null) = save_gv_last_subsc_null;				\
	TREF(gv_some_subsc_null) = save_gv_some_subsc_null;				\
}

#define	GVTR_POOL2BUDDYLIST(GVT_TRIGGER, DST_MSTR)									\
{															\
	int4	len;													\
	char	*addr;													\
	mstr	*dst_mstr;												\
															\
	dst_mstr = DST_MSTR;												\
	addr = dst_mstr->addr;												\
	len = dst_mstr->len;												\
	dst_mstr->addr = (char *)get_new_element(GVT_TRIGGER->gv_trig_list, DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE));	\
	memcpy(dst_mstr->addr, addr, len);										\
}

#define	GVTR_PROCESS_GVSUBS(PTR, END, SUBSDSC, COLON_IMBALANCE, GVT, SUBSCSTRLEN, SUBSCSTR)			\
{														\
	uint4		status, rt_name_len, save_var_name_len;							\
	gv_trigger_t	*lcl_trigdsc;										\
	char		rt_name[MAX_TRIGNAME_LEN], save_var_name[MAX_MIDENT_LEN];				\
														\
	status = gvtr_process_gvsubs(PTR, END, SUBSDSC, COLON_IMBALANCE, GVT);					\
	if (status)												\
	{													\
		lcl_trigdsc = GVT->gvt_trigger->gv_trig_array;							\
		assert(NULL != lcl_trigdsc);									\
		assert((NULL != lcl_trigdsc->rtn_desc.rt_name.addr)						\
			&& (0 != lcl_trigdsc->rtn_desc.rt_name.len));						\
		SAVE_VAR_NAME(save_var_name, save_var_name_len, GVT);						\
		SAVE_RTN_NAME(rt_name, rt_name_len, lcl_trigdsc);						\
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);								\
		if (ERR_PATMAXLEN == status) 									\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) status, 0, ERR_TRIGIS, 2, rt_name_len,		\
					rt_name);								\
		else if (ERR_TRIGSUBSCRANGE == status)								\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) status, 4, save_var_name_len, save_var_name,	\
					SUBSCSTRLEN, SUBSCSTR, ERR_TRIGIS, 2, rt_name_len, rt_name);		\
		else	/* error return from patstr */								\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);					\
	}													\
	/* End of a range (in a set of ranges) or a subscript itself.						\
	 * Either case, colon_imbalance can be safely reset.							\
	 */													\
	COLON_IMBALANCE = FALSE;										\
}

#define	KEYSUB_S2POOL_IF_NEEDED(KEYSUB_MVAL, KEYSUB, THISSUB)				\
{											\
	unsigned char		str_buff[MAX_ZWR_KEY_SZ], *str_end;			\
											\
	KEYSUB_MVAL = lvvalarray[KEYSUB];						\
	if (NULL == KEYSUB_MVAL)							\
	{										\
		PUSH_MV_STENT(MVST_MVAL);						\
		KEYSUB_MVAL = &mv_chain->mv_st_cont.mvs_mval;				\
		KEYSUB_MVAL->mvtype = 0;						\
		lvvalarray[KEYSUB] = KEYSUB_MVAL;					\
		THISSUB = keysub_start[KEYSUB];						\
		str_end = gvsub2str((unsigned char *)THISSUB, str_buff, FALSE);		\
		KEYSUB_MVAL->str.addr = (char *)str_buff;				\
		KEYSUB_MVAL->str.len = INTCAST(str_end - str_buff);			\
		KEYSUB_MVAL->mvtype = MV_STR;						\
		s2pool(&KEYSUB_MVAL->str);						\
	}										\
}

#define PUT_TRIGGER_ON_CMD_TYPE_QUEUE(trigdsc, gvt_trigger, type)						\
{	/* Add to type queue in gvt_trigger */									\
	if (NULL == (gvt_trigger)->type##_triglist)								\
	{	/* Initialize circular list of one element */							\
		(gvt_trigger)->type##_triglist = (trigdsc);							\
		(trigdsc)->next_##type = trigdsc;								\
	} else													\
	{	/* Add element to list - Note this is a simplistic add algorithm that doesn't make		\
		 * any attempt to keep elements sorted. Since triggers are supposed to fire in random		\
		 * order, that's a good thing. Adding elements A,B,C will end up with order A,C,B.		\
		 */												\
		(trigdsc)->next_##type = (gvt_trigger)->type##_triglist->next_##type;				\
		(gvt_trigger)->type##_triglist->next_##type = (trigdsc);					\
	}													\
}

#define SELECT_AND_RANDOMIZE_TRIGGER_CHAIN(gvt_trigger, trigstart, trig_list_offset, type)				\
{															\
	trigstart = (gvt_trigger)->type##_triglist;									\
	if (NULL != trigstart)												\
	{	/* Use arbitrary bit from sum of hodgepodge fields that change over time to see if should		\
		 * rotate the trigger chain by 1 or 2 entries to increase pseudo-randomization.				\
		 */													\
		gvt_trigger->type##_triglist = trigstart = trigstart->next_##type;					\
		if (0 != (0x10 & (process_id + (INTPTR_T)stringpool.free + (INTPTR_T)&trigstart))) /* BYPASSOK */	\
			/* Do a 2nd rotation.. */									\
			gvt_trigger->type##_triglist = trigstart = trigstart->next_##type;				\
	}														\
	trig_list_offset = OFFSETOF(gv_trigger_t, next_##type);								\
}

/* This error macro is used for all definition errors where the target is ^#t(GVN,<index>,<required subscript>) */
#define HASHT_GVN_DEFINITION_RETRY_OR_ERROR(INDEX,SUBSCRIPT,CSA)				\
{												\
	if (CDB_STAGNATE > t_tries)								\
	{											\
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);						\
		t_retry(cdb_sc_triggermod);							\
	} else											\
	{											\
		assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);		\
		SAVE_VAR_NAME(save_var_name, save_var_name_len, gvt);				\
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);						\
		/* format "INDEX,SUBSCRIPT" of ^#t(GVN,INDEX,SUBSCRIPT) in the error message */	\
		SET_PARAM_STRING(util_buff, util_len, INDEX, SUBSCRIPT);			\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, save_var_name_len,	\
				save_var_name, save_var_name_len, save_var_name, util_len,	\
				util_buff);							\
	}											\
}

/* This error macro is used for all definition errors where the target is ^#t(GVN,<#COUNT|#CYCLE>) */
#define HASHT_DEFINITION_RETRY_OR_ERROR(SUBSCRIPT,MOREINFO,CSA)	\
{								\
	if (CDB_STAGNATE > t_tries)				\
	{							\
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);		\
		t_retry(cdb_sc_triggermod);			\
	} else							\
	{							\
		HASHT_DEFINITION_ERROR(SUBSCRIPT,MOREINFO,CSA);	\
	}							\
}

#define HASHT_DEFINITION_ERROR(SUBSCRIPT,MOREINFO,CSA)					\
{											\
	assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);		\
	SAVE_VAR_NAME(save_var_name, save_var_name_len, gvt);				\
	GVTR_HASHTGBL_READ_CLEANUP(TRUE);						\
	rts_error_csa(CSA_ARG(CSA) VARLSTCNT(12) ERR_TRIGDEFBAD, 6, save_var_name_len,	\
			save_var_name, save_var_name_len, save_var_name,		\
			LEN_AND_LIT(SUBSCRIPT), ERR_TEXT, 2, RTS_ERROR_TEXT(MOREINFO));	\
}

/* This code is modeled around "updproc_ch" in updproc.c */
CONDITION_HANDLER(gvtr_tpwrap_ch)
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
		assert(TPRESTART_STATE_NORMAL == tprestart_state);	/* No rethrows possible */
		/* gvtr* code could be called by gvcst_put which plays with "reset_gv_target". But the only gvcst* functions
		 * called by the gvtr* code is "gvcst_get" which is guaranteed not to play with it. Therefore we should NOT
		 * be tampering with it (i.e. trying to reset it like what "trigger_item_tpwrap_ch" does) as the caller expects
		 * it to be untouched throughout the gvtr* activity.
		 */
		UNWIND(NULL, NULL);
	}
	NEXTCH;
}

STATICFNDEF void gvtr_db_tpwrap_helper(sgmnt_addrs *csa, int err_code, boolean_t root_srch_needed)
{
	enum cdb_sc		status, failure;
#	ifdef DEBUG
	gv_namehead		*save_gv_target;
#	endif

	ESTABLISH(gvtr_tpwrap_ch);
	assert(dollar_tlevel);
	assert(gv_target != csa->hasht_tree);
	assert((ERR_GVPUTFAIL == err_code) || (ERR_GVKILLFAIL == err_code) || (ERR_GVZTRIGFAIL == err_code));
	if (root_srch_needed)
	{
		assert(0 < t_tries);
		assert(ERR_GVPUTFAIL != err_code); /* for SET, gvcst_put does the root search anyways. So, don't do it here */
		ASSERT_BEGIN_OF_FRESH_TP_TRANS; /* ensures gvcst_root_search is the first thing done in the restarted transaction */
		GVCST_ROOT_SEARCH; /* any t_retry from gvcst_root_search will transfer control back to gvtr_tpwrap_ch */
		root_srch_needed = FALSE; /* just to be safe */
	}
	gvtr_db_read_hasht(csa);
	DEBUG_ONLY(save_gv_target = gv_target;)
	assert(save_gv_target == gv_target); /* ensure gv_target was not inadvertently modified by gvtr_db_read_hasht */
	/* If no triggers are defined for this global and we came in as non-TP and did op_tstart in GVTR_INIT_AND_TPWRAP_IF_NEEDED
	 * do the op_tcommit. However, do it only for t_tries = 0 for two reasons:
	 *
	 * (a) If in final retry of a non-TP set, we will be holding crit. op_tcommit releases crit which is an out-of-design
	 * situation as that could cause restarts in final retry of the non-TP set. So, complete the non-TP update as a TP update
	 * by deferring the op_tcommit until we reach GVTR_OP_TCOMMIT in gvcst_put/gvcst_kill.
	 *
	 * (b) Assume t_tries = 1 or 2 and we came in as non-TP. If we found no triggers defined and invoked op_tcommit, to resume
	 * the non-TP set, we will invoke t_begin to reset global variables which also resets t_tries. Now, with t_tries = 0,
	 * if a restart happens and we come back here (due to concurrent $ZTRIGGER and MUPIP TRIGGER) and find no triggers defined
	 * for this global, we will again invoke op_tcommit. This can happen in a spinlock thereby not letting the final retry
	 * optimistic -> pessimistic concurrency scheme kick in at all. One workaround would be to not call t_begin
	 * after doing an op_tcommit. But, this would mean examining the various global variables that t_begin resets and
	 * making sure it's okay NOT to do the t_begin. Instead, complete the non-TP update as a TP update by deferring the
	 * op_tcommit until we reach GVTR_OP_TCOMMIT in gvcst_put/gvcst_kill.
	 *
	 * Note: Not doing op_tcommit will cause this non-TP SET to be completed as a TP SET even though no triggers were defined.
	 * This would mean, a non-TP set (in a concurrent environment with trigger loads happening frequently) could end up
	 * writing a TSET record in the corresponding regions' journal file. In a non-trigger environment, this will NOT
	 * be an issue since we come here only if process' trigger cycle is different from database trigger cycle.
	 *
	 * The exception to the above case is when we are processing a ZTRIGGER command (denoted by the err_code passed
	 * in being set to ERR_GVTRIGFAIL). In that case, we will be journaling a TZTRIG record of some form and is ok
	 * even if the global does not exist. So we don't want to prematurely terminate the transaction in that case.
	 */
	if ((NULL == gv_target->gvt_trigger) && (0 == t_tries) && (ERR_GVZTRIGFAIL != err_code))
	{	/* No triggers exist for this global. TP wrap not needed any more for this transaction */
		status = op_tcommit();
		assert(cdb_sc_normal == status); /* if retry, an rts_error should have been issued and we should not reach here */
	}
	REVERT;
}

/* Helper function used by "gvtr_db_read_hasht" function */
STATICFNDEF boolean_t	gvtr_get_hasht_gblsubs(mval *subs_mval, mval *ret_mval)
{
	uint4		curend;
	boolean_t	was_null = FALSE, is_null = FALSE, is_defined;
	short int	max_key;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	is_defined = FALSE;
	curend = gv_currkey->end; /* note down gv_currkey->end before changing it so we can restore it before function returns */
	assert(KEY_DELIMITER == gv_currkey->base[curend]);
	assert(gv_target->gd_csa == cs_addrs);
	max_key = gv_cur_region->max_key_size;
	COPY_SUBS_TO_GVCURRKEY(subs_mval, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey */
	is_defined = gvcst_get(ret_mval);
	assert(!is_defined || (MV_STR & ret_mval->mvtype));	/* assert that gvcst_get() sets type of mval to MV_STR */
	gv_currkey->end = curend;	/* reset gv_currkey->end to what it was at function entry */
	gv_currkey->base[curend] = KEY_DELIMITER;    /* restore terminator for entire key so key is well-formed */
	return is_defined;
}

STATICFNDEF boolean_t	gvtr_get_hasht_gblsubs_and_index(mval *subs_mval, mval *index, mval *ret_mval)
{
	uint4		curend;
	boolean_t	was_null = FALSE, is_null = FALSE, is_defined;
	short int	max_key;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	is_defined = FALSE;
	curend = gv_currkey->end; /* note down gv_currkey->end before changing it so we can restore it before function returns */
	assert(KEY_DELIMITER == gv_currkey->base[curend]);
	assert(gv_target->gd_csa == cs_addrs);
	max_key = gv_cur_region->max_key_size;
	COPY_SUBS_TO_GVCURRKEY(subs_mval, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey */
	COPY_SUBS_TO_GVCURRKEY(index, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey */
	is_defined = gvcst_get(ret_mval);
	assert(!is_defined || (MV_STR & ret_mval->mvtype));	/* assert that gvcst_get() sets type of mval to MV_STR */
	gv_currkey->end = curend;	/* reset gv_currkey->end to what it was at function entry */
	gv_currkey->base[curend] = KEY_DELIMITER;    /* restore terminator for entire key so key is well-formed */
	return is_defined;
}

STATICFNDEF uint4	gvtr_process_range(gv_namehead *gvt, gvtr_subs_t *subsdsc, int type, char *start, char *end)
{
	mval		tmpmval;
	gv_namehead	*save_gvt;
	char		keybuff[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key		*out_key;
	uint4		len, len1, min, nelems;
	gvt_trigger_t	*gvt_trigger;
	char		*dststart, *dstptr, *srcptr, ch;
	boolean_t	ret;
	int		cmpres;

	/* This function has its input string pointing to the stringpool. It does some processing (mval2subsc etc.)
	 * and expects the input string to be untouched during all of this so it sets the global to TRUE
	 * to ensure no one else touches the stringpool in the meantime.
	 */
	DEBUG_ONLY(len = (uint4)(end - start));
	assert(IS_IN_STRINGPOOL(start, len));
	DBG_MARK_STRINGPOOL_UNUSABLE;
	if ('"' == *start)
	{	/* Remove surrounding double-quotes as well as transform TWO CONSECUTIVE intermediate double-quotes into ONE.
		 * We need to construct the transformed string. Since we cannot modify the input string and yet want to avoid
		 * malloc/frees within this routine, we use the buddy_list allocate/free functions for this purpose.
		 */
		assert('"' == *(end - 1));
		start++;
		end--;
	}
	len = UINTCAST(end - start);
	if (len || (GVTR_PARSE_POINT == type))
	{
		gvt_trigger = gvt->gvt_trigger;
		if (len == 0)
		{
			tmpmval.mvtype = MV_STR;
			tmpmval.str.addr = "";
			tmpmval.str.len = 0;
		} else
		{
			nelems = DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE);
			dststart = (char *)get_new_element(gvt_trigger->gv_trig_list, nelems);
			dstptr = dststart;
			for (srcptr = start; srcptr < end; srcptr++)
			{
				ch = *srcptr;
				if ('"' == ch)
				{	/* A double-quote in the middle of the string better be TWO consecutive double-quotes */
					assert(((srcptr + 1) < end) && ('"' == srcptr[1]));
					srcptr++;
				}
				*dstptr++ = ch;
			}
			assert((dstptr - dststart) <= len);
			tmpmval.mvtype = MV_STR;
			tmpmval.str.addr = dststart;
			tmpmval.str.len = INTCAST(dstptr - dststart);
		}
		/* switch gv_target for mval2subsc */
		save_gvt = gv_target;
		gv_target = gvt;
		out_key = (gv_key *)keybuff;
		out_key->end = 0;
		out_key->top = DBKEYSIZE(MAX_KEY_SZ);
		mval2subsc(&tmpmval, out_key);
		gv_target = save_gvt;
		if(len > 0)
		{
		/* Now that mval2subsc is done, free up the allocated dststart buffer */
			ret = free_last_n_elements(gvt_trigger->gv_trig_list, nelems);
			assert(ret);
		}
	}
	/* else it means an open range (where left or right side of range is unspecified)
	 * null subscript on the right side of a range is treated as negative infinity and
	 * on the left side of a range is treated as positive infinity.
	 */
	switch(type)
	{
		case GVTR_PARSE_POINT:
			assert(&subsdsc->gvtr_subs_type == &subsdsc->gvtr_subs_point.gvtr_subs_type);
			len = out_key->end;	/* keep trailing 0 */
			subsdsc->gvtr_subs_point.len = len;
			dststart = (char *)get_new_element(gvt_trigger->gv_trig_list, DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE));
			memcpy(dststart, out_key->base, len);
			subsdsc->gvtr_subs_point.subs_key = dststart;
			subsdsc->gvtr_subs_point.next_range = NULL;
			subsdsc->gvtr_subs_type = GVTR_SUBS_POINT;
			break;
		case GVTR_PARSE_LEFT:
			assert(&subsdsc->gvtr_subs_type == &subsdsc->gvtr_subs_range.gvtr_subs_type);
			if (len)
			{
				len = out_key->end;	/* keep trailing 0 */
				assert(len);
				dststart = (char *)get_new_element(gvt_trigger->gv_trig_list,
									DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE));
				memcpy(dststart, out_key->base, len);
				subsdsc->gvtr_subs_range.subs_key1 = dststart;
				subsdsc->gvtr_subs_range.len1 = len;
			} else
				subsdsc->gvtr_subs_range.len1 = GVTR_RANGE_OPEN_LEN;
			subsdsc->gvtr_subs_type = GVTR_SUBS_RANGE;
			break;
		case GVTR_PARSE_RIGHT:
			assert(&subsdsc->gvtr_subs_type == &subsdsc->gvtr_subs_range.gvtr_subs_type);
			assert(GVTR_SUBS_RANGE == subsdsc->gvtr_subs_type);
			if (len)
			{
				len = out_key->end;	/* keep trailing 0 */
				assert(len);
				dststart = (char *)get_new_element(gvt_trigger->gv_trig_list,
									DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE));
				memcpy(dststart, out_key->base, len);
				subsdsc->gvtr_subs_range.subs_key2 = dststart;
				subsdsc->gvtr_subs_range.len2 = len;
				len1 = subsdsc->gvtr_subs_range.len1;
				if (GVTR_RANGE_OPEN_LEN != len1)
				{	/* Since both ends of the range are not open, do range check of post-collated subscripts */
					min = MIN(len1, len);
					cmpres = memcmp(subsdsc->gvtr_subs_range.subs_key1,
								subsdsc->gvtr_subs_range.subs_key2, min);
					if ((0 < cmpres) || (0 == cmpres) && (len1 > len))
					{	/* Invalid range. Issue error */
						DBG_MARK_STRINGPOOL_USABLE;
						return ERR_TRIGSUBSCRANGE;
					}
				}
			} else
			{	/* right side of the range is open, check if left side is open too, if so reduce it to a "*" */
				if (GVTR_RANGE_OPEN_LEN == subsdsc->gvtr_subs_range.len1)
					subsdsc->gvtr_subs_type = GVTR_SUBS_STAR;
				else
					subsdsc->gvtr_subs_range.len2 = GVTR_RANGE_OPEN_LEN;
			}
			break;
		default:
			assert(FALSE);
			break;
	}
	assert(&subsdsc->gvtr_subs_range.next_range == &subsdsc->gvtr_subs_point.next_range);
	subsdsc->gvtr_subs_range.next_range = NULL;
	DBG_MARK_STRINGPOOL_USABLE;
	return 0;
}

STATICFNDEF uint4	gvtr_process_pattern(char *ptr, uint4 len, gvtr_subs_t *subsdsc, gvt_trigger_t *gvt_trigger)
{	/* subscript is a pattern */
	char		source_buffer[MAX_SRCLINE];
	mstr		instr;
	ptstr		pat_ptstr;
	uint4		status;

	assert('?' == *ptr);
	ptr++;
	len--;
	assert(0 < len);
	if (ARRAYSIZE(source_buffer) <= len)
	{
		assert(FALSE);
		return ERR_PATMAXLEN;
	}
	memcpy(source_buffer, ptr, len);
	instr.addr = source_buffer;
	instr.len = len;
	if (status = patstr(&instr, &pat_ptstr, NULL))
		return status;
	assert(pat_ptstr.len <= MAX_PATOBJ_LENGTH);
	len = pat_ptstr.len * SIZEOF(uint4);
	ptr = (char *)get_new_element(gvt_trigger->gv_trig_list, DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE));
	memcpy(ptr, pat_ptstr.buff, len);
	assert(&subsdsc->gvtr_subs_type == &subsdsc->gvtr_subs_pattern.gvtr_subs_type);
	subsdsc->gvtr_subs_type = GVTR_SUBS_PATTERN;
	subsdsc->gvtr_subs_pattern.pat_mval.mvtype = MV_STR;
	subsdsc->gvtr_subs_pattern.pat_mval.str.addr = ptr;
	subsdsc->gvtr_subs_pattern.pat_mval.str.len = len;
	subsdsc->gvtr_subs_pattern.next_range = NULL;
	return 0;
}

STATICFNDEF uint4 gvtr_process_gvsubs(char *start, char *end, gvtr_subs_t *subsdsc, boolean_t colon_imbalance, gv_namehead *gvt)
{
	uint4		status;

	if ('?' == start[0])
	{
		assert(!colon_imbalance);
		if (status = gvtr_process_pattern(start, UINTCAST(end - start), subsdsc, gvt->gvt_trigger))
			return status;
	} else if ('*' == start[0])
	{	/* subscript is a "*" */
		assert(end == start + 1);
		assert(&subsdsc->gvtr_subs_type == &subsdsc->gvtr_subs_star.gvtr_subs_type);
		subsdsc->gvtr_subs_type = GVTR_SUBS_STAR; /* allow ANY value for this subscript */
		subsdsc->gvtr_subs_star.next_range = NULL;
	} else
	{
		if (status = gvtr_process_range(gvt, subsdsc, colon_imbalance ? GVTR_PARSE_RIGHT : GVTR_PARSE_POINT, start, end))
		{
			/* As of now we expect only ERR_TRIGSUBSCRANGE (which is triggered only for GVTR_PARSE_RIGHT) */
			assert((ERR_TRIGSUBSCRANGE == status) && colon_imbalance);
			return status;
		}
	}
	return 0;
}

void	gvtr_db_read_hasht(sgmnt_addrs *csa)
{
	gv_namehead		*hasht_tree, *save_gvtarget, *gvt;
	mname_entry		gvent;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];
	gv_key			*save_gv_currkey;
	gv_key			*save_gv_altkey;
	mval			tmpmval, *ret_mval;
	boolean_t		is_defined, was_null = FALSE, is_null = FALSE, zdelim_defined, delim_defined;
	boolean_t		save_gv_last_subsc_null, save_gv_some_subsc_null;
	short int		max_key;
	int4			tmpint4, util_len;
	gvt_trigger_t		*gvt_trigger;
	uint4			trigidx, num_gv_triggers, num_pieces, len, cmdtype, index, minpiece, maxpiece;
	gv_trigger_t		*gv_trig_array, *trigdsc, *trigtop;
	uint4			currkey_end, cycle, numsubs, cursub, numlvsubs, curlvsub;
	char			ch, *ptr, *ptr_top, *ptr_start;
	boolean_t		quote_imbalance, colon_imbalance, end_of_subscript;
	uint4			paren_imbalance;
	gvtr_subs_t		*subsdsc;
	mname_entry		*lvnamedsc;
	uint4			*lvindexdsc, status;
	int			ctype;
	char			save_rtn_name[MAX_TRIGNAME_LEN], save_var_name[MAX_MIDENT_LEN];
	uint4			save_rtn_name_len, save_var_name_len;
#	ifdef DEBUG
	int			cntset, icntset, cntkill, icntkill, cntztrig, icntztrig;
	gv_trigger_t		*trigstart;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(dollar_tlevel);		/* the code below is not designed to work in non-TP */
	save_gvtarget = gv_target;
	SETUP_TRIGGER_GLOBAL;
	/* Save gv_currkey and gv_altkey */
	assert(NULL != gv_currkey);
	assert((SIZEOF(gv_key) + gv_currkey->end) <= SIZEOF(save_currkey));
	save_gv_currkey = (gv_key *)save_currkey;
	memcpy(save_gv_currkey, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);
	assert(NULL != gv_altkey);
	assert((SIZEOF(gv_key) + gv_altkey->end) <= SIZEOF(save_altkey));
	save_gv_altkey = (gv_key *)save_altkey;
	memcpy(save_gv_altkey, gv_altkey, SIZEOF(gv_key) + gv_altkey->end);
	save_gv_last_subsc_null = TREF(gv_last_subsc_null);
	save_gv_some_subsc_null = TREF(gv_some_subsc_null);
	DBGTRIGR((stderr, "gvtr_db_read_hasht: begin\n"));
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	gv_currkey->prev = 0;
	if (0 == gv_target->root)
	{	/* ^#t global does not exist. Return right away. */
		DBGTRIGR((stderr, "gvtr_db_read_hasht: no triggers\n"));
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		return;
	}
	/* ^#t global exists.
	 * Initialize gv_target->gvt_trigger from ^#t(<gbl>,...) where <gbl> is the global corresponding to save_gvtarget.
	 *
	 * In order to follow the code below, it would help to have the following ^#t global layout as an example.
	 * The following example assumes the following ^#t global layout corresponding to triggers for the ^GBL global
	 * In the following code, wherever "GBL" is mentioned in the comment, <gbl> is meant for the general case.
	 *
	 *	Input file
	 *	-----------
	 *	^GBL(acn=:,1) -zdelim="|" -pieces="2:5;4:6;8" -commands=Set,Kill,ZTKill -xecute="do ^XNAMEinGBL" -options=NOI,NOC
	 *
	 *	^#t global structure created by MUPIP TRIGGER
	 *	----------------------------------------------
	 *	^#t("GBL","#LABEL")="1"                     # Format of the ^#t global for GBL. Currently set to 1.
	 *						    # Will get bumped up in the future when the ^#t global format changes.
	 *	^#t("GBL","#CYCLE")=<32-bit-decimal-number> # incremented every time any trigger changes happen in ^GBL global
	 *	^#t("GBL","#COUNT")=1                       # indicating there is 1 trigger currently defined for ^GBL global
	 *	^#t("GBL",1,"CMD")="S,K,ZTK"
	 *	^#t("GBL",1,"GVSUBS")="acn=:,1"
	 *	^#t("GBL",1,"OPTIONS")="NOI,NOC"
	 *	^#t("GBL",1,"DELIM")=<undefined>	    # undefined in this case because -zdelim was specified
	 *	^#t("GBL",1,"ZDELIM")="|"		    # would have been undefined in case -delim was specified
	 *	^#t("GBL",1,"PIECES")="2:6;8"
	 *	^#t("GBL",1,"TRIGNAME")="GBL#1"		    # routine name trigger will have
	 *	^#t("GBL",1,"XECUTE")="do ^XNAMEinGBL"
	 *	^#t("GBL",1,"CHSET")="UTF-8"                # assuming the MUPIP TRIGGER was done with $ZCHSET of "UTF-8"
	 */
	/* -----------------------------------------------------------------------------
	 *          Read ^#t("GBL","#LABEL")
	 * -----------------------------------------------------------------------------
	 */
	/* Check value of ^#t("GBL","#LABEL"). We expect it to be 1 indicating the format of the ^#t global.
	 * If not, it is an integ error in the ^#t global so issue an appropriate error.
	 */
	gvt = save_gvtarget;	/* use smaller variable name as it is going to be used in lots of places below */
	/* First add "GBL" subscript to gv_currkey i.e. ^#t("GBL") */
	tmpmval.mvtype = MV_STR;
	tmpmval.str = gvt->gvname.var_name;	/* copy gvname from gvt */
	ret_mval = &tmpmval;
	max_key = gv_cur_region->max_key_size;
	COPY_SUBS_TO_GVCURRKEY(ret_mval, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey */
	/* At this point, gv_currkey points to ^#t("GBL") */
	/* Now check for ^#t("CIF","#LABEL") to determine what format ^#t("GBL") is stored in (if at all it exists) */
	is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_hashlabel, ret_mval);
	if (!is_defined)	/* No triggers exist for "^GBL". Return */
	{
		DBGTRIGR((stderr, "gvtr_db_read_hasht: no triggers for ^GBL\n"));
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		return;
	}
	if ((STR_LIT_LEN(HASHT_GBL_CURLABEL) != ret_mval->str.len) || MEMCMP_LIT(ret_mval->str.addr, HASHT_GBL_CURLABEL))
		HASHT_DEFINITION_RETRY_OR_ERROR("\"#LABEL\"","#LABEL field is not " HASHT_GBL_CURLABEL, csa);
	/* So we can go ahead and read other ^#t("GBL") records */
	/* -----------------------------------------------------------------------------
	 *          Now read ^#t("GBL","#CYCLE")
	 * -----------------------------------------------------------------------------
	 */
	is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_hashcycle, ret_mval);
	if (!is_defined)
		HASHT_DEFINITION_RETRY_OR_ERROR("\"#CYCLE\"","#CYCLE field is missing", csa);
	tmpint4 = mval2i(ret_mval);	/* decimal values are truncated by mval2i so we will accept a #CYCLE of 1.5 as 1 */
	if (0 >= tmpint4) /* ^#t("GBL","#CYCLE") is not a positive integer. Error out */
		HASHT_DEFINITION_ERROR("\"#CYCLE\"","#CYCLE field is negative", csa);
	cycle = (uint4)tmpint4;
	/* Check if ^#t("GBL") has previously been read from the db. If so, check if cycle is same as ^#t("GBL","#CYCLE"). */
	gvt_trigger = gvt->gvt_trigger;
	if (NULL != gvt_trigger)
	{
		DBGTRIGR((stderr, "gvtr_db_read_hasht: gvt_trigger->gv_trigger_cycle = %d \n",gvt_trigger->gv_trigger_cycle));
		if (gvt_trigger->gv_trigger_cycle == cycle)
		{	/* Since cycle is same, no need to reinitialize any triggers for "GBL". Can return safely.
			 * Pass "FALSE" to GVTR_HASHTGBL_READ_CLEANUP macro to ensure gvt->gvt_trigger is NOT freed.
			 */
			DBGTRIGR((stderr, "gvtr_db_read_hasht: trigger has been read before\n"));
			GVTR_HASHTGBL_READ_CLEANUP(FALSE);
			return;
		}
		DBGTRIGR((stderr, "gvtr_db_read_hasht: cycle mismatch, re-initialize triggers\n"));
		/* Now that we have to reinitialize triggers for "GBL", free up previously allocated structure first */
		gvtr_free(gvt);
		assert(NULL == gvt->gvt_trigger);
	}
	gvt_trigger = (gvt_trigger_t *)malloc(SIZEOF(gvt_trigger_t));
	gvt_trigger->gv_trigger_cycle = 0;
	gvt_trigger->gv_trig_array = NULL;
	gvt_trigger->gv_trig_list = NULL;
	/* Set gvt->gvt_trigger to this malloced memory (after gv_trig_array has been initialized to NULL to avoid garbage
	 * values). If we encounter an error below, we will remember to free this up the next time we are in this function
	 */
	gvt->gvt_trigger = gvt_trigger;
	/* -----------------------------------------------------------------------------
	 *          Now read ^#t("GBL","#COUNT")
	 * -----------------------------------------------------------------------------
	 */
	is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_hashcount, ret_mval);
	if (!is_defined)
		HASHT_DEFINITION_RETRY_OR_ERROR("\"#COUNT\"","#COUNT field is missing", csa);
	tmpint4 = mval2i(ret_mval);	/* decimal values are truncated by mval2i so we will accept a #COUNT of 1.5 as 1 */
	if (0 >= tmpint4) /* ^#t("GBL","#COUNT") is not a positive integer. Error out */
		HASHT_DEFINITION_ERROR("\"#COUNT\"","#COUNT field is negative", csa);
	num_gv_triggers = (uint4)tmpint4;
	gvt_trigger->num_gv_triggers = num_gv_triggers;
	/* We want a memory store for all the values that are going to be read in from the database. We dont know upfront
	 * how much memory is needed so we use a buddy list (that expands to fit the needed memory). We need to create a
	 * buddy_list with element size of 1 byte and ask for n-elements if we want n-bytes of contiguous memory.
	 * Minimum value of elemSize for buddy_list is 8 due to alignment requirements of the returned memory location.
	 * Therefore, we set elemSize to 8 bytes instead and will convert as much bytes as we need into as many
	 * 8-byte multiple segments below. We anticipate 256 bytes for each trigger so we allocate space for "num_gv_triggers"
	 * as the start of the buddy_list (to avoid repeated expansions of the buddy list in most cases).
	 */
	gvt_trigger->gv_trig_list = (buddy_list *)malloc(SIZEOF(buddy_list));
	initialize_list(gvt_trigger->gv_trig_list, GVTR_LIST_ELE_SIZE,
					DIVIDE_ROUND_UP(GV_TRIG_LIST_INIT_ALLOC * num_gv_triggers, GVTR_LIST_ELE_SIZE));
	trigdsc = (gv_trigger_t *)malloc(num_gv_triggers * SIZEOF(gv_trigger_t));
	memset(trigdsc, 0, num_gv_triggers * SIZEOF(gv_trigger_t));
	gvt_trigger->gv_trig_array = trigdsc; /* Now that everything is null-initialized, set gvt_trigger->gv_trig_array */
	gvt_trigger->gv_target = gvt;
	/* From this point onwards, we assume the integrity of the ^#t global i.e. MUPIP TRIGGER would have
	 * set ^#t fields to their appropriate value. So we dont do any more checks in case taking the wrong path.
	 */
	/* ------------------------------------------------------------------------------------
	 *          Now read ^#t("GBL",<num>,...) for <num> going from 1 to num_gv_triggers
	 * ------------------------------------------------------------------------------------
	 */
	currkey_end = gv_currkey->end; /* note down gv_currkey->end before changing it so we can restore it before next iteration */
	assert(KEY_DELIMITER == gv_currkey->base[currkey_end]);
	for (trigidx = 1; trigidx <= num_gv_triggers; trigidx++, trigdsc++)
	{	/* All records to be read in this loop are of the form ^#t("GBL",1,...) so add "1" as a subscript to gv_currkey */
		i2mval(&tmpmval, trigidx);
		assert(ret_mval == &tmpmval);
		COPY_SUBS_TO_GVCURRKEY(ret_mval, max_key, gv_currkey, was_null, is_null); /* updates gv_currkey */
		/* Read in ^#t("GBL",1,"TRIGNAME")="GBL#1" */
		is_defined =  gvtr_get_hasht_gblsubs((mval *)&literal_trigname, ret_mval);
		if (!is_defined)
			HASHT_GVN_DEFINITION_RETRY_OR_ERROR(trigidx,",\"TRIGNAME\"", csa);
		trigdsc->rtn_desc.rt_name = ret_mval->str;	/* Copy trigger name mident */
		trigdsc->gvt_trigger = gvt_trigger;		/* Save ptr to our main gvt_trigger struct for this trigger. With
								 * this and given a gv_trigger_t, we can get to the gvt_trigger_t
								 * block containing the gv_target pointer and thus get the trigger
								 * csa, region and index for loading trigger source later without
								 * having to look anything up again.
								 */
		/* Reserve extra space when triggername is placed in buddy list. This space is used by gtm_trigger() if
		 * the trigger name is not unique within the process. One or two of the chars are appended to the trigger
		 * name until an unused name is found.
		 */
		trigdsc->rtn_desc.rt_name.len += TRIGGER_NAME_RESERVED_SPACE;
		GVTR_POOL2BUDDYLIST(gvt_trigger, &trigdsc->rtn_desc.rt_name);
		trigdsc->rtn_desc.rt_name.len -= TRIGGER_NAME_RESERVED_SPACE;	/* Not using the space yet */
		/* Read in ^#t("GBL",1,"CMD")="S,K,ZK,ZTK,ZTR" */
		is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_cmd, ret_mval);
		if (!is_defined)
			HASHT_GVN_DEFINITION_RETRY_OR_ERROR(trigidx,",\"CMD\"", csa);
		/* Initialize trigdsc->cmdmask */
		ptr = ret_mval->str.addr;
		ptr_top = ptr + ret_mval->str.len;
		assert(0 == trigdsc->cmdmask);
		for (ptr_start = ptr; ptr <= ptr_top; ptr++)
		{
			if ((ptr == ptr_top) || (',' == *ptr))
			{
				len = UINTCAST(ptr - ptr_start);
				for (cmdtype = 0; cmdtype < GVTR_CMDTYPES; cmdtype++)
				{
					if ((len == gvtr_cmd_mval[cmdtype].str.len)
					    && (0 ==memcmp(ptr_start, gvtr_cmd_mval[cmdtype].str.addr, len)))
					{
						trigdsc->cmdmask |= gvtr_cmd_mask[cmdtype];
						break;
					}
				}
				assert(GVTR_CMDTYPES > cmdtype);
				ptr_start = ptr + 1;
			}
		}
		assert(0 != trigdsc->cmdmask);
		/* Read in ^#t("GBL",1,"GVSUBS")="acn=:,1" */
		is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_gvsubs, ret_mval);
		if (is_defined)
		{	/* At this point, we expect MUPIP TRIGGER to have ensured ret_mval is NOT the empty string "" */
			numsubs = 0;
			numlvsubs = 0;
			ptr_start = ret_mval->str.addr;
			ptr_top = ptr_start + ret_mval->str.len;
			quote_imbalance = FALSE;
			paren_imbalance = 0;
			end_of_subscript = FALSE;
			for (ptr = ptr_start; ptr <= ptr_top; ptr++)
			{
				if (ptr == ptr_top)
					end_of_subscript = TRUE;
				else if ('"' == (ch = *ptr))
				{
					if (!quote_imbalance)
						quote_imbalance = TRUE;	/* start of double-quoted string */
					else if (((ptr + 1) == ptr_top) || ('"' != ptr[1]))
						quote_imbalance = FALSE;
					else
						ptr++;	/* skip past nested double-quote (TWO consecutive double-quotes) */
				} else if (quote_imbalance)
					continue;
				else if ('(' == ch)
					paren_imbalance++;	/* parens can be inside pattern match alternation expressions */
				else if (')' == ch)
				{
					assert(paren_imbalance);	/* should never go negative */
					paren_imbalance--;
				} else if (paren_imbalance)
					continue;
				else if (',' == ch)
					end_of_subscript = TRUE;
				else if ('=' == ch)
					numlvsubs++;
				if (end_of_subscript)
				{	/* End of current subscript */
					assert(!quote_imbalance);
					assert(!paren_imbalance);
					numsubs++;
					end_of_subscript = FALSE;
				}
			}
			/* Initialize trigdsc->numsubs */
			assert(numsubs);
			assert(MAX_GVSUBSCRIPTS > numsubs);
			trigdsc->numsubs = numsubs;
			/* Allocate trigdsc->subsarray */
			trigdsc->subsarray = (gvtr_subs_t *)get_new_element(gvt_trigger->gv_trig_list,
							DIVIDE_ROUND_UP((numsubs * SIZEOF(gvtr_subs_t)), GVTR_LIST_ELE_SIZE));
			cursub = 0;
			subsdsc = trigdsc->subsarray;
			/* Initialize trigdsc->numlvsubs */
			assert(numlvsubs <= numsubs);
			trigdsc->numlvsubs = numlvsubs;
			if (numlvsubs)
			{
				curlvsub = 0;
				/* Allocate trigdsc->lvnamearray */
				trigdsc->lvnamearray = (mname_entry *)get_new_element(gvt_trigger->gv_trig_list,
							DIVIDE_ROUND_UP((numlvsubs * SIZEOF(mname_entry)), GVTR_LIST_ELE_SIZE));
				lvnamedsc = trigdsc->lvnamearray;
				/* Allocate trigdsc->lvindexarray */
				trigdsc->lvindexarray = (uint4 *)get_new_element(gvt_trigger->gv_trig_list,
							DIVIDE_ROUND_UP((numlvsubs * SIZEOF(uint4)), GVTR_LIST_ELE_SIZE));
				lvindexdsc = trigdsc->lvindexarray;
			}
			/* Initialize trigdsc->subsarray, trigdsc->lvindexarray & trigdsc->lvnamearray */
			quote_imbalance = FALSE;
			colon_imbalance = FALSE;
			paren_imbalance = 0;
			end_of_subscript = FALSE;
			for (ptr = ptr_start; ptr <= ptr_top; ptr++)
			{
				assert(ptr_start <= ptr);
				if (ptr == ptr_top)
					end_of_subscript = TRUE;
				else if ('"' == (ch = *ptr))
				{
					if (!quote_imbalance)
						quote_imbalance = TRUE;	/* start of double-quoted string */
					else if (((ptr + 1) == ptr_top) || ('"' != ptr[1]))
						quote_imbalance = FALSE;
					else
						ptr++;	/* skip past nested double-quote (TWO consecutive double-quotes) */
				} else if (quote_imbalance)
					continue;
				else if ('(' == ch)
					paren_imbalance++;	/* parens can be inside pattern match alternation expressions */
				else if (')' == ch)
				{
					assert(paren_imbalance);	/* should never go negative */
					paren_imbalance--;
				} else if (paren_imbalance)
					continue;
				else if (',' == ch)
					end_of_subscript = TRUE;
				else if ('=' == ch)
				{	/* a local variable name has been specified for a subscript */
					assert(curlvsub < numlvsubs);
					*lvindexdsc++ = cursub;
					len = UINTCAST(ptr - ptr_start);
					assert(len);
#					ifdef DEBUG
					/* Check validity of lvname */
					ctype = ctypetab[ptr_start[0]];
					assert((TK_PERCENT == ctype) || (TK_LOWER == ctype) || (TK_UPPER == ctype));
					for (index = 1; index < len; index++)
					{
						ctype = ctypetab[ptr_start[index]];
						assert((TK_LOWER == ctype) || (TK_UPPER == ctype) || (TK_DIGIT == ctype));
					}
#					endif
					lvnamedsc->var_name.len = len;
					lvnamedsc->var_name.addr = (char *)get_new_element(gvt_trigger->gv_trig_list,
										DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE));
					memcpy(lvnamedsc->var_name.addr, ptr_start, len);
					lvnamedsc->marked = FALSE;
					COMPUTE_HASH_MNAME(lvnamedsc);
					lvnamedsc++;
					curlvsub++;
					assert(lvnamedsc == &trigdsc->lvnamearray[curlvsub]);
					assert(lvindexdsc == &trigdsc->lvindexarray[curlvsub]);
					ptr_start = &ptr[1];
				} else if (';' == ch)
				{	/* End of current range and beginning of new range within same subscript */
					GVTR_PROCESS_GVSUBS(ptr_start, ptr, subsdsc, colon_imbalance, gvt, ret_mval->str.len,
						ret_mval->str.addr);
					/* Assert that irrespective of the type of the subscript, all of them have the
					 * "next_range" member at the same offset so it is safe for us to use any one below.
					 */
					assert(&subsdsc->gvtr_subs_range.next_range == &subsdsc->gvtr_subs_point.next_range);
					assert(&subsdsc->gvtr_subs_pattern.next_range == &subsdsc->gvtr_subs_point.next_range);
					assert(&subsdsc->gvtr_subs_star.next_range == &subsdsc->gvtr_subs_point.next_range);
					subsdsc->gvtr_subs_range.next_range =
							(gvtr_subs_t *)get_new_element(gvt_trigger->gv_trig_list,
									DIVIDE_ROUND_UP(SIZEOF(gvtr_subs_t), GVTR_LIST_ELE_SIZE));
					subsdsc = subsdsc->gvtr_subs_range.next_range;
					ptr_start = &ptr[1];
				} else if (':' == ch)
				{	/* Left side of range */
					status = gvtr_process_range(gvt, subsdsc, GVTR_PARSE_LEFT, ptr_start, ptr);
					assert(0 == status); /* ERR_TRIGSUBSCRANGE is issued only in case of GVTR_PARSE_RIGHT */
					assert(!colon_imbalance);
					colon_imbalance = TRUE;
					ptr_start = &ptr[1];
				}
				if (end_of_subscript)
				{
					assert(!quote_imbalance);
					assert(!paren_imbalance);
					GVTR_PROCESS_GVSUBS(ptr_start, ptr, subsdsc, colon_imbalance, gvt, ret_mval->str.len,
						ret_mval->str.addr);
					cursub++;
					/* We cannot do subsdsc++ because subsdsc could at this point have followed a chain of
					 * "next_range" pointers so could be different from what we had at the start of the loop.
					 */
					subsdsc = &trigdsc->subsarray[cursub];
					assert(subsdsc == &trigdsc->subsarray[cursub]);
					ptr_start = &ptr[1];
					end_of_subscript = FALSE;
				}
			}
			assert(cursub == numsubs);
			assert(!numlvsubs || (curlvsub == numlvsubs));
		}
		/* Else ^#t("GBL","#COUNT") is set to 1 but ^#t("GBL",1,"GVSUBS") is undefined.
		 * Possible if the trigger is defined for the entire ^GBL (i.e. NO subscripts)
		 * All related 5 trigdsc fields are already initialized to either 0 or NULL (by the memset above).
		 */
		/* Read in ^#t("GBL",1,"OPTIONS")="NOI,NOC"	*/
		is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_options, ret_mval);
		if (is_defined)
		{
			trigdsc->options = ret_mval->str;
			GVTR_POOL2BUDDYLIST(gvt_trigger, &trigdsc->options);
		}
		/* Read in ^#t("GBL",1,"DELIM")=<undefined>	*/
		delim_defined = gvtr_get_hasht_gblsubs((mval *)&literal_delim, ret_mval);
		if (delim_defined)
			trigdsc->is_zdelim = FALSE;
		else
		{	/* Read in ^#t("GBL",1,"ZDELIM")="|"		*/
			zdelim_defined = gvtr_get_hasht_gblsubs((mval *)&literal_zdelim, ret_mval);
			if (zdelim_defined)
			{
				assert(!delim_defined); /* DELIM and ZDELIM should NOT have been specified at same time */
				trigdsc->is_zdelim = TRUE;
				/* Initialize trigdsc->delimiter */
				trigdsc->delimiter = ret_mval->str;
			}
		}
		if (delim_defined || zdelim_defined)	/* order of || is important since latter is set only if former is FALSE */
		{
			trigdsc->delimiter = ret_mval->str;
			GVTR_POOL2BUDDYLIST(gvt_trigger, &trigdsc->delimiter);
		}
		/* Read in ^#t("GBL",1,"PIECES")="2:6;8"	*/
		is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_pieces, ret_mval);
		if (is_defined)
		{	/* First determine # of ';' separated piece-ranges */
			num_pieces = 1;	/* for last piece that does NOT have a ';' */
			ptr_start = ret_mval->str.addr;
			ptr_top = ptr_start + ret_mval->str.len;
			for (ptr = ptr_start; ptr < ptr_top; ptr++)
			{
				if (';' == *ptr)
					num_pieces++;
			}
			trigdsc->numpieces = num_pieces;
			trigdsc->piecearray = (gvtr_piece_t *)get_new_element(gvt_trigger->gv_trig_list,
							DIVIDE_ROUND_UP((num_pieces * SIZEOF(gvtr_piece_t)), GVTR_LIST_ELE_SIZE));
			index = 0;
			minpiece = 0;
			for (ptr = ptr_start; ptr <= ptr_top; ptr++)
			{
				if ((ptr == ptr_top) || (';' == *ptr))
				{
					maxpiece = (uint4)asc2i((uchar_ptr_t)ptr_start, INTCAST(ptr - ptr_start));
					if (!minpiece)
						minpiece = maxpiece;
					ptr_start = ptr + 1;
					trigdsc->piecearray[index].min = minpiece;
					trigdsc->piecearray[index].max = maxpiece;
					minpiece = 0;
					index++;
				} else if (':' == *ptr)
				{
					minpiece = (uint4)asc2i((uchar_ptr_t)ptr_start, INTCAST(ptr - ptr_start));
					ptr_start = ptr + 1;
				}
			}
			assert(index == num_pieces);
		}
		/* Read in ^#t("GBL",1,"CHSET")="UTF-8". If CHSET does not match gtm_chset issue error. */
		is_defined =  gvtr_get_hasht_gblsubs((mval *)&literal_chset, ret_mval);
		if (!is_defined)
			HASHT_GVN_DEFINITION_RETRY_OR_ERROR(trigidx,",\"CHSET\"", csa);
		if ((!gtm_utf8_mode && ((STR_LIT_LEN(CHSET_M_STR) != ret_mval->str.len)
						|| memcmp(ret_mval->str.addr, CHSET_M_STR, STR_LIT_LEN(CHSET_M_STR))))
			|| (gtm_utf8_mode && ((STR_LIT_LEN(CHSET_UTF8_STR) != ret_mval->str.len)
						|| memcmp(ret_mval->str.addr, CHSET_UTF8_STR, STR_LIT_LEN(CHSET_UTF8_STR)))))
		{	/* CHSET mismatch */
			SAVE_VAR_NAME(save_var_name, save_var_name_len, gvt);
			SAVE_RTN_NAME(save_rtn_name, save_rtn_name_len, trigdsc);
			GVTR_HASHTGBL_READ_CLEANUP(TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TRIGINVCHSET, 6, save_rtn_name_len, save_rtn_name,
				save_var_name_len, save_var_name, ret_mval->str.len, ret_mval->str.addr);
		}
		/* Defer loading xecute string until time to compile it */
		/* trigdsc->rtn_desc is already NULL-initialized as part of the memset above */
		gv_currkey->end = currkey_end;	/* remove the current <trigidx> and make way for the next <trigidx> in gv_currkey */
		gv_currkey->base[currkey_end] = KEY_DELIMITER;    /* restore terminator for entire key */
	}
	/* Now that ALL triggers for this global have been read, separate the triggers into types. There is a link for each command
	 * type in each trigger and an anchor for the list in gvt_trigger. Each list is circular for that type. Since triggers can
	 * be for more than one command type (e.g. SET, KILL, etc), each trigger can be on more than one queue. Later when we need
	 * to run the triggers of a given type, we can just pick the queue and run it.
	 */
	gv_trig_array = gvt_trigger->gv_trig_array;
	assert(NULL != gv_trig_array);
	gvt_trigger->set_triglist = gvt_trigger->kill_triglist = gvt_trigger->ztrig_triglist = NULL;
	DEBUG_ONLY(icntset = icntkill = icntztrig = 0);
	for (trigdsc = gv_trig_array, trigtop = trigdsc + num_gv_triggers; trigdsc < trigtop; trigdsc++)
	{
		if (0 != (trigdsc->cmdmask & gvtr_cmd_mask[GVTR_CMDTYPE_SET]))
		{
			PUT_TRIGGER_ON_CMD_TYPE_QUEUE(trigdsc, gvt_trigger, set);
			DEBUG_ONLY(++icntset);
		}
		if ((0 != (trigdsc->cmdmask & gvtr_cmd_mask[GVTR_CMDTYPE_KILL]))
			   || (0 != (trigdsc->cmdmask & gvtr_cmd_mask[GVTR_CMDTYPE_ZKILL]))
			   || (0 != (trigdsc->cmdmask & gvtr_cmd_mask[GVTR_CMDTYPE_ZTKILL])))
		{
			PUT_TRIGGER_ON_CMD_TYPE_QUEUE(trigdsc, gvt_trigger, kill);
			DEBUG_ONLY(++icntkill);
		}
		if (0 != (trigdsc->cmdmask & gvtr_cmd_mask[GVTR_CMDTYPE_ZTRIGGER]))
		{
			PUT_TRIGGER_ON_CMD_TYPE_QUEUE(trigdsc, gvt_trigger, ztrig);
			DEBUG_ONLY(++icntztrig);
		}
	}
	gvt_trigger->gv_trig_top = trigdsc;	/* Very top of the array */
#	ifdef DEBUG	/* Verify that the queues are well built */
	{
		cntset = cntkill = cntztrig = 0;
		trigstart = gvt_trigger->set_triglist;
		trigtop = NULL;
		for (trigdsc = trigstart; (NULL != trigdsc) && (trigdsc != trigtop); trigdsc = trigdsc->next_set)
		{
			trigtop = trigstart;	/* Stop when we get back here */
			cntset++;
		}

		trigstart = gvt_trigger->kill_triglist;
		trigtop = NULL;
		for (trigdsc = trigstart; (NULL != trigdsc) && (trigdsc != trigtop); trigdsc = trigdsc->next_kill)
		{
			trigtop = trigstart;	/* Stop when we get back here */
			cntkill++;
		}

		trigstart = gvt_trigger->ztrig_triglist;
		trigtop = NULL;
		for (trigdsc = trigstart; (NULL != trigdsc) && (trigdsc != trigtop); trigdsc = trigdsc->next_ztrig)
		{
			trigtop = trigstart;	/* Stop when we get back here */
			cntztrig++;
		}
		assert(cntset == icntset);
		assert(cntkill == icntkill);
		assert(cntztrig == cntztrig);
	}
#	endif
	GVTR_HASHTGBL_READ_CLEANUP(FALSE);	/* do NOT free gvt->gvt_trigger so pass FALSE */
	DBGTRIGR((stderr, "gvtr_db_read_hasht: gvt_trigger->gv_trigger_cycle = cycle\n"));
	/* Now that ^#t has been read, we update "cycle" to the higher value. In case this transaction restarts,
	 * we cannot be sure of the correctness of whatever we read so we need to undo the "cycle" update.
	 * We take care of this by setting "gvt_triggers_read_this_tn" to TRUE and use this in "tp_clean_up".
	 * Set gvt->trig_read_tn as well so this gvt is part of the list of gvts whose cycle gets restored in tp_clean_up.
	 * In addition, make sure this gvt is added to the gvt_tp_list. In case callers are gvcst_put or gvcst_kill, they
	 * do database operations on gvt and an accompanying tp_hist which automatically ensures this. But in case the caller
	 * is ZTRIGGER, it is possible only the ^#t global gvtarget gets added as part of the above "gvtr_get_hasht_gblsubs"
	 * calls and the triggering global does not get referenced anywhere else in the TP transaction. Since ZTRIGGER command
	 * does no db operations on the triggering global, it is possible "gvt" does not get added to the gvt_tp_list which
	 * means if a trollback/tprestart occurs we would not undo this gvt's trigger related cycles. To avoid
	 * this issue, we add this gvt to the gvt_tp_list always. The macro anyways does nothing if this gvt has already been
	 * added so we should be fine correctness and performance wise.
	 */
	gvt_trigger->gv_trigger_cycle = cycle;
	TREF(gvt_triggers_read_this_tn) = TRUE;
	gvt->trig_read_tn = local_tn;
	/* This ADD_TO_GVT_TP_LIST could potentially happen BEFORE a gvcst_search of this gvt occurred in this transaction.
	 * This means if gvt->clue.end is non-zero, gvcst_search would not get a chance to clear the first_tp_srch_status
	 * fields (which it does using the GVT_CLEAR_FIRST_TP_SRCH_STATUS macro) because gvt->read_local_tn would be set to
	 * local_tn as part of the ADD_TO_GVT_TP_LIST macro invocation. We therefore pass the second parameter indicating
	 * that first_tp_srch_status needs to be cleared too if gvt->read_local_tn gets synced to local_tn. All other callers
	 * of ADD_TO_GVT_TP_LIST (as of this writing) happen AFTER a gvcst_search of this gvt occurred in this TP transaction.
	 * Therefore this is currently the only place which uses TRUE for the second parameter.
	 */
	ADD_TO_GVT_TP_LIST(gvt, RESET_FIRST_TP_SRCH_STATUS_TRUE);
	return;
}

/* Determine if a given trigger matches the input key.
 * Expects key to be parsed into an array of pointers to the individual subscripts.
 * Also expects caller to invoke this function only if the # of subscripts in key is equal to that in the trigger.
 * Uses the function "gvsub2str" (in some cases) which in turn expects gv_target to be set appropriately.
 */
STATICFNDEF	boolean_t	gvtr_is_key_a_match(char *keysub_start[], gv_trigger_t *trigdsc, mval *lvvalarray[])
{
	uint4		numsubs, keysub;
	gvtr_subs_t	*subsdsc, *substop, *subsdsc1;
	uint4		subs_type;
	char		*thissub;
	mval		*keysub_mval;
	uint4		thissublen, len1, len2, min;
	boolean_t	thissub_matched, left_side_matched, right_side_matched;
	int		cmpres;

	numsubs = trigdsc->numsubs;
	assert(numsubs);
	subsdsc = trigdsc->subsarray;
	for (keysub = 0; keysub < numsubs; keysub++, subsdsc++)
	{
		thissub_matched = FALSE;
		/* For each key subscript, check against the trigger subscript (or chain of subscripts in case of SETs of ranges) */
		subsdsc1 = subsdsc; /* note down separately since we might need to follow chain of "next_range" within this subsc */
		do
		{
			subs_type = subsdsc1->gvtr_subs_type;
			switch(subs_type)
			{
				case GVTR_SUBS_STAR:
					/* easiest case, * matches any subscript so skip to the next subscript */
					thissub_matched = TRUE;
					break;
				case GVTR_SUBS_PATTERN:
					/* Determine match by reverse transforming subscript to string format and invoking
					 * do_pattern. Before that check if transformation has already been done. If so use
					 * that instead of redoing it. If not, do transformation and store it in M-stack
					 * (to protect it from garbage collection) and also update lvvalarray to help with
					 * preventing future recomputations of the same.
					 */
					KEYSUB_S2POOL_IF_NEEDED(keysub_mval, keysub, thissub);
					if (do_pattern(keysub_mval, &subsdsc1->gvtr_subs_pattern.pat_mval))
						thissub_matched = TRUE;
					break;
				case GVTR_SUBS_POINT:
					thissub = keysub_start[keysub];
					thissublen = UINTCAST(keysub_start[keysub + 1] - thissub);
					assert(0 < (int4)thissublen);	/* ensure it is not negative (i.e. huge positive value) */
					if ((thissublen == subsdsc1->gvtr_subs_point.len)
							&& (0 == memcmp(subsdsc1->gvtr_subs_point.subs_key, thissub, thissublen)))
						thissub_matched = TRUE;
					break;
				case GVTR_SUBS_RANGE:
					thissub = keysub_start[keysub];
					thissublen = UINTCAST(keysub_start[keysub + 1] - thissub);
					assert(0 < (int4)thissublen);	/* ensure it is not negative (i.e. huge positive value) */
					/* Check left side of range */
					len1 = subsdsc1->gvtr_subs_range.len1;
					left_side_matched = TRUE;
					if (GVTR_RANGE_OPEN_LEN != len1)
					{
						min = MIN(len1, thissublen);
						cmpres = memcmp(subsdsc1->gvtr_subs_range.subs_key1, thissub, min);
						if ((0 < cmpres) || (0 == cmpres) && (thissublen < len1))
							left_side_matched = FALSE;
					}
					/* Check right side of range */
					len2 = subsdsc1->gvtr_subs_range.len2;
					if (left_side_matched)
					{
						right_side_matched = TRUE;
						if (GVTR_RANGE_OPEN_LEN != len2)
						{
							min = MIN(len2, thissublen);
							cmpres = memcmp(subsdsc1->gvtr_subs_range.subs_key2, thissub, min);
							if ((0 > cmpres) || (0 == cmpres) && (thissublen > len2))
								right_side_matched = FALSE;
						}
						if (right_side_matched)
							thissub_matched = TRUE;
					}
					break;
				default:
					assert(FALSE);
					break;
			}
			if (thissub_matched)
				break;
			assert(&subsdsc1->gvtr_subs_range.next_range == &subsdsc1->gvtr_subs_point.next_range);
			assert(&subsdsc1->gvtr_subs_range.next_range == &subsdsc1->gvtr_subs_pattern.next_range);
			assert(&subsdsc1->gvtr_subs_range.next_range == &subsdsc1->gvtr_subs_star.next_range);
			subsdsc1 = subsdsc1->gvtr_subs_range.next_range;
			if (NULL == subsdsc1)	/* this key subscript did NOT match with trigger */
				return FALSE;
		} while (TRUE);
	}
	return TRUE;
}

void	gvtr_free(gv_namehead *gvt)
{
	gvt_trigger_t		*gvt_trigger;
	gv_trigger_t		*gv_trig_array, *trigdsc, *trigtop;
	buddy_list		*gv_trig_list;
	uint4			num_gv_triggers;

	gvt_trigger = gvt->gvt_trigger;
	if (NULL == gvt_trigger)
		return;
	gv_trig_array = gvt_trigger->gv_trig_array;
	if (NULL != gv_trig_array)
	{
		num_gv_triggers = gvt_trigger->num_gv_triggers;
		assert(0 < num_gv_triggers);
		for (trigdsc = gv_trig_array, trigtop = trigdsc + num_gv_triggers; trigdsc < trigtop; trigdsc++)
		{
			if (NULL != trigdsc->rtn_desc.rt_adr)
				gtm_trigger_cleanup(trigdsc);
		}
		free(gv_trig_array);
		gvt_trigger->gv_trig_array = NULL;
	}
	gv_trig_list = gvt_trigger->gv_trig_list;
	if (NULL != gv_trig_list) /* free up intermediate mallocs */
	{
		cleanup_list(gv_trig_list);
		free(gv_trig_list);
		gvt_trigger->gv_trig_list = NULL;
	}
	free(gvt_trigger);
	gvt->gvt_trigger = NULL;
	gvt->db_trigger_cycle = 0;	/* Force triggers to be reloaded implicitly if not explicitly done by caller */
}

/* Initializes triggers for global variable "gvt" from ^#t global. */
void	gvtr_init(gv_namehead *gvt, uint4 cycle, boolean_t tp_is_implicit, int err_code)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gv_namehead		*dir_tree;
	uint4			lcl_t_tries, save_t_tries, loopcnt;
	uint4			cycle_start;
	boolean_t		root_srch_needed;
	enum cdb_sc		failure;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(dollar_tlevel);		/* A TP wrap should have been done by the caller if needed */
	assert(!skip_dbtriggers);	/* should not come here if triggers are not supposed to be invoked */
	assert(gv_target == gvt);
	save_t_tries = t_tries; /* note down the value of t_tries at the entry to this function */
	csa = gvt->gd_csa;
	assert(NULL != csa);	/* database for this gvt better be opened at this point */
	dir_tree = csa->dir_tree;
	if (gvt == dir_tree)
		return; /* trigger initialization should return for directory tree (e.g. set^%GBLDEF is caller) */
	csd = csa->hdr;
	assert((gvt->db_trigger_cycle != cycle) || (gvt->db_dztrigger_cycle < csa->db_dztrigger_cycle));
	root_srch_needed = FALSE;
	/* Check if TP was in turn an implicit TP (e.g. created by the GVTR_INIT_AND_TPWRAP_IF_NEEDED macro). */
	if (tp_is_implicit)
	{	/* If implicit TP, we need to go through gvtr_db_tpwrap_helper since we don't want any t_retry calls to end up
		 * invoking MUM_TSTART. This way any restarts signaled during ^#t global reads will be handled internally in
		 * gvtr_init without disturbing the caller gvcst_put/gvcst_kill in any manner. But this assumes that the ^#t
		 * global is the FIRST global reference in this TP transaction. Otherwise the TP restart would have to transfer
		 * control back to wherever that global reference occurred instead of this ^#t global read. Assert that below.
		 */
		cycle_start = csa->db_trigger_cycle;
		ASSERT_BEGIN_OF_FRESH_TP_TRANS;
		lcl_t_tries = t_tries;
		t_fail_hist[lcl_t_tries] = cdb_sc_normal;
		assert(donot_INVOKE_MUMTSTART);
		for (loopcnt = 0; ; loopcnt++)
		{
			/* In case of restart in gvtr_db_read_hasht (handled by gvtr_tpwrap_ch), tp_restart will call tp_clean_up
			 * which will reset sgm_info_ptr to NULL and csa->sgm_info_ptr->tp_set_sgm_done to FALSE. Call tp_set_sgm
			 * to set sgm_info_ptr and first_sgm_info as gvcst_get relies on this being set. Also re-inits
			 * csa->db_trigger_cycle to the latest copy from csd->db_trigger_cycle which is important to detect if
			 * triggers have changed.
			 */
			tp_set_sgm();
			gvtr_db_tpwrap_helper(csa, err_code, root_srch_needed);
			root_srch_needed = FALSE;
			assert(t_tries >= lcl_t_tries);
			if (!dollar_tlevel)
			{	/* Came in as non-tp. Did op_tstart in the caller (GVTR_INIT_AND_TPWRAP_IF_NEEDED).
				 * op_tcommit was completed by gvtr_db_tpwrap_helper as no triggers were defined.
				 * Can break out of the loop.
				 */
				assert(ERR_GVZTRIGFAIL != err_code);
				assert((0 == t_tries) && (NULL == gvt->gvt_trigger));
				break;
			} else
			{
				failure = t_fail_hist[lcl_t_tries];
				if ((lcl_t_tries == t_tries) && (cdb_sc_normal == failure))
				{	/* Read of ^#t completed successfully as there is no change in t_tries.
					 * Safe to break out of the loop.
					 */
					break;
				}
				/* else we encountered a TP restart (which would have triggered a call to t_retry
				 * which in turn would have done a rts_error(TPRETRY) which would have been caught	{BYPASSOK}
				 * by gvtr_tpwrap_ch which would in turn have unwound the C-stack upto the point
				 * where the ESTABLISH is done in gvtr_tpwrap_helper and then returned from there).
				 * In this case we have to keep retrying the read until there are no tp restarts or
				 * an op_tcommit is done by gvtr_db_tpwrap_helper (in the event that no triggers
				 * are defined).
				 */
				assert(dollar_trestart);
				/* Before restarting, check if the restart is due to online rollback. If so, based on whether the
				 * rollback took the database back to a different logical state or not, we need to either issue
				 * an error OR redo the root search of the original global as online rollback related restart
				 * resets root block of all gv_targets to zero.
				 */
				assert(((cdb_sc_onln_rlbk1 != failure) && (cdb_sc_onln_rlbk2 != failure)) || !gv_target->root);
				assert((cdb_sc_onln_rlbk2 != failure) || TREF(dollar_zonlnrlbk));
				if (cdb_sc_onln_rlbk1 == failure)
				{
					root_srch_needed = (ERR_GVPUTFAIL != err_code);
				} else if (cdb_sc_onln_rlbk2 == failure)
				{
					assert(tstart_trigger_depth == gtm_trigger_depth);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_DBROLLEDBACK);
				}
				/* update lcl_t_tries to reflect the fact that a restart happened */
				lcl_t_tries = t_tries;
				t_fail_hist[lcl_t_tries] = cdb_sc_normal;
			}
			/* We expect the above function to return with either op_tcommit or a tp_restart invoked.
			 * In the case of op_tcommit, we expect dollar_tlevel to be 0 and if so we break out of the loop.
			 * In the tp_restart case, we expect a maximum of 4 tries/retries and much lesser usually.
			 * Additionally we also want to avoid an infinite loop so limit the loop to what is considered
			 * a huge iteration count and GTMASSERT if that is reached as it suggests an out-of-design situation.
			 */
			if (TPWRAP_HELPER_MAX_ATTEMPTS < loopcnt)
				GTMASSERT;
		}
		/* It is possible we have restarted one or more times. If so, it is possible for csa->db_trigger_cycle
		 * to have also been updated one or more times by t_retry() or tp_set_sgm but "cycle" would not have been
		 * refreshed. Since we snapshoted cycle into start_cycle, we can check if csa->db_trigger_cycle has changed.
		 * If it has, we update "cycle" so the correct value gets set into gvt->db_trigger_cycle below.
		 */
		if (cycle_start != csa->db_trigger_cycle)
			cycle = csa->db_trigger_cycle;
	} else
		gvtr_db_read_hasht(csa);
	/* Note that only gvt->db_trigger_cycle (and not CSA->db_trigger_cycle) should be touched here.
	 * CSA->db_trigger_cycle should be left untouched and updated only in t_end/tp_tend in crit.
	 * This way we are protected from the following situation which would arise if we incremented CSA here.
	 * Let us say a TP transaction does TWO updates SET ^X=1,^Y=1 and then goes to commit. Let us say
	 * before the ^X=1 set, csd had a cycle of 10 so that would have been copied over to X's gvt and to
	 * csa. Let us say just before the ^Y=1 set, csd cycle got incremented to 11 (due to a concurrent MUPIP
	 * TRIGGER which reloaded triggers for both ^X and ^Y). Now ^Y's gvt, csa and csd would all have a
	 * cycle of 11.  At commit time, we would check if csa and csd have the same cycle and they do so we
	 * will assume no restart needed when actually a restart is needed because our view of ^X's triggers
	 * is stale even though our view of ^Y's triggers is uptodate. Not touching csa's cycle here saves us
	 * from this situation as csa's cycle will be 10 at commit time which will be different from csd's
	 * cycle of 11 and in turn would cause a restart. This avoids us from otherwise having to go through
	 * all the gvts that participated in this transaction and comparing their cycle with csd.
	 */
	gvt->db_trigger_cycle = cycle;
	gvt->db_dztrigger_cycle = csa->db_dztrigger_cycle; /* No more trigger reads for this global until next $ZTRIGGER() */
}

/* Determines matching triggers for a given gv_currkey and uses "gtm_trigger" to invokes them with appropriate parameters.
 * Returns 0 for success and ERR_TPRETRY in case a retry is detected.
 */
int	gvtr_match_n_invoke(gtm_trigger_parms *trigparms, gvtr_invoke_parms_t *gvtr_parms)
{
	mident			gbl;
	gvtr_cmd_type_t		gvtr_cmd;
	gvt_trigger_t		*gvt_trigger;
	boolean_t		is_set_trigger, is_ztrig_trigger, ok_to_invoke_trigger;
	char			*key_ptr, *key_start, *key_end;
	char			*keysub_start[MAX_KEY_SZ + 1];
	gv_key			*save_gv_currkey;
	char			save_currkey[SIZEOF(short) + SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_trigger_t		*trigdsc, *trigstop, *trigstart;
	int			gtm_trig_status, tfxb_status, num_triggers_invoked, trigmax, trig_list_offset;
	mstr			*ztupd_mstr;
	mval			*keysub_mval;
	mval			*lvvalarray[MAX_GVSUBSCRIPTS + 1];
	mval			*ztupd_mval, dummy_mval;
	uint4			*lvindexarray;
	uint4			keysubs, keylen, numlvsubs, curlvsub, lvvalindex, cursub;
	char			*thissub;
	mval			*ret_mval;
	mval			tmpmval;
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];
	int4			util_len;
#	ifdef DEBUG
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sgm_info		*si;
	gv_namehead		*save_targ;
	int			lcl_gtm_trigger_depth;
#	endif

	DEBUG_ONLY(csa = cs_addrs);
	DEBUG_ONLY(csd = cs_data);
	DEBUG_ONLY(si = sgm_info_ptr);
	DEBUG_ONLY(save_targ = gv_target);
	assert(gv_target != csa->dir_tree);
	assert(dollar_tlevel);
	/* Initialize trigger parms that dont depend on the context of the matching trigger */
	gvtr_cmd = gvtr_parms->gvtr_cmd;
	gvt_trigger = gvtr_parms->gvt_trigger;
	trigparms->ztriggerop_new = &gvtr_cmd_mval[gvtr_cmd];
	is_set_trigger = (GVTR_CMDTYPE_SET == gvtr_cmd);
	is_ztrig_trigger = (GVTR_CMDTYPE_ZTRIGGER == gvtr_cmd);
	if (is_set_trigger)
	{
		/* Check that minimal pre-filling of trigparms has already occurred in the caller */
		assert(NULL != trigparms->ztoldval_new);
		assert(NULL != trigparms->ztvalue_new);
		assert(NULL != trigparms->ztdata_new);
		PUSH_MV_STENT(MVST_MVAL);	/* protect $ztupdate from stp_gcol */
		ztupd_mval = &mv_chain->mv_st_cont.mvs_mval;
		ztupd_mval->str.len = 0;
		ztupd_mval->mvtype = 0; /* keep mvtype set to 0 until mval is fully initialized (later below) */
		ztupd_mstr = &ztupd_mval->str;
		trigparms->ztupdate_new = ztupd_mval;
	} else
	{	/* KILL or ZTRIGGER type command */
		ztupd_mval = &dummy_mval;
		trigparms->ztupdate_new = (mval *)&literal_zero;
	}
	trigparms->lvvalarray = lvvalarray;
	trigparms->ztvalue_changed = FALSE;
	/* Parse gv_currkey into array of subscripts to facilitate trigger matching.
	 * Save contents of gv_currkey into local array as the global variable gv_currkey could change
	 * in between invocations of the "gtm_trigger" function in the for loop below. Not just that,
	 * gv_currkey could be even freed and remalloced as a bigger array (if a db with a bigger keysize
	 * gets opened) inside any of the "gtm_trigger" invocations. So we should maintain pointers only
	 * to the local copy (not the global gv_currkey) for use inside all iterations of the for loop.
	 */
	save_gv_currkey = (gv_key *)ROUND_UP2((INTPTR_T)save_currkey, SIZEOF(gv_currkey->end));
	assert(((char *)save_gv_currkey + SIZEOF(gv_key) + gv_currkey->end) < ARRAYTOP(save_currkey));
	memcpy(save_gv_currkey, gv_currkey, OFFSETOF(gv_key, base[0]) + gv_currkey->end + 1); /* + 1 for second null terminator */
	key_ptr = (char *)save_gv_currkey->base;
	DEBUG_ONLY(key_start = key_ptr;)
	key_end = key_ptr + save_gv_currkey->end;
	assert(KEY_DELIMITER == *key_end);
	assert(KEY_DELIMITER == *(key_end - 1));
	keysubs = 0;
	keysub_start[0] = key_ptr;
	for ( ; key_ptr < key_end; key_ptr++)
	{
		if (KEY_DELIMITER == *key_ptr)
		{
			assert(ARRAYSIZE(keysub_start) > keysubs);
			assert(keysubs < ARRAYSIZE(lvvalarray));
			lvvalarray[keysubs] = NULL;
			keysub_start[keysubs++] = key_ptr + 1;
		}
	}
	assert(keysubs);
	keysubs--;	/* do not count global name as subscript */
	assert(keysub_start[keysubs] == key_end);
	assert(NULL == lvvalarray[keysubs]);
	DEBUG_ONLY(keylen = INTCAST(key_end - key_start));
	assert(!keysubs || keylen);
	/* Match & Invoke triggers. Take care to ensure they are invoked in an UNPREDICTABLE order.
	 * Current implementation is to invoke triggers in a rotating order. For example, each command
	 * type is in a circular queue - say A, B, C. Each time we come here to drive triggers for this
	 * node, bump the gvt_trigger list pointer to the next element in the queue we are processing. So
	 * if triggers A, B, C are on the SET queue, first time we will process A,B,C, next time B,C,A,
	 * and the third, C,A,B.
	 */
	num_triggers_invoked = 0;
	if (is_set_trigger)
	{	/* Chose the set command list to process */
		SELECT_AND_RANDOMIZE_TRIGGER_CHAIN(gvt_trigger, trigstart, trig_list_offset, set);
	} else if (is_ztrig_trigger)
	{	/* Chose the ztrig command list to process */
		SELECT_AND_RANDOMIZE_TRIGGER_CHAIN(gvt_trigger, trigstart, trig_list_offset, ztrig);

	} else
	{	/* Chose the kill command list to process */
		SELECT_AND_RANDOMIZE_TRIGGER_CHAIN(gvt_trigger, trigstart, trig_list_offset, kill);
	}

	trigmax = gvt_trigger->num_gv_triggers;
	trigstop = NULL;			/* So we can get through the first iteration */
	for (trigdsc = trigstart;
	     (NULL != trigdsc) && (trigdsc != trigstop);
	     --trigmax, trigdsc = *(gv_trigger_t **)((char *)trigdsc + trig_list_offset))	/* Follow the designated list */
	{
		DBGTRIGR((stderr, "gvtr_match_n_invoke: top of trigr scan loop\n"));
		trigstop = trigstart;		/* Stop when we get back to where we started */
		if (0 > trigmax)
			GTMASSERT;		/* Loop ender "just in case" */
		assert(trigdsc >= gvt_trigger->gv_trig_array);
		assert(trigdsc < gvt_trigger->gv_trig_top);
		assert((trigdsc->cmdmask & gvtr_cmd_mask[gvtr_cmd]) || !is_set_trigger);
		if (!is_set_trigger && !is_ztrig_trigger && !(trigdsc->cmdmask & gvtr_cmd_mask[gvtr_cmd]))
			continue; /* Trigger is for different command. Currently only possible for KILL/ZKILL (asserted above) */
		/* Check that global variables which could have been modified inside gvcst_put/gvcst_kill have been
		 * reset to their default values before going into trigger code as that could cause a nested call to
		 * gvcst_put/gvcst_kill and we dont want any non-default value of this global variable from the parent
		 * gvcst_put/gvcst_kill to be reset by the nested invocation.
		 */
		assert(INVALID_GV_TARGET == reset_gv_target);
		if ((keysubs == trigdsc->numsubs) && (!keysubs || gvtr_is_key_a_match(keysub_start, trigdsc, lvvalarray)))
		{
			/* Note: lvvalarray could be updated above in case any trigger patterns
			 * needed to have been checked for a key-match. Before invoking the trigger,
			 * check if it specified any local variables for subscripts. If so,
			 * initialize any that are not yet already done.
			 */
			numlvsubs = trigdsc->numlvsubs;
			if (numlvsubs)
			{
				lvindexarray = trigdsc->lvindexarray;
				for (curlvsub = 0; curlvsub < numlvsubs; curlvsub++)
				{
					lvvalindex = lvindexarray[curlvsub];
					assert(lvvalindex < keysubs);
					/* lvval not already computed. Do so by reverse transforming subscript to
					 * string format. Store mval containing string in M-stack (to protect it
					 * from garbage collection). Also update lvvalarray to prevent future
					 * recomputations of the same across the many triggers to be checked for
					 * the given key.
					 */
					keysub_mval = lvvalarray[lvvalindex];
					KEYSUB_S2POOL_IF_NEEDED(keysub_mval, lvvalindex, thissub);
				}
			}
			/* Check if trigger specifies a piece delimiter and pieces of interest.
			 * If so, check if any of those pieces are different. If not, trigger should NOT be invoked.
			 */
			ok_to_invoke_trigger = TRUE;
			if (is_set_trigger)
			{
				assert(0 == ztupd_mval->mvtype);
				if (trigdsc->delimiter.len)
				{
					strpiecediff(&trigparms->ztoldval_new->str, &trigparms->ztvalue_new->str,
						&trigdsc->delimiter, trigdsc->numpieces, trigdsc->piecearray,
						!trigdsc->is_zdelim && gtm_utf8_mode, ztupd_mstr);
					if (!ztupd_mstr->len)
					{	/* No pieces of interest changed. So dont invoke trigger. */
						DBGTRIGR((stderr, "gvtr_match_n_invoke: Turning off ok_to_invoke_trigger #1\n"));
						ok_to_invoke_trigger = FALSE;
					} else
					{	/* The string containing list of updated pieces is pointing
						 * to a buffer allocated inside "strpiecediff". Since this
						 * function could be called again by a nested trigger, move
						 * this buffer to the stringpool.
						 */
						s2pool(ztupd_mstr);
						ztupd_mval->mvtype = MV_STR;
							/* now ztupd_mval->str is protected from stp_gcol */
					}
				}
			}
			if (ok_to_invoke_trigger)
			{
				DBGTRIGR((stderr, "gvtr_match_n_invoke: Inside trigger drive block\n"));
				DEBUG_ONLY(lcl_gtm_trigger_depth = gtm_trigger_depth);
				/* To exercise the trigger load code, for a debug build, we often load the trigger source code even
				 * when not needed (based on whether a random bit in an address is on or not). Trigger code is only
				 * needed when the trigger has not yet been compiled (the routine address is NULL). For a pro
				 * build, trigger source is only loaded when needed. Usually there will be no source here unless a
				 * given trigger is nesting unpleasantly so is still loaded from an ealier invocation on the
				 * stack. It's not strictly illegal but probably leads to a trigger depth error later.
				 */
				if ((0 == trigdsc->xecute_str.str.len) && ((NULL == trigdsc->rtn_desc.rt_adr)
									   DEBUG_ONLY(|| (0 != ((INTPTR_T)trigdsc & 0x100)))))

				{	/* Trigger xecute string not compiled yet, so load it */
					/* Read in ^#t("GBL",1,"XECUTE")="do ^XNAMEinGBL"	*/
					DBGTRIGR((stderr, "gvtr_match_n_invoke: Fetching trigger source\n"));
					tfxb_status = trigger_fill_xecute_buffer(trigdsc);
					if (0 != tfxb_status)
					{
						assert(ERR_TPRETRY == tfxb_status);
						ztupd_mval->mvtype = 0;	/* so stp_gcol - if invoked somehow - can free up any space
									 * currently occupied by this no-longer-necessary mval */
						gvtr_parms->num_triggers_invoked = num_triggers_invoked;
						return tfxb_status;
					}
					assert(NULL != trigdsc->xecute_str.str.addr);
					if (MAX_XECUTE_LEN <= trigdsc->xecute_str.str.len)
					{
						assert(FALSE);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_XECUTE_LEN);
					}
				}
				gtm_trig_status = gtm_trigger(trigdsc, trigparms);
				/* note: the above call may update trigparms->ztvalue_new for SET type triggers */
				if (NULL != trigdsc->xecute_str.str.addr)
				{	/* Now that gtm_trigger() has compiled the xecute string, the source  can be freed
					 * if it still allocated. Note a $TEXT() call or other source reference could have
					 * stolen the buffer so it may no longer be here for us to free.
					 */
					if (0 < trigdsc->xecute_str.str.len)
					{
						free(trigdsc->xecute_str.str.addr);
						trigdsc->xecute_str.str.addr = NULL;
						trigdsc->xecute_str.str.len = 0;
					}
				}
				assert(lcl_gtm_trigger_depth == gtm_trigger_depth);
				num_triggers_invoked++;
				ztupd_mval->mvtype = 0;	/* so stp_gcol -if invoked somehow - can free up any space
							 * currently occupied by this no-longer-necessary mval */
				assert((0 == gtm_trig_status) || (ERR_TPRETRY == gtm_trig_status));
				if (0 != gtm_trig_status)
				{
					gvtr_parms->num_triggers_invoked = num_triggers_invoked;
					return gtm_trig_status;
				}
			}
			/* At this time, gv_cur_region, cs_addrs, gv_target, gv_currkey could all be
			 * different from whatever they were JUST before the gtm_trigger() call. All
			 * of them would be restored to what they were after the gtm_trigger_fini call.
			 * It is ok to be in this out-of-sync situation since we dont rely on these
			 * global variables until the "gtm_trigger_fini" call. Any interrupt that
			 * happens before then will anyways know to save/restore those variables in
			 * this that are pertinent in its processing.
			 */
		}
	}
	assert(INVALID_GV_TARGET == reset_gv_target);
	assert(0 <= num_triggers_invoked);
	if (num_triggers_invoked)
		gtm_trigger_fini(FALSE, FALSE);
	/* Verify that gtm_trigger_fini restored gv_cur_region/cs_addrs/cs_data/gv_target/gv_currkey
	 * properly (gtm_trigger could have changed these values depending on the M code that was invoked).
	 */
	assert(csa == cs_addrs);
	assert(csd == cs_data);
	assert(si == sgm_info_ptr);
	assert(gv_target == save_targ);
	assert(0 == memcmp(save_gv_currkey, gv_currkey, OFFSETOF(gv_key, base[0]) + gv_currkey->end));
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	gvtr_parms->num_triggers_invoked = num_triggers_invoked;
	return 0;
}
#endif /* GTM_TRIGGER */
