/****************************************************************
 *								*
 * Copyright (c) 2012-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "gdsblkops.h"
#include "gdskill.h"
#include "gdscc.h"
#include "copy.h"
#include "interlock.h"
#include "muextr.h"
#include "mupint.h"
/* Include prototypes */
#include "t_end.h"
#include "t_retry.h"
#include "collseq.h"
#include "mu_getkey.h"
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
#include "change_reg.h"
#include "gtm_time.h"
#include "mvalconv.h"
#include "t_qread.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "tp.h"
#include <math.h>

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	gv_namehead		*gv_target;
GBLREF	uint4			process_id;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	unsigned char		rdfail_detail;
GBLREF	uint4			mu_int_adj[];
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t		mu_subsc;
GBLREF	block_id		mu_int_adj_prev[MAX_BT_DEPTH + 1];
GBLREF	boolean_t		mu_key;
GBLREF	boolean_t		null_coll_key;
GBLREF	gv_key			*mu_start_key;
GBLREF	gv_key			*mu_end_key;

STATICDEF	uint4		mu_size_cumulative[MAX_BT_DEPTH + 1][CUMULATIVE_TYPE_MAX];
STATICDEF	int		targ_levl;
STATICDEF	INTPTR_T	saveoff[MAX_BT_DEPTH + 1];

STATICFNDCL enum cdb_sc dfs(int lvl, sm_uc_ptr_t pBlkBase, boolean_t endtree, boolean_t skiprecs);
STATICFNDCL enum cdb_sc read_block(block_id nBlkId, sm_uc_ptr_t *pBlkBase_ptr, int *nLevl_ptr, int desired_levl);

#define	ANY_ROOT_LEVL		(MAX_BT_DEPTH + 5)	/* overload invalid level value */
#define	MAX_SCANS		200000000		/* catch infinite loops */

/* No MBSTART and MBEND below as the macro uses a continue command */
#define CHECK_KEY_RANGE(MUSZ_START_KEY, MUSZ_END_KEY, BUFF, RCNT, MUSZ_RANGE_DONE)		\
{												\
	int4	CMP_KEY;									\
												\
	CMP_KEY = memcmp(BUFF, MUSZ_START_KEY->base, MUSZ_START_KEY->end + 1);			\
	if (MUSZ_END_KEY)									\
	{											\
		if (memcmp(BUFF, MUSZ_END_KEY->base, MUSZ_END_KEY->end + 1) > 0)		\
			MUSZ_RANGE_DONE = TRUE;							\
	} else if (0 < CMP_KEY)									\
		MUSZ_RANGE_DONE = TRUE;								\
	if (0 > CMP_KEY)									\
	{											\
		RCNT--;										\
		continue;									\
	}											\
}

