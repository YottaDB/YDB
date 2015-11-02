/****************************************************************
 *								*
 *	Copyright 2004, 2012 Fidelity Information Services, Inc	*
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

void		db_auto_upgrade(gd_region *reg);
#ifdef VMS
void		db_init(gd_region *reg, sgmnt_data_ptr_t tsd);
#else
void		db_init(gd_region *reg);
void		gvcst_redo_root_search(void);
#endif
gd_region	*dbfilopn (gd_region *reg);
void		dbsecspc(gd_region *reg, sgmnt_data_ptr_t csd, gtm_uint64_t *sec_size);
mint		gvcst_data(void);
enum cdb_sc	gvcst_dataget(mint *dollar_data, mval *val);
bool		gvcst_gblmod(mval *v);
boolean_t	gvcst_get(mval *v);
void		gvcst_incr(mval *increment, mval *result);
void		gvcst_init(gd_region *greg);
void		gvcst_kill(bool do_subtree);
enum cdb_sc	gvcst_lftsib(srch_hist *full_hist);
bool		gvcst_order(void);
void		gvcst_put(mval *v);
bool		gvcst_query(void);
boolean_t	gvcst_queryget(mval *val);
void		gvcst_root_search(void);
enum cdb_sc	gvcst_rtsib(srch_hist *full_hist, int level);
enum cdb_sc	gvcst_search(gv_key *pKey, srch_hist *pHist);
enum cdb_sc	gvcst_search_blk(gv_key *pKey, srch_blk_status *pStat);
enum cdb_sc	gvcst_search_tail(gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey);
void		gvcst_tp_init(gd_region *);
bool		gvcst_zprevious(void);

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
