/****************************************************************
 *								*
 * Copyright (c) 2014-2017 Fidelity National Information	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "trigger.h"
#include <rtnhdr.h>			/* needed for gv_trigger.h */
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "gv_trigger_protos.h"
#include "trigger_upgrade_protos.h"
#include "tp_restart.h"
#include "change_reg.h"
#include "targ_alloc.h"
#include "mv_stent.h"
#include "op.h"
#include "tp_frame.h"
#include "gvcst_protos.h"
#include "gvsub2str.h"
#include "mvalconv.h"
#include "op_tcommit.h"
#include "format_targ_key.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "jnl.h"			/* for jnl_format prototype and for tp.h */
#include "tp.h"				/* needed for T_BEGIN_SETORKILL_NONTP_OR_TP */
#include "t_begin.h"
#include "repl_sp.h"			/* for F_CLOSE (used by JNL_FD_CLOSE) */
#include "gtmio.h"			/* for CLOSEFILE_RESET macro */

GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		dollar_tlevel;
GBLREF	gv_key		*gv_currkey;
GBLREF	unsigned int	t_tries;
#ifdef DEBUG
GBLREF	boolean_t	is_replicator;
GBLREF	boolean_t	donot_INVOKE_MUMTSTART;
#endif

error_def(ERR_TRIGUPBADLABEL);

#define LITERAL_TRIGJNLREC	"; ^#t physical upgrade from #LABEL 2,3 to #LABEL 4 (no logical change)"
#define	LITERAL_TRIGJNLREC_LEN	STR_LIT_LEN(LITERAL_TRIGJNLREC)
LITDEF mval literal_trigjnlrec	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_TRIGJNLREC_LEN,
							(char *)LITERAL_TRIGJNLREC, 0, 0);

LITREF	mval		literal_batch;
LITREF	mval		literal_curlabel;
LITREF	mval		literal_hashlabel;
LITREF	mval		literal_hashcycle;
LITREF	mval		literal_hashcount;
LITREF	mval		literal_hashtrhash;
#define TRIGGER_SUBSDEF(SUBSTYPE, SUBSNAME, LITMVALNAME, TRIGFILEQUAL, PARTOFHASH)	LITREF mval LITMVALNAME;
#include "trigger_subs_def.h"
#undef TRIGGER_SUBSDEF

DEFINE_NSB_CONDITION_HANDLER(trigger_upgrade_ch)

STATICFNDCL void	gvtr_set_hasht_gblsubs(mval *subs_mval, mval *set_mval);
STATICFNDCL void	gvtr_kill_hasht_gblsubs(mval *subs_mval, boolean_t killall);
STATICFNDEF void	gvtr_set_hashtrhash(char *trigvn, int trigvn_len, uint4 hash_code, int trigindx);

STATICFNDEF void	gvtr_set_hasht_gblsubs(mval *subs_mval, mval *set_mval)
{
	uint4		curend;
	boolean_t	was_null = FALSE, is_null = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	curend = gv_currkey->end; /* note down gv_currkey->end before changing it so we can restore it before function returns */
	assert(KEY_DELIMITER == gv_currkey->base[curend]);
	assert(gv_target->gd_csa == cs_addrs);
	COPY_SUBS_TO_GVCURRKEY(subs_mval, gv_cur_region, gv_currkey, was_null, is_null); /* updates gv_currkey */
	MV_FORCE_STR(set_mval);
	gvcst_put(set_mval);
	gv_currkey->end = curend;	/* reset gv_currkey->end to what it was at function entry */
	gv_currkey->base[curend] = KEY_DELIMITER;    /* restore terminator for entire key so key is well-formed */
	return;
}