int4 mu_size_scan(glist *gl_ptr, int4 level)
{
	block_id		nBlkId;
	boolean_t		equal, tn_aborted, verify_reads, retry=FALSE;
	enum cdb_sc		status;
	trans_num		ret_tn;
	int			h, i, k;
	int4			nLevl;
	sm_uc_ptr_t		pBlkBase;
	unsigned int		lcl_t_tries;
	unsigned char		mu_size_root_lvl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	inctn_opcode = inctn_invalid_op;
	memset(mu_size_cumulative,0, SIZEOF(mu_size_cumulative));
	memset(mu_int_adj, 0, SIZEOF(int4) * (MAX_BT_DEPTH + 1));
	memset(mu_int_adj_prev, 0, SIZEOF(mu_int_adj_prev));
	/* set gv_target/gv_currkey/gv_cur_region/cs_addrs/cs_data to correspond to <globalname,reg> in gl_ptr */
	DO_OP_GVNAME(gl_ptr);
	if (0 == gv_target->root)
	{	/* Global does not exist (online rollback). Not an error. */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_GBLNOEXIST, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
		return EXIT_NRM;
	}
	gv_target->alt_hist->depth = MAX_BT_DEPTH;	/* initialize: don't copy to saveoff if restart before a single success */
	for (k = 0; k <= MAX_BT_DEPTH; k++)
	{
		saveoff[k] = 0;
		gv_target->hist.h[k].cr = NULL;		/* initialize for optimization in read_block which bumps cr refer bits */
	}
	if (MUKEY_NULLSUBS == mu_key)
		CHECK_COLL_KEY(gl_ptr, null_coll_key);
	targ_levl = 0;
	/* Read the root block and convert negative levels to positive. Negative levels are defined to be counted from root with
	 * -1 identifying the children of root
	 */
	t_begin(ERR_MUSIZEFAIL, 0);
	for(;;)
	{	/* retry loop */
		status = read_block(gv_target->root, &pBlkBase, &nLevl, ANY_ROOT_LEVL);
		if (cdb_sc_normal != status)
		{
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
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_GBLNOEXIST, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
				return EXIT_NRM;
			}
			continue;
		}
		break;
	}
	mu_size_root_lvl =  nLevl;
	if (level < 0)
		level += nLevl;
	if (level < 0 || nLevl < level)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUSIZEINVARG, 2, LEN_AND_LIT("HEURISTIC.LEVEL"));
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
		{
			if (!retry)
				CHECK_ADJACENCY(nBlkId, nLevl, mu_int_adj[nLevl]);
			status = dfs(nLevl, pBlkBase, TRUE, TRUE);
		}
		if (cdb_sc_endtree != status)
		{
			assert(cdb_sc_normal != status);	/* should have continued recursive search */
			if (cdb_sc_restarted != status)
				t_retry(status);
			lcl_t_tries = TREF(prev_t_tries);
			ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
			if (tn_aborted)
			{	/* Global does not exist (online rollback). Not an error. */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_GBLNOEXIST, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
				return EXIT_NRM;
			}
			/* update saveoff */
			if (gv_target->alt_hist->depth < MAX_BT_DEPTH)
			{
				for (i = targ_levl; i <= gv_target->alt_hist->depth; i++)
					saveoff[i] = gv_target->alt_hist->h[i - targ_levl].curr_rec.offset;
			}
			retry = TRUE;
			continue;
		}
		break;
	}
	for (i = mu_size_root_lvl; i >= level; i--)
	{
		util_out_print("Level          Blocks          Records         Adjacent", FLUSH);
		util_out_print("!5UL !15UL !16UL !16UL", FLUSH, i, mu_size_cumulative[i][BLK],
							mu_size_cumulative[i][REC], mu_int_adj[i]);

	}
	if (mu_ctrlc_occurred || mu_ctrly_occurred)
		return EXIT_ERR;
	return EXIT_NRM;
}

enum cdb_sc dfs(int lvl, sm_uc_ptr_t pBlkBase, boolean_t endtree, boolean_t skiprecs)
{
	block_id			nBlkId;
	boolean_t			next_endtree, last_rec, next_skiprecs, long_blk_id;
	cache_rec_ptr_t			cr;
	enum cdb_sc			status;
	int				curroff, incr_recs = 0, incr_scans = 0;
	int4				child_nLevl, i, rCnt;
	sm_uc_ptr_t			pTop, pRec, child_pBlkBase;
	srch_hist			sibhist;
	unsigned short			nRecLen;
	boolean_t			musz_range_done = FALSE;
	int				bstar_rec_sz;
	unsigned char			buff[MAX_KEY_SZ + 1];

