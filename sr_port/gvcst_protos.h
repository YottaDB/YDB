/****************************************************************
 *								*
 *	Copyright 2004, 2011 Fidelity Information Services, Inc	*
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
void		db_init(gd_region *reg, sgmnt_data_ptr_t tsd);
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

#endif
