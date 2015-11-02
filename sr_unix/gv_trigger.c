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

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "rtnhdr.h"		/* for rtn_tabent in gv_trigger.h */
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
#include "hashtab.h"		/* for STR_HASH in COMPUTE_HASH_MNAME */
#include "hashtab_mname.h"	/* for COMPUTE_HASH_MNAME */
#include "tp_set_sgm.h"
#include "t_begin.h"
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
#include "strpiecediff.h"
#include "gtm_utf8.h"		/* for CHSET_M_STR and CHSET_UTF8_STR */

#ifdef GTM_TRIGGER

GBLREF	boolean_t		implicit_tstart;	/* see gbldefs.c for comment */
GBLREF	boolean_t		is_dollar_incr;
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	int4			gtm_trigger_depth;
GBLREF	uint4			update_trans;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	short			dollar_tlevel;
GBLREF	uint4			t_err;
GBLREF	int			tprestart_state;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

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

error_def(ERR_GVGETFAIL);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGINVCHSET);

#define	GVTR_PARSE_POINT	1
#define	GVTR_PARSE_LEFT		2
#define	GVTR_PARSE_RIGHT	3

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

#define	GVTR_PROCESS_GVSUBS(PTR, END, SUBSDSC, COLON_IMBALANCE, GVT)			\
{											\
	uint4	status;									\
											\
	status = gvtr_process_gvsubs(PTR, END, SUBSDSC, COLON_IMBALANCE, GVT);		\
	if (status)									\
	{										\
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);					\
		/* TODO : Issue error return from patstr or ARRAYSIZE check */		\
		rts_error(VARLSTCNT(1) status);						\
	}										\
	/* End of a range (in a set of ranges) or a subscript itself.			\
	 * Either case, colon_imbalance can be safely reset.				\
	 */										\
	COLON_IMBALANCE = FALSE;							\
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