STATICFNDEF void	gvtr_kill_hasht_gblsubs(mval *subs_mval, boolean_t killall)
{
	uint4		curend;
	boolean_t	was_null = FALSE, is_null = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	curend = gv_currkey->end; /* note down gv_currkey->end before changing it so we can restore it before function returns */
	assert(KEY_DELIMITER == gv_currkey->base[curend]);
	assert(gv_target->gd_csa == cs_addrs);
	COPY_SUBS_TO_GVCURRKEY(subs_mval, gv_cur_region, gv_currkey, was_null, is_null); /* updates gv_currkey */
	gvcst_kill(killall);
	gv_currkey->end = curend;	/* reset gv_currkey->end to what it was at function entry */
	gv_currkey->base[curend] = KEY_DELIMITER;    /* restore terminator for entire key so key is well-formed */
	return;
}

/* Set ^#t(<gbl>,"#TRHASH",hash_code,nnn)=<gbl>_$c(0)_trigindx where gv_currkey is ^#t(<gbl>).
 * Note: This routine has code very similar to that in "add_trigger_hash_entry". There is just
 * not enough commonality to justify merging the two.
 */
STATICFNDEF void	gvtr_set_hashtrhash(char *trigvn, int trigvn_len, uint4 hash_code, int trigindx)
{
	uint4		curend;
	mval		mv_indx, *mv_indx_ptr;
	mval		mv_hash;
	int		hash_indx, num_len;
	char		name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	char		*ptr;
	char		indx_str[MAX_DIGITS_IN_INT];
	uint4		len;
	int4		result;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	curend = gv_currkey->end; /* note down gv_currkey->end before changing it so we can restore it before function returns */
	assert(KEY_DELIMITER == gv_currkey->base[curend]);
	assert(gv_target->gd_csa == cs_addrs);
	/* Set ^#t(<gvn>,"#TRHASH",kill_hash_code,i). To do that, first determine the
	 * highest i such that ^#t(<gvn>,"#TRHASH",kill_hash_code,i) exists. Set
	 * ^#t(<gvn>,"#TRHASH",kill_hash_code,i+1)=kill_hash_code in that case.
	 */
	MV_FORCE_UMVAL(&mv_hash, hash_code);
	BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
	op_zprevious(&mv_indx);
	mv_indx_ptr = &mv_indx;
	hash_indx = (0 == mv_indx.str.len) ? 1 : (mval2i(mv_indx_ptr) + 1);
	i2mval(mv_indx_ptr, hash_indx);
	MV_FORCE_STR(mv_indx_ptr);
	/* Prepare the value of the SET */
	num_len = 0;
	I2A(indx_str, num_len, trigindx);
	assert(MAX_MIDENT_LEN >= trigvn_len);
	memcpy(name_and_index, trigvn, trigvn_len);
	ptr = name_and_index + trigvn_len;
	*ptr++ = '\0';
	memcpy(ptr, indx_str, num_len);
	len = trigvn_len + 1 + num_len;
	/* Do the SET */
	SET_TRIGGER_GLOBAL_SUB_SUB_MSUB_MSUB_STR(trigvn, trigvn_len,
		LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH), mv_hash, mv_indx, name_and_index, len, result);
	assert(PUT_SUCCESS == result);
	gv_currkey->end = curend;	/* reset gv_currkey->end to what it was at function entry */
	gv_currkey->base[curend] = KEY_DELIMITER;    /* restore terminator for entire key so key is well-formed */
	return;
}

