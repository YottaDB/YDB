/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdsblkops.h"
#include "gdskill.h"
#include "gdscc.h"
#include "copy.h"
#include "interlock.h"
#include "muextr.h"

/* Include prototypes */
#include "t_end.h"
#include "t_retry.h"
#include "mupip_size.h"
#include "util.h"
#include "t_begin.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search prototype */
#include "gvcst_bmp_mark_free.h"
#include "gvcst_kill_sort.h"
#include "gtmmsg.h"
#include "add_inter.h"
#include "t_abort.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "memcoherency.h"

#include "gtm_time.h"
#include "mvalconv.h"
#include "t_qread.h"
#include "longset.h"            /* needed for cws_insert.h */
#include "hashtab_int4.h"
#include "cws_insert.h"
#include "min_max.h"
#include <math.h>

error_def(ERR_GBLNOEXIST);
error_def(ERR_MUSIZEFAIL);
error_def(ERR_MUSIZEINVARG);

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gv_namehead		*gv_target;
GBLREF	unsigned int		t_tries;
GBLREF	int4			process_id;
GBLREF  inctn_opcode_t          inctn_opcode;
GBLREF  unsigned char           rdfail_detail;

GBLDEF	uint4			total_recs;
GBLDEF	uint4			total_scans;
GBLDEF	int			targ_levl;
GBLDEF	INTPTR_T		saveoff[MAX_BT_DEPTH + 1];

#define ANY_ROOT_LEVL		(MAX_BT_DEPTH + 5)	/* overload invalid level value */
#define MAX_RECS_PER_BLK	65535
#define	MAX_SCANS		200000000		/* catch infinite loops */

#define GET_AND_CHECK_RECLEN(status, nRecLen, pRec, pTop)		\
{									\
	status = cdb_sc_normal;						\
	GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);		\
	if (nRecLen == 0)						\
		status = cdb_sc_badoffset;				\
	else if (pRec + nRecLen > pTop)					\
		status = cdb_sc_blklenerr;				\
}

#define GET_AND_CHECK_LEVL(status, nLevl, desired_levl, pBlkBase)	\
{									\
	status = cdb_sc_normal;						\
	nLevl = ((blk_hdr_ptr_t)pBlkBase)->levl;			\
	if (MAX_BT_DEPTH < (int)nLevl)					\
		status = cdb_sc_maxlvl;					\
	else if (ANY_ROOT_LEVL == desired_levl)				\
	{								\
		if (0 == (int)nLevl)					\
			status = cdb_sc_badlvl;				\
	} else if (desired_levl !=(int)nLevl)				\
		status = cdb_sc_badlvl;					\
}

#define BLK_LOOP(i, pRec, pBlkBase, pTop, nRecLen)	for (pTop = pBlkBase + ((blk_hdr_ptr_t)pBlkBase)->bsiz,			\
							     pRec = pBlkBase + SIZEOF(blk_hdr),	i = 0;				\
							     i < MAX_RECS_PER_BLK && (pRec != pTop); i++, pRec += nRecLen)

enum cdb_sc dfs(int lvl, sm_uc_ptr_t pBlkBase, boolean_t endtree, boolean_t skiprecs);
enum cdb_sc read_block(block_id nBlkId, sm_uc_ptr_t *pBlkBase_ptr, int *nLevl_ptr, int desired_levl);