STATICFNDEF void	gvtr_db_tpwrap(sgmnt_addrs *csa, boolean_t tp_is_implicit)
{
	mval		ts_mv;
	unsigned int	lcl_t_tries;

	assert((!dollar_tlevel && !tp_is_implicit) || (dollar_tlevel && tp_is_implicit));
	if (!dollar_tlevel)
	{	/* We are not in TP. But we want to read a consistent copy of ALL triggers for this global as of this point.
		 * Therefore we need to wrap the read inside a TP transaction. But if we are in the final retry of the
		 * non-TP transaction, then we already hold crit and therefore do not need the TP wrap. Not doing so also
		 * saves us from having to change op_tstart which currently unconditionally resets t_tries to 0 assuming it
		 * will never be invoked in the final retry when dollar_tlevel is 0.
		 */
		if (CDB_STAGNATE > t_tries)
		{
			ts_mv.mvtype = MV_STR;
			ts_mv.str.len = 0;
			ts_mv.str.addr = NULL;
			implicit_tstart = TRUE;
			op_tstart(TRUE, TRUE, &ts_mv, 0);	/* 0 ==> save no locals but RESTART OK */
			assert(FALSE == implicit_tstart);	/* should have been reset by op_tstart at very beginning */
		}
	} else
	{
		lcl_t_tries = t_tries;
		t_fail_hist[lcl_t_tries] = cdb_sc_normal;
	}
	do
	{
		if (dollar_tlevel)
			tp_set_sgm();	/* set sgm_info_ptr & first_sgm_info for TP start as well as restart (hence inside loop) */
		gvtr_db_tpwrap_helper(csa, tp_is_implicit);
		if (tp_is_implicit)
		{
			assert(t_tries >= lcl_t_tries);
			if ((lcl_t_tries == t_tries) && (cdb_sc_normal == t_fail_hist[lcl_t_tries]))
				break;
			lcl_t_tries = t_tries;
			t_fail_hist[lcl_t_tries] = cdb_sc_normal;
		} else if (!dollar_tlevel)
		{	/* If dollar_tlevel is still non-zero, this means we encountered a TP restart (which would have
			 * triggered a call to t_retry which in turn would have done a rts_error(TPRETRY) which would
			 * have been caught by gvtr_tpwrap_ch which would in turn have unwound the C-stack upto the point
			 * where the ESTABLISH is done in gvtr_tpwrap_helper and then returned from there.
			 */
			break;
		}
	} while (TRUE);
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
		rc = tp_restart(1);
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

STATICFNDEF void	gvtr_db_tpwrap_helper(sgmnt_addrs *csa, boolean_t tp_is_implicit)
{
	enum cdb_sc		status;

	ESTABLISH(gvtr_tpwrap_ch);
	gvtr_db_read_hasht(csa);
	/* Do not do op_tcommit in TWO cases
	 * 	a) If we were already in a TP transaction (i.e. "gvtr_db_tpwrap" did not do the op_tstart).
	 * 	b) If we came into "gvtr_db_tpwrap" as a non-TP transaction but in the final retry.
	 * 		In this case we would not have done any implicit tstart so should not do any commit either.
	 */
	if (!tp_is_implicit && dollar_tlevel)
	{
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

STATICFNDEF void	gvtr_process_range(gv_namehead *gvt, gvtr_subs_t *subsdsc, int type, char *start, char *end)
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
	if (len)
	{
		gvt_trigger = gvt->gvt_trigger;
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
		/* switch gv_target for mval2subsc */
		save_gvt = gv_target;
		gv_target = gvt;
		out_key = (gv_key *)&keybuff[0];
		out_key->end = 0;
		out_key->top = DBKEYSIZE(MAX_KEY_SZ);
		mval2subsc(&tmpmval, out_key);
		gv_target = save_gvt;
		/* Now that mval2subsc is done, free up the allocated dststart buffer */
		ret = free_last_n_elements(gvt_trigger->gv_trig_list, nelems);
		assert(ret);
	}
	/* else len == 0 means an open range (where left or right side of range is unspecified) */
	switch(type)
	{
		case GVTR_PARSE_POINT:
			assert(len);
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
						assert(FALSE);
						/* TODO : Issue error */
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
		/* TODO: return whatever error currently compiler issues for > MAX_SRCLINE */
		return (uint4)-1;
	}
	memcpy(source_buffer, ptr, len);
	instr.addr = source_buffer;
	instr.len = len;
	if (status = patstr(&instr, &pat_ptstr, NULL))
		return status;
	assert(pat_ptstr.len <= MAX_PATOBJ_LENGTH);
	len = pat_ptstr.len * SIZEOF(uint4);
	ptr = (char *)get_new_element(gvt_trigger->gv_trig_list, DIVIDE_ROUND_UP(len, GVTR_LIST_ELE_SIZE));
	memcpy(ptr, &pat_ptstr.buff[0], len);
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
		gvtr_process_range(gvt, subsdsc, colon_imbalance ? GVTR_PARSE_RIGHT : GVTR_PARSE_POINT, start, end);
	return 0;
}

/* Sorts SET triggers ahead of KILL triggers. Implements it in linear O(n) time.
 * Does two passes.
 * 	1) The first one brings KILL-only type triggers to the tail of the trigger array
 * 		The head of the array now contains a mix of SET-only type triggers and SET,KILL type triggers (i.e. multiple cmds)
 * 	2) The second one brings SET-only type triggers to the head of the trigger array
 * 		This way SET,KILL type mixed triggers now go to the middle of the array.
 * This way, if a SET command needs to match/invoke triggers, all it needs to check for a match is the set of
 * 	triggers from the beginning (SET) to the middle section (SET,KILL).
 * And if a KILL command needs to match/invoke triggers, all it needs to check for a match is the set of
 * 	triggers from the middle section (SET,KILL) to the end (KILL).
 *
 * Example
 *      ----------------------------------------------------------------------------
 *      Trigger array     0      1            2           3           4      5
 *      ----------------------------------------------------------------------------
 *      Input array :    KILL1  SET1,KILL2   SET2        SET3,KILL3  KILL4  SET4
 *      After Pass1 :    SET4   SET1,KILL2   SET2        SET3,KILL3  KILL4  KILL1
 *      After Pass2 :    SET4   SET2         SET1,KILL2  SET3,KILL3  KILL4  KILL1
 *      ----------------------------------------------------------------------------
 *
 * So SET commands need to check only from array indices 0 through 3, whereas KILL commands need to check indices 2 through 5.
 * In this case, gvt_trigger->trigdsc_setstop will point to index 4 and gvt_trigger->trigdsc_killstart will point to index 2.
 * */
STATICFNDEF	void	gvtr_sort_sets_first(gvt_trigger_t *gvt_trigger, gv_trigger_t *trigtop)
{
	gv_trigger_t		*trigdscleft, *trigdscright, tmptrig, *trigdscsetstop, *trigdsckillstart;
	DEBUG_ONLY(
		boolean_t	killstart_seen;
		boolean_t	setstop_seen;
	)

	/* -------------- PASS 1 : Move KILL-only type triggers to end of the array ----------- */
	trigdscleft = &gvt_trigger->gv_trig_array[0];
	trigdscright = trigtop - 1;
	trigdscsetstop = trigtop;	/* the point where SET type triggers stop */
	do
	{	/* Find earliest non-SET type trigdsc on the left side of the array */
		for ( ; trigdscleft < trigdscright; trigdscleft++)
			if (!(gvtr_cmd_mask[GVTR_CMDTYPE_SET] & trigdscleft->cmdmask))
				break;
		assert((trigdscleft < trigdscright) || (trigdscleft == trigdscright));
		if (trigdscleft == trigdscright)
			break;
		/* Find latest SET type trigdsc on the right side of the array */
		for ( ; trigdscright > trigdscleft; trigdscright--)
			if (gvtr_cmd_mask[GVTR_CMDTYPE_SET] & trigdscright->cmdmask)
				break;
		assert((trigdscright > trigdscleft) || (trigdscright == trigdscleft));
		assert(trigdscsetstop > trigdscright);
		trigdscsetstop = trigdscright;
		if (trigdscright == trigdscleft)
			break;
		/* Swap these two trigger entries */
		tmptrig = *trigdscleft;
		*trigdscleft = *trigdscright;
		*trigdscright = tmptrig;
		/* Move both left and right trigger indices one position closer to each other */
		trigdscleft++;
		trigdscright--;
	} while (trigdscleft < trigdscright);
	if (trigdscsetstop > trigdscright)
	{
		assert(trigdscright == (trigdscsetstop - 1));
		if (!(trigdscright->cmdmask & gvtr_cmd_mask[GVTR_CMDTYPE_SET]))
			trigdscsetstop = trigdscright;
	}
	/* -------------- PASS 2 : Move SET-only type triggers to end of the array ----------- */
	trigdscleft = &gvt_trigger->gv_trig_array[0];
	trigdsckillstart = trigdscsetstop;	/* the point where KILL type triggers start */
	/* No need to do any sorting if ALL entries in the array are KILL-only type triggers */
	if (trigdscsetstop > trigdscleft)
	{
		trigdscright = trigdscsetstop - 1;
		do
		{	/* Find earliest SET,KILL type trigdsc on the left side of the array */
			for ( ; trigdscleft < trigdscright; trigdscleft++)
				if (~gvtr_cmd_mask[GVTR_CMDTYPE_SET] & trigdscleft->cmdmask)
					break;
			assert((trigdscleft < trigdscright) || (trigdscleft == trigdscright));
			if (trigdscleft == trigdscright)
				break;
			/* Find latest SET-only type trigdsc on the right side of the array */
			for ( ; trigdscright > trigdscleft; trigdscright--)
				if (!(~gvtr_cmd_mask[GVTR_CMDTYPE_SET] & trigdscright->cmdmask))
					break;
			assert((trigdscright > trigdscleft) || (trigdscright == trigdscleft));
			assert(trigdsckillstart > trigdscright);
			trigdsckillstart = trigdscright;
			if (trigdscright == trigdscleft)
				break;
			/* Swap these two trigger entries */
			tmptrig = *trigdscleft;
			*trigdscleft = *trigdscright;
			*trigdscright = tmptrig;
			/* Move both left and right trigger indices one position closer to each other */
			trigdscleft++;
			trigdscright--;
		} while (trigdscleft < trigdscright);
		if (trigdsckillstart > trigdscright)
		{
			assert(trigdscright == (trigdsckillstart - 1));
			if (trigdscright->cmdmask & ~gvtr_cmd_mask[GVTR_CMDTYPE_SET])
				trigdsckillstart = trigdscright;
		}
	}
#	ifdef DEBUG
	/* Verify that the array entries have been sorted right */
	killstart_seen = FALSE;
	setstop_seen   = FALSE;
	for (trigdscleft = &gvt_trigger->gv_trig_array[0]; trigdscleft < trigtop; trigdscleft++)
	{
		if (!(~gvtr_cmd_mask[GVTR_CMDTYPE_SET] & trigdscleft->cmdmask))
		{	/* SET-only type trigger */
			assert(!setstop_seen);
			assert(!killstart_seen);
		} else if (!(gvtr_cmd_mask[GVTR_CMDTYPE_SET] & trigdscleft->cmdmask))
		{	/* KILL-only type trigger */
			assert(setstop_seen || (trigdscsetstop == trigdscleft));
			setstop_seen = TRUE;
			assert(killstart_seen || (trigdsckillstart == trigdscleft));
			killstart_seen = TRUE;
		} else
		{	/* SET,KILL mixed type trigger */
			assert(killstart_seen || (trigdsckillstart == trigdscleft));
			killstart_seen = TRUE;
			assert(!setstop_seen);
		}
	}
#	endif
	gvt_trigger->cur_set_trigdsc = trigdscsetstop - 1;
	gvt_trigger->cur_kill_trigdsc = trigtop - 1;
	gvt_trigger->trigdsc_setstop = trigdscsetstop;
	gvt_trigger->trigdsc_killstart = trigdsckillstart;
	gvt_trigger->gv_trig_top = trigtop;
}

STATICFNDEF void	gvtr_db_read_hasht(sgmnt_addrs *csa)
{
	gv_namehead		*hasht_tree, *save_gvtarget, *gvt;
	mname_entry		gvent;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gv_key			*save_gv_altkey;
	mval			tmpmval, *ret_mval;
	boolean_t		is_defined, was_null = FALSE, is_null = FALSE, zdelim_defined, delim_defined;
	short int		max_key;
	int4			tmpint4;
	gvt_trigger_t		*gvt_trigger;
	uint4			trigidx, num_gv_triggers, num_pieces, len, cmdtype, index, minpiece, maxpiece;
	gv_trigger_t		*gv_trig_array, *trigdsc;
	uint4			currkey_end, cycle, numsubs, cursub, numlvsubs, curlvsub;
	char			ch, *ptr, *ptr_top, *ptr_start;
	boolean_t		quote_imbalance, colon_imbalance, end_of_subscript;
	uint4			paren_imbalance;
	gvtr_subs_t		*subsdsc;
	mname_entry		*lvnamedsc;
	uint4			*lvindexdsc, status;
	int			ctype;

	save_gvtarget = gv_target;
	SETUP_TRIGGER_GLOBAL;
	/* Save gv_currkey and gv_altkey */
	assert(NULL != gv_currkey);
	assert((SIZEOF(gv_key) + gv_currkey->end) <= SIZEOF(save_currkey));
	save_gv_currkey = (gv_key *)&save_currkey[0];
	memcpy(save_gv_currkey, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);
	assert(NULL != gv_altkey);
	assert((SIZEOF(gv_key) + gv_altkey->end) <= SIZEOF(save_altkey));
	save_gv_altkey = (gv_key *)&save_altkey[0];
	memcpy(save_gv_altkey, gv_altkey, SIZEOF(gv_key) + gv_altkey->end);
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	gv_currkey->prev = 0;
	if (0 == gv_target->root)
	{	/* ^#t global does not exist. Return right away. */
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
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		return;
	}
	if ((STR_LIT_LEN(HASHT_GBL_CURLABEL) != ret_mval->str.len) || MEMCMP_LIT(ret_mval->str.addr, HASHT_GBL_CURLABEL))
	{	/* ^#t("GBL","#LABEL") is NOT expected value so issue error */
		assert(FALSE);
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		/* TODO : issue error */
		return;
	}
	/* So we can go ahead and read other ^#t("GBL") records */
	/* -----------------------------------------------------------------------------
	 *          Now read ^#t("GBL","#CYCLE")
	 * -----------------------------------------------------------------------------
	 */
	is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_hashcycle, ret_mval);
	assert(is_defined);
	if (!is_defined)
	{	/* ^#t("GBL","#LABEL") is defined but NOT ^#t("GBL","#CYCLE"). ^#t global integrity is suspect */
		assert(FALSE);
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		/* TODO : issue error */
		return;
	}
	tmpint4 = mval2i(ret_mval);	/* decimal values are truncated by mval2i so we will accept a #CYCLE of 1.5 as 1 */
	if (0 >= tmpint4)
	{	/* ^#t("GBL","#CYCLE") is not a positive integer. Error out */
		assert(FALSE);
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		/* TODO : issue error */
		return;
	}
	cycle = (uint4)tmpint4;
	/* Check if ^#t("GBL") has previously been read from the db. If so, check if cycle is same as ^#t("GBL","#CYCLE"). */
	gvt_trigger = gvt->gvt_trigger;
	if (NULL != gvt_trigger)
	{
		if (gvt_trigger->gv_trigger_cycle == cycle)
		{	/* Since cycle is same, no need to reinitialize any triggers for "GBL". Can return safely.
			 * Pass "FALSE" to GVTR_HASHTGBL_READ_CLEANUP macro to ensure gvt->gvt_trigger is NOT freed.
			 */
			GVTR_HASHTGBL_READ_CLEANUP(FALSE);
			return;
		}
		/* Now that we have to reinitialize triggers for "GBL", free up previously allocated structure first */
		gvtr_free(gvt);
		assert(NULL == gvt->gvt_trigger);
	}
	gvt_trigger = (gvt_trigger_t *)malloc(SIZEOF(gvt_trigger_t));
	gvt_trigger->gv_trig_array = NULL;
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
	{	/* ^#t("GBL","#LABEL") is defined but NOT ^#t("GBL","#COUNT"). ^#t global integrity is suspect */
		assert(FALSE);
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		/* TODO : issue error */
		return;
	}
	tmpint4 = mval2i(ret_mval);	/* decimal values are truncated by mval2i so we will accept a #COUNT of 1.5 as 1 */
	if (0 >= tmpint4)
	{	/* ^#t("GBL","#COUNT") is not a positive integer. Error out */
		assert(FALSE);
		GVTR_HASHTGBL_READ_CLEANUP(TRUE);
		/* TODO : issue error */
		return;
	}
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
		/* Read in ^#t("GBL",1,"CMD")="S,K,ZK,ZTK" */
		is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_cmd, ret_mval);
		assert(is_defined); /* ^#t("GBL","#COUNT") is set to 1 so ^#t("GBL",1,"CMD") should be defined */
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
						&& !memcmp(ptr_start, gvtr_cmd_mval[cmdtype].str.addr, len))
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
			subsdsc = &trigdsc->subsarray[0];
			/* Initialize trigdsc->numlvsubs */
			assert(numlvsubs <= numsubs);
			trigdsc->numlvsubs = numlvsubs;
			if (numlvsubs)
			{
				curlvsub = 0;
				/* Allocate trigdsc->lvnamearray */
				trigdsc->lvnamearray = (mname_entry *)get_new_element(gvt_trigger->gv_trig_list,
							DIVIDE_ROUND_UP((numlvsubs * SIZEOF(mname_entry)), GVTR_LIST_ELE_SIZE));
				lvnamedsc = &trigdsc->lvnamearray[0];
				/* Allocate trigdsc->lvindexarray */
				trigdsc->lvindexarray = (uint4 *)get_new_element(gvt_trigger->gv_trig_list,
							DIVIDE_ROUND_UP((numlvsubs * SIZEOF(uint4)), GVTR_LIST_ELE_SIZE));
				lvindexdsc = &trigdsc->lvindexarray[0];
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
					GVTR_PROCESS_GVSUBS(ptr_start, ptr, subsdsc, colon_imbalance, gvt);
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
					gvtr_process_range(gvt, subsdsc, GVTR_PARSE_LEFT, ptr_start, ptr);
					assert(!colon_imbalance);
					colon_imbalance = TRUE;
					ptr_start = &ptr[1];
				}
				if (end_of_subscript)
				{
					assert(!quote_imbalance);
					assert(!paren_imbalance);
					GVTR_PROCESS_GVSUBS(ptr_start, ptr, subsdsc, colon_imbalance, gvt);
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
		/* Read in ^#t("GBL",1,"TRIGNAME")="GBL#1" */
		is_defined =  gvtr_get_hasht_gblsubs((mval *)&literal_trigname, ret_mval);
		assert(is_defined);	/* mupip trigger should be making sure trigger name is created */
		trigdsc->rtn_desc.rt_name = ret_mval->str;	/* Copy trigger name mident */
		trigdsc->rtn_desc.rt_adr = NULL;
		/* Reserve extra space when triggername is placed in buddy list. This space is used by gtm_trigger() if
		 * the trigger name is not unique within the process.One or two of the chars are appended to the trigger
		 * name until an unused name is found.
		 */
		trigdsc->rtn_desc.rt_name.len += TRIGGER_NAME_RESERVED_SPACE;
		GVTR_POOL2BUDDYLIST(gvt_trigger, &trigdsc->rtn_desc.rt_name);
		trigdsc->rtn_desc.rt_name.len -= TRIGGER_NAME_RESERVED_SPACE;	/* Not using the space yet */
		/* Read in ^#t("GBL",1,"CHSET")="UTF-8". If CHSET does not match gtm_chset issue error. */
		is_defined =  gvtr_get_hasht_gblsubs((mval *)&literal_chset, ret_mval);
		assert(is_defined);	/* mupip trigger should have made sure of this */
		if ((!gtm_utf8_mode && ((STR_LIT_LEN(CHSET_M_STR) != ret_mval->str.len)
						|| memcmp(ret_mval->str.addr, CHSET_M_STR, STR_LIT_LEN(CHSET_M_STR))))
			|| (gtm_utf8_mode && ((STR_LIT_LEN(CHSET_UTF8_STR) != ret_mval->str.len)
						|| memcmp(ret_mval->str.addr, CHSET_UTF8_STR, STR_LIT_LEN(CHSET_UTF8_STR)))))
		{	/* CHSET mismatch */
			rts_error(VARLSTCNT(8) ERR_TRIGINVCHSET, 6, trigdsc->rtn_desc.rt_name.len, trigdsc->rtn_desc.rt_name.addr,
				gvt->gvname.var_name.len, gvt->gvname.var_name.addr, ret_mval->str.len, ret_mval->str.addr);
		}
		/* Read in ^#t("GBL",1,"XECUTE")="do ^XNAMEinGBL"	*/
		ret_mval->str.len = 0;	/* Initialize "len" to 0 (instead of garbage) in case is_defined comes back as FALSE */
		is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_xecute, ret_mval);
		assert(is_defined);
		/* ^#t("GBL",1,"CMD") is defined but ^#t("GBL",1,"XECUTE") is undefined.
		 * We set xecute_str.len to 0 this way we will see a runtime error when trying to compile this null length
		 * string. We dont do any checks of is_defined here for performance reasons (assuming MUPIP TRIGGER would have
		 * loaded the proper content) and yet handle out-of-design situations gracefully.
		 * TODO: Verify if above is still necessary - MUPIP TRIGGER now pre-compiling triggers (SEstes)
		 */
		if (MAX_SRCLINE <= ret_mval->str.len)
		{
			assert(FALSE);
			GVTR_HASHTGBL_READ_CLEANUP(TRUE);
			/* TODO : Issue error */
		}
		/* Initialize trigdsc->xecute_str */
		trigdsc->xecute_str = *ret_mval;
		GVTR_POOL2BUDDYLIST(gvt_trigger, &trigdsc->xecute_str.str);
		/* trigdsc->rtn_desc is already NULL-initialized as part of the memset above */
		gv_currkey->end = currkey_end;	/* remove the current <trigidx> and make way for the next <trigidx> in gv_currkey */
		gv_currkey->base[currkey_end] = KEY_DELIMITER;    /* restore terminator for entire key */
	}
	/* Now that ALL triggers for this global has been read, move all GVTR_CMDTYPE_SET type triggers to the front of the array.
	 * This way, any SET command (most frequent) can stop examining triggers (for a potential match) the moment it encounters
	 * a non-GVTR_CMDTYPE_SET type trigger in the trigger array.
	 */
	gvtr_sort_sets_first(gvt_trigger, trigdsc);
	GVTR_HASHTGBL_READ_CLEANUP(FALSE);	/* do NOT free gvt->gvt_trigger so pass FALSE */
	gvt_trigger->gv_trigger_cycle = cycle;	/* Now that ^#t has been read, we can safely update "cycle" to the higher value */
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
	subsdsc = &trigdsc->subsarray[0];
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