/* Upgrade ^#t global in "reg" region */
void	trigger_upgrade(gd_region *reg)
{
	boolean_t		est_first_pass, do_upgrade, is_defined;
	boolean_t		was_null = FALSE, is_null = FALSE;
	int			seq_num, trig_seq_num;
	int			currlabel;
	mval			tmpmval, xecuteimval, *gvname, *tmpmv, *tmpmv2;
	int4			result, tmpint4;
	uint4			curend, gvname_prev, xecute_curend;
	uint4			hash_code, kill_hash_code;
	int			count, i, xecutei, tncount;
	char			*trigname, *trigindex, *ptr;
	char			name_and_index[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT];
	char			trigvn[MAX_MIDENT_LEN + 1 + MAX_DIGITS_IN_INT], nullbyte[1];
	uint4			trigname_len, name_index_len;
	int			ilen;
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	uint4			sts;
	int			close_res;
	hash128_state_t		hash_state, kill_hash_state;
	uint4			hash_totlen, kill_hash_totlen;
	int			trig_protected_mval_push_count;
#	ifdef DEBUG
	int			save_dollar_tlevel;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gv_cur_region == reg);
	assert(!dollar_tlevel);	/* caller should have ensured this. this is needed as otherwise things get complicated. */
	assert(!is_replicator);	/* caller should have ensured this. this is needed so we dont bump jnl_seqno (if replicating) */
	csa = &FILE_INFO(reg)->s_addrs;
	assert(csa->hdr->hasht_upgrade_needed);
	/* If before-image journaling is turned on in this region (does not matter if replication is turned on or not),
	 * once this transaction is done, we need to switch to new journal file and cut the back link because
	 * otherwise it is possible for backward journal recovery (or rollback) or source server to encounter
	 * the journal records generated in this ^#t-upgrade-transaction in which case they dont know to handle
	 * it properly (e.g. rollback or backward recovery does not know to restore csa->hdr->hasht_upgrade_needed
	 * if it rolls back this transaction). To achieve this, we set hold_onto_crit to TRUE and do the jnl link
	 * cut AFTER the transaction commits but before anyone else can sneak in to do any more updates.
	 * Since most often we expect databases to be journaled, we do this hold_onto_crit even for the non-journaled case.
	 */
	grab_crit(reg);
	csa->hold_onto_crit = TRUE;
	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	assert(!donot_INVOKE_MUMTSTART);
	DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
	op_tstart(IMPLICIT_TSTART, TRUE, &literal_batch, 0); /* 0 ==> save no locals but RESTART OK */
	ESTABLISH_NORET(trigger_upgrade_ch, est_first_pass);
	/* On a TP restart anywhere down below, this line is where the restart resumes execution from */
	assert(donot_INVOKE_MUMTSTART);	/* Make sure still set for every try/retry of TP transaction */
	change_reg(); /* TP_CHANGE_REG wont work as we need to set sgm_info_ptr */
	assert(NULL != cs_addrs);
	assert(csa == cs_addrs);
	SET_GVTARGET_TO_HASHT_GBL(csa);	/* sets up gv_target */
	assert(NULL != gv_target);
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;	/* Needed to do every retry in case restart was due to an online rollback.
						 * This also sets up gv_currkey */
	/* Do actual upgrade of ^#t global.
	 *
	 * Below is a sample layout of the label 2 ^#t global
	 * -------------------------------------------------------
	 * ^#t("#TNAME","x")="a"_$C(0)_"1"		(present in DEFAULT only)
	 * ^#t("#TRHASH",89771515,1)="a"_$C(0)_"1"	(present in DEFAULT only)
	 * ^#t("#TRHASH",106937755,1)="a"_$C(0)_"1"	(present in DEFAULT only)
	 * ^#t("a",1,"BHASH")="106937755"
	 * ^#t("a",1,"CHSET")="M"
	 * ^#t("a",1,"CMD")="S"
	 * ^#t("a",1,"LHASH")="89771515"
	 * ^#t("a",1,"TRIGNAME")="x#"
	 * ^#t("a",1,"XECUTE")=" do ^twork"
	 * ^#t("a","#COUNT")="1"
	 * ^#t("a","#CYCLE")="1"
	 * ^#t("a","#LABEL")="2"
	 *
	 * Below is a sample layout of the label 3 ^#t global
	 * -------------------------------------------------------
	 * ^#t("#LABEL")="3"				(present only after upgrade, not regular trigger load)
	 * ^#t("#TNAME","x")="a"_$C(0)_"1"		(present in CURRENT region)
	 * ^#t("a",1,"BHASH")="71945627"
	 * ^#t("a",1,"CHSET")="M"
	 * ^#t("a",1,"CMD")="S"
	 * ^#t("a",1,"LHASH")="71945627"
	 * ^#t("a",1,"TRIGNAME")="x#"
	 * ^#t("a",1,"XECUTE")=" do ^twork"
	 * ^#t("a","#COUNT")="1"
	 * ^#t("a","#CYCLE")="2"
	 * ^#t("a","#LABEL")="3"
	 * ^#t("a","#TRHASH",71945627,1)="a"_$C(0)_"1"
	 *
	 * Key aspects of the format change
	 * ----------------------------------
	 * 1) New ^#t("#LABEL")="3" to indicate the format of the ^#t global. This is in addition to
	 * 	^#t("a","#LABEL") etc. which is already there. This way we have a #LABEL for not just the installed
	 * 	triggers but also for the name information stored in the #TNAME nodes.
	 * 2) In the BHASH and LHASH fields. The hash computation is different so there are more chances of BHASH and LHASH
	 * 	matching in which case we store only one #TRHASH entry (instead of two). So thre is fewer ^#t records in the new
	 * 	format in most cases.
	 * 3) ^#t("a","#LABEL") bumps from 2 to 3. Similarly ^#t("a","#CYCLE") bumps by one (to make sure triggers for this
	 *	global get re-read if and when we implement an -ONLINE upgrade).
	 * 4) DEFAULT used to have ^#t("#TNAME",...) nodes corresponding to triggers across ALL regions in the gbldir and
	 * 	other regions used to have NO ^#t("#TNAME",...) nodes whereas after the upgrade every region have
	 *	^#t("#TNAME",...) nodes	corresponding to triggers installed in that region. So it is safer to kill ^#t("#TNAME")
	 *	nodes and add them as needed.
	 * 5) #TRHASH has moved from ^#t() to ^#t(<gbl>). So it is safer to kill ^#t("#TRHASH")	nodes and add them as needed.
	 *
	 * Below is a sample layout of the label 4 ^#t global
	 * -------------------------------------------------------
	 * ^#t("#TNAME","x")="a"_$C(0)_"1"		(present in CURRENT region)
	 * ^#t("a",1,"BHASH")="71945627"
	 * ^#t("a",1,"CHSET")="M"
	 * ^#t("a",1,"CMD")="S"
	 * ^#t("a",1,"LHASH")="71945627"
	 * ^#t("a",1,"TRIGNAME")="x#"
	 * ^#t("a",1,"XECUTE")=" do ^twork"
	 * ^#t("a","#COUNT")="1"
	 * ^#t("a","#CYCLE")="2"
	 * ^#t("a","#LABEL")="4"
	 * ^#t("a","#TRHASH",71945627,1)="a"_$C(0)_"1"
	 *
	 * Key aspects of the format change
	 * ----------------------------------
	 * 1) Removed ^#t("#LABEL") as it is redundant information and trigger load does not include it
	 * 2) Multiline triggers were incorrectly processed resulting in incorrect BHASH and LHASH values. Upgrade fixes this
	 * 3) ^#t("a","#LABEL") bumps from 3 to 4. Similarly ^#t("a","#CYCLE") bumps by one (to make sure
	 * 	triggers for this global get re-read if and when we implement an -ONLINE upgrade).
	 */
	tmpmv = &tmpmval;	/* At all points maintain this relationship. The two are used interchangeably below */
	if (gv_target->root)
		do_upgrade = TRUE;
	/* The below logic assumes ^#t global does not have any integrity errors */
	assert(do_upgrade);	/* caller should have not invoked us otherwise */
	if (do_upgrade)
	{	/* kill ^#t("#TRHASH"), ^#t("#TNAME") and ^#t("#LABEL") first. Regenerate each again as we process ^#t(<gbl>,...) */
		csa->incr_db_trigger_cycle = TRUE; /* so that we increment csd->db_trigger_cycle at commit time.
							 * this forces concurrent processes to read upgraded triggers.
							 */
		if (JNL_WRITE_LOGICAL_RECS(csa))
		{	/* Note that the ^#t upgrade is a physical layout change. But it has no logical change (i.e. users
			 * see the same MUPIP TRIGGER -SELECT output as before). So write only a dummy LGTRIG journal
			 * record for this operation. Hence write a string that starts with a trigger comment character ";".
			 */
			assert(!gv_cur_region->read_only);
			jnl_format(JNL_LGTRIG, NULL, (mval *)&literal_trigjnlrec, 0);
		}
		/* KILL ^#t("#LABEL") unconditionally */
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
		if (0 != gvcst_data())
			gvcst_kill(TRUE);
		/* KILL ^#t("#TNAME") unconditionally and regenerate */
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME));
		if (0 != gvcst_data())
			gvcst_kill(TRUE);
		/* KILL ^#t("#TRHASH") unconditionally and regenerate */
		BUILD_HASHT_SUB_CURRKEY(LITERAL_HASHTRHASH, STRLEN(LITERAL_HASHTRHASH));
		if (0 != gvcst_data())
			gvcst_kill(TRUE);
		/* Loop through all global names for which ^#t(<gvn>) exists. The only first-level subscripts of ^#t starting
		 * with # are #TNAME and #TRHASH in collation order. So after #TRHASH we expect to find subscripts that are
		 * global names. Hence the HASHTRHASH code is placed AFTER the HASHTNAME code above.
		 */
		TREF(gd_targ_gvnh_reg) = NULL;	/* needed so op_gvorder below goes through gvcst_order (i.e. focuses only
						 * on the current region) and NOT through gvcst_spr_order (which does not
						 * apply anyways in the case of ^#t).
						 */
		nullbyte[0] = '\0';
		trig_protected_mval_push_count = 0;
		INCR_AND_PUSH_MV_STENT(gvname); /* Protect gvname from garbage collection */
		do
		{
			op_gvorder(gvname);
			if (0 == gvname->str.len)
				break;
			assert(ARRAYSIZE(trigvn) > gvname->str.len);
			memcpy(&trigvn[0], gvname->str.addr, gvname->str.len);
			gvname->str.addr = &trigvn[0];	/* point away from stringpool to avoid stp_gcol issues */
			/* Save gv_currkey->prev so it is restored before next call to op_gvorder (which cares about this field).
			 * gv_currkey->prev gets tampered with in the for loop below (e.g. BUILD_HASHT_SUB_CURRKEY macro).
			 * No need to do this for gv_currkey->end since the body of the for loop takes care of restoring it.
			 */
			gvname_prev = gv_currkey->prev;
			BUILD_HASHT_SUB_CURRKEY(gvname->str.addr, gvname->str.len);
			/* At this point, gv_currkey is ^#t(<gvn>) */
			/* Increment ^#t(<gvn>,"#CYCLE") */
			is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_hashcycle, tmpmv);
			assert(is_defined);
			tmpint4 = mval2i(tmpmv);
			tmpint4++;
			i2mval(tmpmv, tmpint4);
			gvtr_set_hasht_gblsubs((mval *)&literal_hashcycle, tmpmv);
			/* Read ^#t(<gvn>,"#COUNT") */
			is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_hashcount, tmpmv);
			if (is_defined)
			{
				tmpint4 = mval2i(tmpmv);
				count = tmpint4;
				/* Get ^#t(<gvn>,"#LABEL"), error out for invalid values. Upgrade disallowed for label 1 triggers */
				is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_hashlabel, tmpmv);
				assert(is_defined);
				currlabel = mval2i(tmpmv);
				if ((V19_HASHT_GBL_LABEL_INT >= currlabel) || (HASHT_GBL_CURLABEL_INT <= currlabel))
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_TRIGUPBADLABEL, 6, currlabel,
							HASHT_GBL_CURLABEL_INT, gvname->str.len, gvname->str.addr,
							REG_LEN_STR(reg));
				/* Set ^#t(<gvn>,"#LABEL")=HASHT_GBL_CURLABEL */
				gvtr_set_hasht_gblsubs((mval *)&literal_hashlabel, (mval *)&literal_curlabel);
			} else
				count = 0;
			/* Kill ^#t(<gvn>,"#TRHASH") unconditionally and regenerate */
			gvtr_kill_hasht_gblsubs((mval *)&literal_hashtrhash, TRUE);
			/* At this point, gv_currkey is ^#t(<gvn>) */
			for (i = 1; i <= count; i++)
			{
				/* At this point, gv_currkey is ^#t(<gvn>) */
				curend = gv_currkey->end; /* note gv_currkey->end before changing it so we can restore it later */
				assert(KEY_DELIMITER == gv_currkey->base[curend]);
				assert(gv_target->gd_csa == cs_addrs);
				i2mval(tmpmv, i);
				COPY_SUBS_TO_GVCURRKEY(tmpmv, gv_cur_region, gv_currkey, was_null, is_null);
				/* At this point, gv_currkey is ^#t(<gvn>,i) */
				/* Compute new LHASH and BHASH hash values.
				 *	LHASH uses : GVSUBS,                        XECUTE
				 *	BHASH uses : GVSUBS, DELIM, ZDELIM, PIECES, XECUTE
				 * So reach each of these pieces and compute hash along the way.
				 */
				STR_PHASH_INIT(hash_state, hash_totlen);
				STR_PHASH_PROCESS(hash_state, hash_totlen, gvname->str.addr, gvname->str.len);
				STR_PHASH_PROCESS(hash_state, hash_totlen, nullbyte, 1);
				/* Read in ^#t(<gvn>,i,"GVSUBS") */
				is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_gvsubs, tmpmv);
				if (is_defined)
				{
					STR_PHASH_PROCESS(hash_state, hash_totlen, tmpmval.str.addr, tmpmval.str.len);
					STR_PHASH_PROCESS(hash_state, hash_totlen, nullbyte, 1);
				}
				/* Copy over SET hash state (2-tuple <state,totlen>) to KILL hash state before adding
				 * the PIECES, DELIM, ZDELIM portions (those are only part of the SET hash).
				 */
				kill_hash_state = hash_state;
				kill_hash_totlen = hash_totlen;
				/* Read in ^#t(<gvn>,i,"PIECES") */
				is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_pieces, tmpmv);
				if (is_defined)
				{
					STR_PHASH_PROCESS(hash_state, hash_totlen, tmpmval.str.addr, tmpmval.str.len);
					STR_PHASH_PROCESS(hash_state, hash_totlen, nullbyte, 1);
				}
				/* Read in ^#t(<gvn>,i,"DELIM") */
				is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_delim, tmpmv);
				if (is_defined)
				{
					STR_PHASH_PROCESS(hash_state, hash_totlen, tmpmval.str.addr, tmpmval.str.len);
					STR_PHASH_PROCESS(hash_state, hash_totlen, nullbyte, 1);
				}
				/* Read in ^#t(<gvn>,i,"ZDELIM") */
				is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_zdelim, tmpmv);
				if (is_defined)
				{
					STR_PHASH_PROCESS(hash_state, hash_totlen, tmpmval.str.addr, tmpmval.str.len);
					STR_PHASH_PROCESS(hash_state, hash_totlen, nullbyte, 1);
				}
				/* Read in ^#t(<gvn>,i,"XECUTE").
				 * Note: The XECUTE portion of the trigger definition is used in SET and KILL hash.
				 * But since we have started maintaining "hash_state" and "kill_hash_state" separately
				 * (due to PIECES, DELIM, ZDELIM) we need to update the hash for both using same input string.
				 */
				is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_xecute, tmpmv);
				if (is_defined)
				{
					STR_PHASH_PROCESS(hash_state, hash_totlen, tmpmval.str.addr, tmpmval.str.len);
					STR_PHASH_PROCESS(kill_hash_state, kill_hash_totlen, tmpmval.str.addr, tmpmval.str.len);
				} else
				{	/* Multi-record XECUTE string */
					/* At this point, gv_currkey is ^#t(<gvn>,i) */
					xecute_curend = gv_currkey->end; /* note gv_currkey->end so we can restore it later */
					assert(KEY_DELIMITER == gv_currkey->base[xecute_curend]);
					tmpmv2 = (mval *)&literal_xecute;
					COPY_SUBS_TO_GVCURRKEY(tmpmv2, gv_cur_region, gv_currkey, was_null, is_null);
					xecutei = 1;
					do
					{
						i2mval(&xecuteimval, xecutei);
						is_defined = gvtr_get_hasht_gblsubs(&xecuteimval, tmpmv);
						if (!is_defined)
							break;
						STR_PHASH_PROCESS(hash_state, hash_totlen, tmpmval.str.addr, tmpmval.str.len);
						STR_PHASH_PROCESS(kill_hash_state, kill_hash_totlen,
									tmpmval.str.addr, tmpmval.str.len);
						xecutei++;
					} while (TRUE);
					/* Restore gv_currkey to ^#t(<gvn>,i) */
					gv_currkey->end = xecute_curend;
					gv_currkey->base[xecute_curend] = KEY_DELIMITER;
				}
				STR_PHASH_RESULT(hash_state, hash_totlen, hash_code);
				STR_PHASH_RESULT(kill_hash_state, kill_hash_totlen, kill_hash_code);
				/* Set ^#t(<gvn>,i,"LHASH") */
				MV_FORCE_UMVAL(tmpmv, kill_hash_code);
				gvtr_set_hasht_gblsubs((mval *)&literal_lhash, tmpmv);
				/* Set ^#t(<gvn>,i,"BHASH") */
				MV_FORCE_UMVAL(tmpmv, hash_code);
				gvtr_set_hasht_gblsubs((mval *)&literal_bhash, tmpmv);
				/* Read in ^#t(<gvn>,i,"TRIGNAME") to determine if #SEQNUM/#TNCOUNT needs to be maintained */
				is_defined = gvtr_get_hasht_gblsubs((mval *)&literal_trigname, tmpmv);
				assert(is_defined);
				assert('#' == tmpmval.str.addr[tmpmval.str.len - 1]);
				tmpmval.str.len--;
				if ((tmpmval.str.len <= ARRAYSIZE(name_and_index)) &&
						(NULL != (ptr = memchr(tmpmval.str.addr, '#', tmpmval.str.len))))
				{	/* Auto-generated name. Need to maintain #SEQNUM/#TNCOUNT */
					/* Take copy of trigger name into non-stringpool location to avoid stp_gcol issues */
					trigname_len = ptr - tmpmval.str.addr;
					ptr++;
					name_index_len = (tmpmval.str.addr + tmpmval.str.len) - ptr;
					assert(ARRAYSIZE(name_and_index) >= (trigname_len + 1 + name_index_len));
					trigname = &name_and_index[0];
					trigindex = ptr;
					memcpy(trigname, tmpmval.str.addr, tmpmval.str.len);
					A2I(ptr, ptr + name_index_len, trig_seq_num);
					/* At this point, gv_currkey is ^#t(<gvn>,i) */
					/* $get(^#t("#TNAME",<trigger name>,"#SEQNUM")) */
					BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME),
						trigname, trigname_len, LITERAL_HASHSEQNUM, STR_LIT_LEN(LITERAL_HASHSEQNUM));
					seq_num = gvcst_get(tmpmv) ? mval2i(tmpmv) : 0;
					if (trig_seq_num > seq_num)
					{	/* Set ^#t("#TNAME",<trigger name>,"#SEQNUM") = trig_seq_num */
						SET_TRIGGER_GLOBAL_SUB_SUB_SUB_STR(LITERAL_HASHTNAME,
							STR_LIT_LEN(LITERAL_HASHTNAME), trigname, trigname_len,
							LITERAL_HASHSEQNUM, STR_LIT_LEN(LITERAL_HASHSEQNUM),
							trigindex, name_index_len, result);
						assert(PUT_SUCCESS == result);
					}
					/* set ^#t("#TNAME",<trigger name>,"#TNCOUNT")++ */
					BUILD_HASHT_SUB_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME),
						trigname, trigname_len, LITERAL_HASHTNCOUNT, STR_LIT_LEN(LITERAL_HASHTNCOUNT));
					tncount = gvcst_get(tmpmv) ? mval2i(tmpmv) + 1 : 1;
					i2mval(tmpmv, tncount);
					SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME),
						trigname, trigname_len, LITERAL_HASHTNCOUNT, STR_LIT_LEN(LITERAL_HASHTNCOUNT),
						tmpmval, result);
					trigname_len += 1 + name_index_len; /* in preparation for ^#t("#TNAME") set below */
					assert(PUT_SUCCESS == result);
					BUILD_HASHT_SUB_CURRKEY(gvname->str.addr, gvname->str.len);
					/* At this point, gv_currkey is ^#t(<gvn>) */
				} else
				{
					/* Take copy of trigger name into non-stringpool location to avoid stp_gcol issues */
					trigname = &name_and_index[0];  /* in preparation for ^#t("#TNAME") set below */
					trigname_len = MIN(tmpmval.str.len, ARRAYSIZE(name_and_index));
					assert(ARRAYSIZE(name_and_index) >= trigname_len);
					memcpy(trigname, tmpmval.str.addr, trigname_len);
					/* Restore gv_currkey to what it was at beginning of for loop iteration */
					gv_currkey->end = curend;
					gv_currkey->base[curend] = KEY_DELIMITER;
				}
				/* At this point, gv_currkey is ^#t(<gvn>) */
				if (kill_hash_code != hash_code)
					gvtr_set_hashtrhash(gvname->str.addr, gvname->str.len, kill_hash_code, i);
				/* Set ^#t(<gvn>,"#TRHASH",hash_code,i) */
				gvtr_set_hashtrhash(gvname->str.addr, gvname->str.len, hash_code, i);
				/* Set ^#t("#TNAME",<trigname>)=<gvn>_$c(0)_<trigindx> */
				/* The upgrade assumes that the region does not contain two triggers with the same name.
				 * V62000 and before could potentially have this out of design case. Once implemented
				 * the trigger integrity check will warn users of this edge case */
				ptr = &trigvn[gvname->str.len];
				*ptr++ = '\0';
				ilen = 0;
				I2A(ptr, ilen, i);
				ptr += ilen;
				assert(ptr <= ARRAYTOP(trigvn));
				SET_TRIGGER_GLOBAL_SUB_SUB_STR(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME),
					trigname, trigname_len, trigvn, ptr - gvname->str.addr, result);
				assert(PUT_SUCCESS == result);
				BUILD_HASHT_SUB_CURRKEY(gvname->str.addr, gvname->str.len);
				/* At this point, gv_currkey is ^#t(<gvn>) */
			}
			/* At this point, gv_currkey is ^#t(<gvn>) i.e. gv_currkey->end is correct but gv_currkey->prev
			 * might have been tampered with. Restore it to proper value first.
			 */
			 gv_currkey->prev = gvname_prev;
			gvname->mvtype = 0; /* can now be garbage collected in the next iteration */
		} while (TRUE);
	}
	op_tcommit();
	REVERT; /* remove our condition handler */
	DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE;)
	if (csa->hold_onto_crit)
	{
		assert(csa->now_crit);	/* we should still hold crit */
		/* Switch to new journal file and cut previous link if we did ^#t upgrade on a journaled region */
		if (do_upgrade && JNL_WRITE_LOGICAL_RECS(csa))
		{
			sts = set_jnl_file_close();
			assert(SS_NORMAL == sts);	/* because we should have done jnl_ensure_open already
							 * in which case set_jnl_file_close has no way of erroring out.
							 */
			sts = jnl_file_open_switch(reg, 0);
			if (sts)
			{
				jpc = csa->jnl;
				if (NOJNL != jpc->channel)
					JNL_FD_CLOSE(jpc->channel, close_res);	/* sets jpc->channel to NOJNL */
				assert(NOJNL == jpc->channel);
				jnl_send_oper(jpc, sts);
			}
		}
		csa->hold_onto_crit = FALSE;
	} else
		grab_crit(reg);
	csa->hdr->hasht_upgrade_needed = FALSE;
	rel_crit(reg);
	return;	/* if any errors happen during upgrade (e.g. TRANS2BIG), we will not reach here */
}
#endif /* GTM_TRIGGER */