	assert(mu_size_cumulative[lvl][BLK] < MAX_SCANS);
	long_blk_id = IS_64_BLK_ID(pBlkBase);
	if (lvl == targ_levl)
	{	/* reached the bottom. count records in this block and validate */
		BLK_LOOP(rCnt, pRec, pBlkBase, pTop, nRecLen, musz_range_done)
		{
			GET_AND_CHECK_RECLEN(status, nRecLen, pRec, pTop, nBlkId, long_blk_id);
			RETURN_IF_ABNORMAL_STATUS(status);
			assert((MAX_BT_DEPTH + 1) > lvl);	/* this assert ensures that the CHECK_ADJACENCY macro
			 					 * does not overrun the boundaries of the mu_int_adj array.
								 */
			if (lvl)
			{
				CHECK_ADJACENCY(nBlkId, lvl -1, mu_int_adj[lvl - 1]);
				if (BSTAR_REC_SIZE == (((rec_hdr *)pRec)->rsiz))
				{	/* Found a star record. In this case we cannot invoke the GET_KEY_CPY_BUFF macro
					 * as that expects a valid key (which a star record does not contain). Treat the
					 * star record as if it is within any specified range (i.e. follow the logic as if
					 * "musz_range_done" is FALSE.
					 */
					continue;
				}
			}
			GET_KEY_CPY_BUFF(pRec, nRecLen, buff, status);
			RETURN_IF_ABNORMAL_STATUS(status);
			if (mu_subsc) /* Subscript option chosen */
			{
				CHECK_KEY_RANGE(mu_start_key, mu_end_key, buff, rCnt, musz_range_done);
				if (musz_range_done)
				{
					if (!lvl)	/* Dont count at data level */
						break;
					else
						continue;
				}
			}
		}
		incr_recs = rCnt;
		incr_scans = 1;
	} else if (lvl > targ_levl)
	{	/* visit each child */
		/* Assumption on the fact that level > 0 is always true for this case,
		 * since lowest the level can go is 0 and is checked before calling dfs.
		 */
		assert(lvl);
		gv_target->hist.h[lvl - targ_levl].curr_rec.offset = saveoff[lvl];
		bstar_rec_sz = bstar_rec_size(long_blk_id);
		BLK_LOOP(rCnt, pRec, pBlkBase, pTop, nRecLen, musz_range_done)
		{
			boolean_t first_iter;

			GET_AND_CHECK_RECLEN(status, nRecLen, pRec, pTop, nBlkId, long_blk_id);
			RETURN_IF_ABNORMAL_STATUS(status);
			curroff = (INTPTR_T)(pRec - pBlkBase);
			gv_target->hist.h[lvl - targ_levl].curr_rec.offset = curroff;
			if ((((rec_hdr *)pRec)->rsiz) == bstar_rec_sz) /* Found the star key */
			{
				if (skiprecs && (curroff < saveoff[lvl]))
					continue;	/* skip these guys, we've already counted over there */
			} else
			{
				GET_KEY_CPY_BUFF(pRec, nRecLen, buff, status);
				RETURN_IF_ABNORMAL_STATUS(status);
				if (mu_subsc) /* Subscript option chosen */
					CHECK_KEY_RANGE(mu_start_key, mu_end_key, buff, rCnt, musz_range_done);
				if (skiprecs && (curroff < saveoff[lvl]))
					continue;	/* skip these guys, we've already counted over there */
			}
			status = read_block(nBlkId, &child_pBlkBase, &child_nLevl, lvl - 1);
			RETURN_IF_ABNORMAL_STATUS(status);
			last_rec = ((pRec + nRecLen) == pTop);
			if (lvl && (nBlkId != mu_int_adj_prev[lvl - 1]))
				CHECK_ADJACENCY(nBlkId, lvl - 1, mu_int_adj[lvl - 1]);
			first_iter = (curroff == saveoff[lvl]);
			next_endtree = endtree && last_rec;
			next_skiprecs = skiprecs && first_iter;
			status = dfs(lvl - 1, child_pBlkBase, next_endtree, next_skiprecs);
			if (status != cdb_sc_normal)
			{
				if (cdb_sc_endtree == status)
				{
					mu_size_cumulative[lvl][REC] += rCnt + 1;
					mu_size_cumulative[lvl][BLK]++;
				}
				return status;
			}
		}
		incr_recs = rCnt;
		incr_scans = 1;
	}
	/* make sure we can really move on from this block to the next: validate all blocks down to here */
	memcpy(&sibhist.h[0], &gv_target->hist.h[lvl], SIZEOF(srch_blk_status) * (gv_target->hist.depth - lvl + 2));
	if ((trans_num)0 == t_end(&sibhist, NULL, TN_NOT_SPECIFIED))
		return cdb_sc_restarted;
	mu_size_cumulative[lvl][BLK] += incr_scans;
	mu_size_cumulative[lvl][REC] += incr_recs;
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
	cache_rec_ptr_t			cr;
	enum cdb_sc			status;
	int				cycle, i;
	register srch_blk_status	*pCurr;
	register srch_hist		*pTargHist;
	sm_uc_ptr_t			pBlkBase;
	trans_num			tn;
	unsigned char			nLevl;

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
	RETURN_IF_ABNORMAL_STATUS(status);
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