STATICFNDEF	void	gvtr_free(gv_namehead *gvt)
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
}

/* Initializes triggers for global variable "gvt" from ^#t global. */
void	gvtr_init(gv_namehead *gvt, uint4 cycle, boolean_t tp_is_implicit)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gv_namehead		*dir_tree;
	boolean_t		is_nontp;
	uint4			save_t_err;
	uint4			save_update_trans;

	assert(!skip_dbtriggers);	/* should not come here if triggers are not supposed to be invoked */
	csa = gvt->gd_csa;
	assert(NULL != csa);	/* database for this gvt better be opened at this point */
	dir_tree = csa->dir_tree;
	if (gvt == dir_tree)
		return; /* trigger initialization should return for directory tree (e.g. set^%GBLDEF is caller) */
	csd = csa->hdr;
	assert(csa->db_trigger_cycle <= cycle);
	assert(gvt->db_trigger_cycle < cycle);
	is_nontp = !dollar_tlevel;
	/* Save t_err (for non-TP and TP) and update_trans (for non-TP) of explicit update */
	save_t_err = t_err;
	if (is_nontp)
	{	/* Wrap the ^#t global read as a separate TP transaction as we dont know how many records we will
		 * need to read and might not be able to fit all of it into the non-TP transaction structures which
		 * assume at most two "gvcst_search"es.
		 */
		save_update_trans = update_trans;
		assert(save_update_trans);	/* triggers can be invoked only by updates currently */
		assert(FALSE == tp_is_implicit);
		gvtr_db_tpwrap(csa, FALSE);
		/* Restore t_err and update_trans for non-TP */
		t_begin(save_t_err, save_update_trans);
	} else	/* already in TP */
	{
		assert(sgm_info_ptr == csa->sgm_info_ptr);
		save_update_trans = sgm_info_ptr->update_trans;
		assert(save_update_trans); /* triggers can be invoked only by updates currently */
		/* Just set t_err to reflect this particular action */
		t_err = ERR_GVGETFAIL;
		/* Check if TP was in turn an implicit TP (e.g. created by the GVTR_INIT_AND_TPWRAP_IF_NEEDED macro).
		 * If so, we still need to do something like the gvtr_db_tpwrap done above since we do not want any
		 * t_retry calls to end up invoking MUM_TSTART.
		 */
		if (tp_is_implicit)
		{	/* If implicit tp, for the first try, we would not yet have invoked the implicit
			 * "op_tstart" (in GVTR_INIT_AND_TPWRAP_IF_NEEDED macro) when calling gvtr_init.
			 * Therefore, this should have been the second (or more) invocation of the GVTR_INIT_AND_TPWRAP_IF_NEEDED
			 * macro for the same gvcst_put or gvcst_kill action which means there was at least one retry.
			 */
			assert(t_tries);
			assert(donot_INVOKE_MUMTSTART);
			gvtr_db_tpwrap(csa, tp_is_implicit);
			assert(sgm_info_ptr == csa->sgm_info_ptr);
			/* If a TP restart happened inside gvtr_db_tpwrap, we would have reset sgm_info_ptr->update_trans to 0
			 * but we want to leave this function with update_trans unchanged since the caller (gvcst_put/gvcst_kill)
			 * relies on that. So reset update_trans.
			 */
			sgm_info_ptr->update_trans = save_update_trans;
		} else
			gvtr_db_read_hasht(csa);
		/* Restore t_err for TP */
		t_err = save_t_err;
		assert(sgm_info_ptr == csa->sgm_info_ptr);
		assert(csa->sgm_info_ptr->update_trans); /* triggers can be invoked only by updates currently */
	}
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
}

