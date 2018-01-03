/****************************************************************
 *								*
 * Copyright (c) 2004-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* collection of prototypes of gvcst_* modules that need a bare minimum of mdef.h & gdsfhead.h to be already included */

#ifndef GVCST_PROTOS_H_INCLUDED
#define GVCST_PROTOS_H_INCLUDED

#define		OK_TO_BYPASS_FALSE	FALSE
#define		OK_TO_BYPASS_TRUE	TRUE
#define		DO_STATSDB_INIT_FALSE	FALSE
#define		DO_STATSDB_INIT_TRUE	TRUE

void		db_auto_upgrade(gd_region *reg);
int		db_init(gd_region *reg, boolean_t ok_to_bypass);
void		db_init_err_cleanup(boolean_t retry_dbinit);
void		gvcst_redo_root_search(void);
gd_region	*dbfilopn (gd_region *reg);
void		dbsecspc(gd_region *reg, sgmnt_data_ptr_t csd, gtm_uint64_t *sec_size);
unsigned char	gvcst_cre_autoDB(gd_region *reg);
mint		gvcst_data(void);
mint		gvcst_data2(void);
enum cdb_sc	gvcst_dataget(mint *dollar_data, mval *val);
enum cdb_sc	gvcst_dataget2(mint *dollar_data, mval *val, unsigned char *sn_ptr);
void		gvcst_deferred_init_statsDB(void);
void		gvcst_set_statsdb_fname(sgmnt_data_ptr_t csd, gd_region *baseDBreg, char *statsdb_fname, uint4 *statsdb_fname_len);
boolean_t	gvcst_gblmod(mval *v);
boolean_t	gvcst_get(mval *v);
boolean_t	gvcst_get2(mval *v, unsigned char *sn_ptr);
void		gvcst_incr(mval *increment, mval *result);
void		gvcst_init(gd_region *greg, gd_addr *addr);
void		gvcst_init_statsDB(gd_region *baseDBreg, boolean_t do_statsdb_init);
void		gvcst_kill(boolean_t do_subtree);
void		gvcst_kill2(boolean_t do_subtree, boolean_t *span_status, boolean_t killing_chunks);
enum cdb_sc	gvcst_lftsib(srch_hist *full_hist);
boolean_t	gvcst_order(void);
boolean_t	gvcst_order2(void);
void		gvcst_put(mval *v);
void		gvcst_put2(mval *val, span_parms *parms);
boolean_t	gvcst_query(void);
boolean_t	gvcst_query2(void);
boolean_t	gvcst_queryget(mval *val);
boolean_t	gvcst_queryget2(mval *val, unsigned char *sn_ptr);
void		gvcst_remove_statsDB_linkage(gd_region *baseDBreg);
void		gvcst_remove_statsDB_linkage_all(void);
void		gvcst_remove_statsDB_linkage_wrapper(gd_region *baseDBreg, gd_region *statsDBreg);
enum cdb_sc	gvcst_root_search(boolean_t donot_restart);
enum cdb_sc	gvcst_rtsib(srch_hist *full_hist, int level);
enum cdb_sc	gvcst_search(gv_key *pKey, srch_hist *pHist);
enum cdb_sc 	gvcst_search_blk(gv_key *pKey, srch_blk_status *pStat);
enum cdb_sc	gvcst_search_blk_expand_prevkey(gv_key *pKey, srch_blk_status *pStat);
enum cdb_sc	gvcst_search_tail(gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey);
enum cdb_sc	gvcst_search_tail_expand_prevkey(gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey);

mint		gvcst_spr_data(void);
void		gvcst_spr_kill(void);
boolean_t	gvcst_spr_order(void);
boolean_t	gvcst_spr_zprevious(void);
boolean_t	gvcst_spr_query(void);
boolean_t	gvcst_spr_queryget(mval *cumul_val);

# define INVOKE_GVCST_SPR_XXX(GVNH_REG, STATEMENT)	\
{							\
	assert(NULL != GVNH_REG->gvspan);		\
	STATEMENT;					\
}

void		gvcst_statshare_optin(void);
void		gvcst_statshare_optout(void);
void		gvcst_tp_init(gd_region *);
boolean_t	gvcst_zprevious(void);
boolean_t	gvcst_zprevious2(void);

/* gvcst_dataget and gvcst_dataget2 take the following values as input */
#define DG_GETONLY	2
#define DG_DATAONLY	3
#define DG_DATAGET	4
#define DG_GETSNDATA	5

#define	INCREMENT_GD_TARG_TN(LCL_TN)									\
{													\
	(TREF(gd_targ_tn))++;										\
	LCL_TN = TREF(gd_targ_tn);									\
	if (0 == LCL_TN)										\
	{	/* We have wrapped around after 2**64 increments. Reset gd_targ_reg_array to		\
		 * avoid incorrect matches of tn (the pre-wrapped tn with the post-wrapped tn).		\
		 */											\
	 	assert(SIZEOF(trans_num) == SIZEOF((TREF(gd_targ_reg_array))[0]));			\
		memset(TREF(gd_targ_reg_array), 0, TREF(gd_targ_reg_array_size) * SIZEOF(trans_num));	\
		LCL_TN++;										\
		TREF(gd_targ_tn) = LCL_TN;								\
	}												\
}

GBLREF	boolean_t	gv_play_duplicate_kills;

/* In case "gv_play_duplicate_kills" is TRUE, invoke "gvcst_kill" even if the GVT does not exist. This is so we record
 * in the jnl file and the db (curr_tn wise) that such a KILL occurred. Also do this only if the update is an explicit
 * update. Else this update is not replicated so we dont have the need to write a journal record for the KILL. While at
 * this, add a few asserts that in case of backward recovery, the to-be-killed node always exists. This is because backward
 * recovery is supposed to mirror GT.M activity on the database and so should see the exact same state of the database
 * that GT.M saw. The only exception is in case of replication and the update process wrote a KILL jnl record because it
 * has to mirror the jnl state of the primary. In this case though nodeflags will have the JS_IS_DUPLICATE bit set.
 */
#define	IS_OK_TO_INVOKE_GVCST_KILL(GVT)											\
(															\
	DBG_ASSERT(!jgbl.forw_phase_recovery || gv_play_duplicate_kills GTMTRIG_ONLY(&& IS_EXPLICIT_UPDATE_NOASSERT))	\
	DBG_ASSERT(!jgbl.forw_phase_recovery || gv_target->root || (JS_IS_DUPLICATE & jgbl.mur_jrec_nodeflags)		\
			|| jgbl.mur_options_forward)									\
	(GVT->root || (gv_play_duplicate_kills GTMTRIG_ONLY(&& IS_EXPLICIT_UPDATE_NOASSERT)))				\
)

#endif