int4 mu_size_scan(mval *gn, int4 level)
{
	enum cdb_sc		status;
	trans_num		ret_tn;
	int			k, h;
	boolean_t		verify_reads;
	boolean_t		tn_aborted;
	boolean_t		equal;
	unsigned int		lcl_t_tries;
	block_id		nBlkId;
	int4			nLevl;
	sm_uc_ptr_t		pBlkBase;
	int			i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	inctn_opcode = inctn_invalid_op;
	op_gvname(VARLSTCNT(1) gn);
	if (0 == gv_target->root)
	{	/* Global does not exist (online rollback). Not an error. */
		gtm_putmsg(VARLSTCNT(4) ERR_GBLNOEXIST, 2, gn->str.len, gn->str.addr);
		return EXIT_NRM;
	}
	gv_target->alt_hist->depth = MAX_BT_DEPTH;	/* initialize: don't copy to saveoff if restart before a single success */
	total_recs = total_scans = 0;
	for (k = 0; k <= MAX_BT_DEPTH; k++)
	{
		saveoff[k] = 0;
		gv_target->hist.h[k].cr = NULL;		/* initialize for optimization in read_block which bumps cr refer bits */
	}
	targ_levl = 0;
	/* Read the root block and convert negative levels to positive. Negative levels are defined to be counted from root with
	 * -1 being children of root */
	t_begin(ERR_MUSIZEFAIL, 0);
	for(;;)
	{	/* retry loop */
		status = read_block(gv_target->root, &pBlkBase, &nLevl, ANY_ROOT_LEVL);
		if (cdb_sc_normal != status)
		{
			assert(CDB_STAGNATE > t_tries);
			t_retry(status);
			continue;
		}
		memcpy(&gv_target->hist.h[0], &gv_target->hist.h[nLevl], SIZEOF(srch_blk_status));
		gv_target->hist.h[1].blk_num = 0;
		if ((trans_num)0 == t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED)){
			lcl_t_tries = TREF(prev_t_tries);
			ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
			if (tn_aborted)
			{	/* Global does not exist (online rollback). Not an error. */
				gtm_putmsg(VARLSTCNT(4) ERR_GBLNOEXIST, 2, gn->str.len, gn->str.addr);
				return EXIT_NRM;
			}
			continue;
		}
		break;
	}
	if (level < 0)
		level += nLevl;
	if (level < 0 || nLevl < level)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.LEVEL"));
		return EXIT_ERR;
	}
	targ_levl = level;
	/* Run the dfs down to targ_levl to count records and blocks. Validate every path from root to blocks at targ_levl */
	t_begin(ERR_MUSIZEFAIL, 0);
	for (;;)
	{	/* retry loop. note that multiple successful read transactions can occur within a single iteration */
		nBlkId = gv_target->root;
		nLevl = ANY_ROOT_LEVL;
		status = read_block(nBlkId, &pBlkBase, &nLevl, ANY_ROOT_LEVL);
		if (cdb_sc_normal == status)
			status = dfs(nLevl, pBlkBase, TRUE, TRUE);
		if (cdb_sc_endtree != status)
		{
			assert(cdb_sc_normal != status);	/* should have continued recursive search */
			if (cdb_sc_restarted != status)
				t_retry(status);
			lcl_t_tries = TREF(prev_t_tries);
			ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
			if (tn_aborted)
			{	/* Global does not exist (online rollback). Not an error. */
				gtm_putmsg(VARLSTCNT(4) ERR_GBLNOEXIST, 2, gn->str.len, gn->str.addr);
				return EXIT_NRM;
			}
			/* update saveoff */
			if (gv_target->alt_hist->depth < MAX_BT_DEPTH)
			{
				for (i = targ_levl; i <= gv_target->alt_hist->depth; i++)
					saveoff[i] = gv_target->alt_hist->h[i - targ_levl].curr_rec.offset;
			}
			continue;
		}
		break;
	}
	util_out_print("Level          Blocks          Records", FLUSH);
	util_out_print("!5UL !15UL !16UL", FLUSH, level, total_scans, total_recs);
	if (mu_ctrlc_occurred || mu_ctrly_occurred)
		return EXIT_ERR;
	return EXIT_NRM;
}

enum cdb_sc dfs(int lvl, sm_uc_ptr_t pBlkBase, boolean_t endtree, boolean_t skiprecs)
{
	int				incr_recs = 0, incr_scans = 0;
	boolean_t			first_iter, last_rec, next_endtree, next_skiprecs;
	trans_num			ret_tn;
	sm_uc_ptr_t			pVal, pTop, pRec, child_pBlkBase;
	int4				child_nLevl;
	unsigned short			nRecLen;
	int4				i;
	enum cdb_sc			status;
	block_id			nBlkId;
	int				curroff;
	cache_rec_ptr_t			cr;
	srch_hist			sibhist;