/* Determines matching triggers for a given gv_currkey and uses "gtm_trigger" to invokes them with appropriate parameters.
 * Returns 0 for success and ERR_TPRETRY in case a retry is detected.
 */
int	gvtr_match_n_invoke(gtm_trigger_parms *trigparms, gvtr_invoke_parms_t *gvtr_parms)
{
	gvtr_cmd_type_t		gvtr_cmd;
	gvt_trigger_t		*gvt_trigger;
	boolean_t		is_set_trigger, ok_to_invoke_trigger, trigtop_reached;
	char			*key_ptr, *key_start, *key_end;
	char			*keysub_start[MAX_KEY_SZ + 1];
	gv_trigger_t		*trigdsc, *trigtop, *trigstart, *trigcmd_start;
	int			gtm_trig_status, num_triggers_invoked;
	mstr			*ztupd_mstr;
	mval			*keysub_mval;
	mval			*lvvalarray[MAX_GVSUBSCRIPTS + 1];
	mval			*ztupd_mval, dummy_mval;
	uint4			*lvindexarray;
	uint4			keysubs, keylen, numlvsubs, curlvsub, lvvalindex, cursub;
	char			*thissub;
#	ifdef DEBUG
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sgm_info		*si;
	gv_key			*save_gv_currkey;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_namehead		*save_targ;
	int			lcl_gtm_trigger_depth;
#	endif

	DEBUG_ONLY(csa = cs_addrs;)
	DEBUG_ONLY(csd = cs_data;)
	DEBUG_ONLY(si = sgm_info_ptr;)
	DEBUG_ONLY(save_targ = gv_target;)
	assert(gv_target != csa->dir_tree);
	assert(dollar_tlevel);
	/* Check that minimal pre-filling of trigparms has already occurred in the caller */
	assert(NULL != trigparms->ztoldval_new);
	assert(NULL != trigparms->ztvalue_new);
	assert(NULL != trigparms->ztdata_new);
	/* Initialize trigger parms that dont depend on the context of the matching trigger */
	gvtr_cmd = gvtr_parms->gvtr_cmd;
	gvt_trigger = gvtr_parms->gvt_trigger;
	trigparms->ztriggerop_new = &gvtr_cmd_mval[gvtr_cmd];
	is_set_trigger = (GVTR_CMDTYPE_SET == gvtr_cmd);
	if (is_set_trigger)
	{
		PUSH_MV_STENT(MVST_MVAL);	/* protect $ztupdate from stp_gcol */
		ztupd_mval = &mv_chain->mv_st_cont.mvs_mval;
		ztupd_mval->str.len = 0;
		ztupd_mval->mvtype = 0; /* keep mvtype set to 0 until mval is fully initialized (later below) */
		ztupd_mstr = &ztupd_mval->str;
		trigparms->ztupdate_new = ztupd_mval;
	} else
	{
		ztupd_mval = &dummy_mval;
		trigparms->ztupdate_new = (mval *)&literal_zero;
	}
	trigparms->lvvalarray = &lvvalarray[0];
	trigparms->ztvalue_changed = FALSE;
	/* Parse gv_currkey into array of subscripts to facilitate trigger matching */
	key_ptr = (char *)gv_currkey->base;
	DEBUG_ONLY(key_start = key_ptr;)
	key_end = key_ptr + gv_currkey->end;
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
	DEBUG_ONLY(keylen = INTCAST(key_end - key_start);)
	assert(!keysubs || keylen);
	/* Save gv_currkey to check that it is restored correctly after "gtm_trigger_fini" is invoked */
	DEBUG_ONLY(
		assert((SIZEOF(gv_key) + gv_currkey->end) < ARRAYSIZE(save_currkey));
		save_gv_currkey = (gv_key *)&save_currkey[0];
		memcpy(save_gv_currkey, gv_currkey, OFFSETOF(gv_key, base[0]) + gv_currkey->end);
	)
	/* Match & Invoke triggers. Take care to ensure they are invoked in an UNPREDICTABLE order.
	 * Current implementation is to invoke triggers in a rotating order. For example, if there are
	 * 3 triggers 0, 1 and 2, the first update to this global will match triggers in the order
	 * 0,1,2, the second update will match triggers in the order 1,2,0 and the third 2,0,1.
	 */
	num_triggers_invoked = 0;
	if (is_set_trigger)
	{	/* Determine last non-SET type of trigger in array */
		trigcmd_start = &gvt_trigger->gv_trig_array[0];
		trigstart = ++gvt_trigger->cur_set_trigdsc;
		trigtop = gvt_trigger->trigdsc_setstop;
		if (trigstart >= trigtop) /* trigstart can be > trigtop if there are NO "SET" type triggers */
		{
			trigstart = trigcmd_start;
			gvt_trigger->cur_set_trigdsc = trigstart;
		}
		assert(&gvt_trigger->gv_trig_array[0] <= gvt_trigger->cur_set_trigdsc);
		assert(gvt_trigger->cur_set_trigdsc <= gvt_trigger->trigdsc_setstop);
	} else
	{	/* Determine first KILL type of trigger in array */
		trigcmd_start = gvt_trigger->trigdsc_killstart;
		trigstart = ++gvt_trigger->cur_kill_trigdsc;
		trigtop = gvt_trigger->gv_trig_top;
		assert(trigtop == &gvt_trigger->gv_trig_array[gvt_trigger->num_gv_triggers]);
		if (trigstart >= trigtop) /* trigstart can be > trigtop if there are NO "SET" type triggers */
		{
			trigstart = trigcmd_start;
			gvt_trigger->cur_kill_trigdsc = trigstart;
		}
		assert(gvt_trigger->cur_kill_trigdsc >= gvt_trigger->trigdsc_killstart);
		assert(gvt_trigger->cur_kill_trigdsc <= gvt_trigger->gv_trig_top);
	}
	trigtop_reached = FALSE;
	/* Note: trigstart would be EQUAL to trigtop if there are NO "SET" or "KILL" type triggers for this global */
	for ( trigdsc = trigstart; ; trigdsc++)
	{
		if (trigdsc == trigtop)
		{
			trigdsc = trigcmd_start;
			trigtop_reached = TRUE;
		}
		if (trigtop_reached && (trigdsc == trigstart))
			break;
		assert((trigdsc->cmdmask & gvtr_cmd_mask[gvtr_cmd]) || !is_set_trigger);
		if (!is_set_trigger && !(trigdsc->cmdmask & gvtr_cmd_mask[gvtr_cmd]))
			continue; /* Trigger is for different command. Currently only possible for KILL/ZKILL (asserted above) */
		/* Check that global variables which could have been modified inside gvcst_put/gvcst_kill have been
		 * reset to their default values before going into trigger code as that could cause a nested call to
		 * gvcst_put/gvcst_kill and we dont want any non-default value of this global variable from the parent
		 * gvcst_put/gvcst_kill to be reset by the nested invocation.
		 */
		assert(INVALID_GV_TARGET == reset_gv_target);
		if ((keysubs == trigdsc->numsubs) && (!keysubs || gvtr_is_key_a_match(keysub_start, trigdsc, &lvvalarray[0])))
		{
			/* Note: lvvalarray could be updated above in case any trigger patterns
			 * needed to have been checked for a key-match. Before invoking the trigger,
			 * check if it specified any local variables for subscripts. If so,
			 * initialize any that are not yet already done.
			 */
			if (numlvsubs = trigdsc->numlvsubs)
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
				} else if (gvtr_parms->duplicate_set)
				{	/* No pieces of interest specified AND no change to value.
					 * Do NOT invoke triggers.
					 */
					ok_to_invoke_trigger = FALSE;
				}
				assert(ok_to_invoke_trigger || (0 == ztupd_mval->mvtype));
			}
			if (ok_to_invoke_trigger)
			{
				DEBUG_ONLY(lcl_gtm_trigger_depth = gtm_trigger_depth;)
				gtm_trig_status = gtm_trigger(trigdsc, trigparms);
					/* note: the above call may update trigparms->ztvalue_new for SET type triggers */
				assert(lcl_gtm_trigger_depth == gtm_trigger_depth);
				num_triggers_invoked++;
				ztupd_mval->mvtype = 0;	/* so stp_gcol (if invoked somehow) can free up any space
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
		gtm_trigger_fini(FALSE);
	/* Verify that gtm_trigger_fini restored gv_cur_region/cs_addrs/cs_data/gv_target/gv_currkey
	 * properly (gtm_trigger could have changed these values depending on the M code that was invoked).
	 */
	assert(csa == cs_addrs);
	assert(csd == cs_data);
	assert(si == sgm_info_ptr);
	assert(gv_target == save_targ);
	assert(0 == memcmp(save_gv_currkey, gv_currkey, OFFSETOF(gv_key, base[0]) + gv_currkey->end));
	DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC;
	gvtr_parms->num_triggers_invoked = num_triggers_invoked;
	return 0;
}
#endif /* GTM_TRIGGER */