	assert(total_scans < MAX_SCANS);
	if (lvl == targ_levl)
	{	/* reached the bottom. count records in this block and validate */
		BLK_LOOP(i, pRec, pBlkBase, pTop, nRecLen)
		{
			GET_AND_CHECK_RECLEN(status, nRecLen, pRec, pTop);
			if (cdb_sc_normal != status)
			{
				assert(CDB_STAGNATE > t_tries);
				return status;
			}
		}
		incr_recs = i;
		incr_scans = 1;
	} else if (lvl > targ_levl)
	{	/* visit each child */
		first_iter = TRUE;
		gv_target->hist.h[lvl - targ_levl].curr_rec.offset = saveoff[lvl];
		BLK_LOOP(i, pRec, pBlkBase, pTop, nRecLen)
		{
			GET_AND_CHECK_RECLEN(status, nRecLen, pRec, pTop);
			if (cdb_sc_normal != status)
			{
				assert(CDB_STAGNATE > t_tries);
				return status;
			}
			curroff = (INTPTR_T)(pRec - pBlkBase);
			gv_target->hist.h[lvl - targ_levl].curr_rec.offset = curroff;
			if (skiprecs && (curroff < saveoff[lvl]))
				continue;	/* skip these guys, we've already counted over there */
			pVal = pRec + nRecLen - SIZEOF(block_id);
			GET_LONG(nBlkId, pVal);
			status = read_block(nBlkId, &child_pBlkBase, &child_nLevl, lvl - 1);
			if (status != cdb_sc_normal)
			{
				assert(CDB_STAGNATE > t_tries);
				return status;
			}
			last_rec = ((pRec + nRecLen) == pTop);
			first_iter = (curroff == saveoff[lvl]);
			next_endtree = endtree && last_rec;
			next_skiprecs = skiprecs && first_iter;
			status = dfs(lvl - 1, child_pBlkBase, next_endtree, next_skiprecs);
			if (status != cdb_sc_normal)
				return status;
			first_iter = FALSE;
		}
	}
	/* make sure we can really move on from this block to the next: validate all blocks down to here */
	memcpy(&sibhist.h[0], &gv_target->hist.h[lvl], SIZEOF(srch_blk_status) * (gv_target->hist.depth - lvl + 2));
	if ((trans_num)0 == (ret_tn = t_end(&sibhist, NULL, TN_NOT_SPECIFIED)))
		return cdb_sc_restarted;
	total_recs += incr_recs;
	total_scans += incr_scans;
	if (endtree || mu_ctrlc_occurred || mu_ctrly_occurred)
		return cdb_sc_endtree;	/* note: usage slightly different from elsewhere, since we've already done validation */
	assert(lvl >= targ_levl);
	memcpy(gv_target->alt_hist, &gv_target->hist, SIZEOF(srch_hist)); /* take a copy of most recently validated history */
	gv_target->alt_hist->h[lvl - targ_levl + 1].curr_rec.offset++; /* don't recount the previously validated/counted path */
	for (i = 0; i <= (lvl - targ_levl); i++)
		gv_target->alt_hist->h[i].curr_rec.offset = 0;
	/* Free up the cache record for the block we're done with. I.e. mark it available to whoever makes the next pass through
	 * db_csh_getn.
	 */
	cr = gv_target->alt_hist->h[lvl - targ_levl].cr;
	assert((NULL != cr) || (dba_mm == cs_data->acc_meth));
	if (NULL != cr)
		cr->refer = FALSE;
	gv_target->clue.end = 1;	/* to set start_tn to earliest tn in history */
	t_begin(ERR_MUSIZEFAIL, 0);	/* start a new transaction and continue recursive search */
	gv_target->clue.end = 0;
	return cdb_sc_normal;
}

enum cdb_sc read_block(block_id nBlkId, sm_uc_ptr_t *pBlkBase_ptr, int *nLevl_ptr, int desired_levl)
{
	sm_uc_ptr_t			pBlkBase;
	register srch_blk_status	*pCurr;
	register srch_hist		*pTargHist;
	unsigned char			nLevl;
	cache_rec_ptr_t			cr;
	int				cycle;
	trans_num			tn;
	enum cdb_sc			status;
	int				i;

	pTargHist = &gv_target->hist;
	tn = cs_addrs->ti->curr_tn;
	if ((dba_mm != cs_data->acc_meth) && (ANY_ROOT_LEVL != desired_levl))
	{	/* avoid reading into a cache record we're already using in this transaction. prevents self-induced restarts. */
		for (i = 0; i <= MAX_BT_DEPTH; i++)
			if (pTargHist->h[i].blk_num && (NULL != (cr = pTargHist->h[i].cr)))	/* note: assignment */
				cr->refer = TRUE;
	}
#	ifdef DEBUG
	/* restart occasionally */
	if ((nBlkId % ((process_id % 25) + 25) == 0) && (t_tries == 0))
		return cdb_sc_blkmod;
#	endif
	if (NULL == (pBlkBase = t_qread(nBlkId, (sm_int_ptr_t)&cycle, &cr)))
		return (enum cdb_sc)rdfail_detail;
	GET_AND_CHECK_LEVL(status, nLevl, desired_levl, pBlkBase);
	if (cdb_sc_normal != status)
	{
		assert(CDB_STAGNATE > t_tries);
		return status;
	}
	pCurr = &pTargHist->h[nLevl - targ_levl];	/* No blocks to read beneath input level */
	if (ANY_ROOT_LEVL == desired_levl)
	{
		if (nLevl < targ_levl)
			pCurr = &pTargHist->h[0];
		(pCurr + 1)->blk_num = 0;
		pTargHist->depth = (int)nLevl;
	}
	pCurr->cse = NULL;
	pCurr->blk_num = nBlkId;
	pCurr->buffaddr = pBlkBase;
	pCurr->tn = tn;
	pCurr->cycle = cycle;
	pCurr->cr = cr;
	*nLevl_ptr = nLevl;
	*pBlkBase_ptr = pBlkBase;
	return cdb_sc_normal;
}
