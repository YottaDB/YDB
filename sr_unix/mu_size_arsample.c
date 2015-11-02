/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
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
#include "mu_reorg.h"

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

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gv_namehead		*gv_target;
GBLREF	unsigned int		t_tries;
GBLREF	int4			process_id;
GBLREF  inctn_opcode_t          inctn_opcode;
GBLREF  unsigned char		rdfail_detail;

#define MAX_RECS_PER_BLK	65535
#define	MAX_RELIABLE		10000		/* Used to tweak the error estimates */
#define EPS			1e-6
#define APPROX_F_MAX		500		/* Approximate upper bound for the number of records per index block in
						 * a realistic database.
						 */
#define	DYNAMIC_F_MAX		10		/* Choice of these two constants relates to choice of APPROX_F_MAX */
#define EXTRA_F_MAX		50
#define SQR(X)			((double)(X) * (double)(X))
#define ROUND(X)		((int)((X) + 0.5)) /* c89 does not have round() and some Solaris machines uses that compiler */

typedef struct
{	/* cumulative running stats */
	int 	n;			/* number of previous traversals */
	int	N[MAX_BT_DEPTH + 1];	/* number of accepted samples at each given level (more are likely for higher levels) */
	double	M[MAX_BT_DEPTH + 1];	/* M[j] := mean of r[j]'s over previous n traversals
					 * Note: M_n = M_{n-1} + (r_n - M_{n-1}) / n
					 */
	double	S[MAX_BT_DEPTH + 1];	/* S[j] := sum of (r_i[j] - M[j])^2 over each previous traversal, i=1..n
				 	 * Note: S_n = S_{n-1} + (r_n - M_n) * (r_n - M_{n-1})
					 * Later, S values are divided by number of samples to give a plugin estimate of variance
					 * and subsequently are divided by the sample size to give the variance of the mean
				 	 */
	double	f_max[MAX_BT_DEPTH + 1];	/* estimated max fanning factor */
	double	r_max[MAX_BT_DEPTH + 1];	/* max records found in a block at a given level */
	/* Final estimates */
	double	blktot[MAX_BT_DEPTH + 1];	/* estimated #blocks at each level */
	double	blkerr[MAX_BT_DEPTH + 1];	/* approximate variance of blktot */
	double	rectot[MAX_BT_DEPTH + 1];	/* estimated #records at each level */
	double	B;				/* estimated total blocks */
	double	error;				/* approximate error in estimate B */
	double	R;				/* estimated total records */
} stat_t;

STATICFNDCL void finalize_stats_ar(stat_t *stat, boolean_t ar);
STATICFNDCL void accum_stats_ar(stat_t *stat, double *r, boolean_t ar);

#define CLEAR_VECTOR(v)								\
{										\
	int	j;								\
										\
	for (j = 0; j <= MAX_BT_DEPTH; j++)					\
		v[j] = 0;							\
}
#define INIT_STATS(stat)							\
{										\
	int	j;								\
										\
	stat.n = 0;								\
	for (j = 0; j <= MAX_BT_DEPTH; j++)					\
	{									\
		stat.f_max[j] = APPROX_F_MAX;					\
		stat.r_max[j] = 1;						\
	}									\
	CLEAR_VECTOR(stat.N);							\
	CLEAR_VECTOR(stat.M);							\
	CLEAR_VECTOR(stat.S);							\
	CLEAR_VECTOR(stat.blktot);						\
	CLEAR_VECTOR(stat.blkerr);						\
	CLEAR_VECTOR(stat.rectot);						\
}


int4 mu_size_arsample(mval *gn, uint4 M, boolean_t ar, int seed)
{
	enum cdb_sc		status;
	trans_num		ret_tn;
	int			k, h;
	boolean_t		verify_reads;
	boolean_t		tn_aborted;
	unsigned int		lcl_t_tries;
	double			r[MAX_BT_DEPTH + 1];	/* r[j] is #records in level j block of current traversal */
	stat_t		rstat, ustat;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	inctn_opcode = inctn_invalid_op;
	op_gvname(VARLSTCNT(1) gn);
	if (0 == gv_target->root)
        {       /* Global does not exist (online rollback). Not an error. */
                gtm_putmsg(VARLSTCNT(4) ERR_GBLNOEXIST, 2, gn->str.len, gn->str.addr);
                return EXIT_NRM;
        }
	if (!seed)
		seed = (int4)(time(0) * process_id);
	srand48(seed);

	/* do random traversals until M of them are accepted at level 1 */
	INIT_STATS(rstat);
	for (k = 1; rstat.N[1] < M; k++)
	{
		if (mu_ctrlc_occurred || mu_ctrly_occurred)
			return EXIT_ERR;
		t_begin(ERR_MUSIZEFAIL, 0);
		for (;;)
		{
			CLEAR_VECTOR(r);
			if (cdb_sc_normal != (status = rand_traverse(r)))
			{
				assert(CDB_STAGNATE > t_tries);
				t_retry(status);
				continue;
			}
			gv_target->clue.end = 0;
			gv_target->hist.h[0] = gv_target->hist.h[1];	/* No level 0 block to validate */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			if ((trans_num)0 == (ret_tn = t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED)))
			{
				ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
				if (tn_aborted)
				{	/* Global does not exist (online rollback). Not an error. */
					gtm_putmsg(VARLSTCNT(4) ERR_GBLNOEXIST, 2, gn->str.len, gn->str.addr);
					return EXIT_NRM;
				}
				continue;
			}
			accum_stats_ar(&rstat, r, ar);
			break;
		}
	}
	finalize_stats_ar(&rstat, ar);

	/* display rstat data */
	/* Showing the error as 2 standard deviations which is a 95% confidence interval for the mean number of blocks at
	 * each level*/
	util_out_print("!/Number of generated samples = !UL", FLUSH, rstat.n);
	util_out_print("Number of accepted samples = !UL", FLUSH, rstat.N[1]);
	util_out_print("Level          Blocks           2 sigma(+/-)      % Accepted", FLUSH);
	for (h = MAX_BT_DEPTH; (h >= 0) && (rstat.blktot[h] < EPS); h--);
	for ( ; h > 0; h--)
		util_out_print("!5UL !15UL !15UL ~ !3UL% !15UL", FLUSH, h, (int)ROUND(rstat.blktot[h]),
				(int)ROUND(sqrt(rstat.blkerr[h])*2),
				(int)ROUND(sqrt(rstat.blkerr[h])*2/rstat.blktot[h]*100),
				(int)ROUND(100.0*rstat.N[h]/rstat.n)
				);
	util_out_print("!5UL !15UL !15UL ~ !3UL%             N/A", FLUSH, h, (int)ROUND(rstat.blktot[h]),
			(int)ROUND(sqrt(rstat.blkerr[h])*2),
			(int)ROUND(sqrt(rstat.blkerr[h])*2/rstat.blktot[h]*100.0)
			);
	util_out_print("Total !15UL !15UL ~ !3UL%             N/A", FLUSH, (int)ROUND(rstat.B),
			(int)ROUND(sqrt(rstat.error)*2),
			(int)ROUND(sqrt(rstat.error)*2/rstat.B*100.0)
			);

	return EXIT_NRM;
}


void accum_stats_ar(stat_t *stat, double *r, boolean_t ar)
{
	int		j, depth, n;
	double	random, M0, accept[MAX_BT_DEPTH + 1];

	++stat->n;
	for (j = MAX_BT_DEPTH; (j >= 0) && (r[j] < EPS); j--)
		accept[j] = 0;
	depth = j;
	assert(depth >= 0);				/* r[0] should remain zero since we don't maintain it */
	accept[depth] = 1;				/* always accept the root */
	for (j = depth - 1; j >= 1; j--)
	{
		if (!ar)
			accept[j] = 1;			/* don't reject anything */
		else if (j == depth - 1)
			accept[j] = accept[j + 1];	/* always accept level beneath root, too */
		else
			accept[j] = accept[j + 1] * (r[j + 1] / stat->f_max[j + 1]);
	}
	accept[0] = 0;					/* computing #blks (e.g #recs in lvl 1+), not #recs in lvl 0+ */

	random = drand48();
	for (j = 0; j <= MAX_BT_DEPTH; j++)
	{
		if (random < accept[j])
		{
			n = ++stat->N[j];
			M0 = stat->M[j];
			stat->M[j] += (r[j] - stat->M[j]) / n;
			stat->S[j] += (r[j] - stat->M[j]) * (r[j] - M0);
			if (n > DYNAMIC_F_MAX)
				stat->f_max[j] = stat->r_max[j] + EXTRA_F_MAX;
		}
		stat->r_max[j] = MAX(stat->r_max[j], r[j]);
	}
}


void finalize_stats_ar(stat_t *stat, boolean_t ar)
{
	int	j;
	double	factor;

	for (j = 0; j <= MAX_BT_DEPTH; j++)
		/* Variance of the mean (mean referes to avg number of records per block) is Var(R)/N where N is samples size */
		if (stat->N[j] > 0)
		{
			stat->S[j] /= stat->N[j];
			stat->S[j] /= stat->N[j];
		}
	stat->N[0] = stat->n;				/* for arithmetic below */
	for (j = MAX_BT_DEPTH; (j >= 0) && (stat->M[j] < EPS); j--);
	assert(j >= 0);					/* stat->M[0] should remain zero since we don't maintain it */
	stat->blktot[j] = 1;
	stat->blkerr[j] = 0;
	for (j-- ; j >= 0; j--)
	{
		if (stat->M[j + 1] == 0)
			stat->M[j + 1] = EPS;		/* remove any chance of division by zero */
		stat->blktot[j] = stat->blktot[j + 1] * stat->M[j + 1];
		/* Var(XY) assuming X and Y are independent = E[X]^2*Var(Y) + E[Y]^2*Var(X) + Var(X)*Var(Y) */
		stat->blkerr[j] = SQR(stat->M[j + 1])*stat->blkerr[j + 1] +
						  SQR(stat->blktot[j + 1])*stat->S[j + 1] + stat->blkerr[j + 1]*stat->S[j + 1];
	}
	stat->B = 0;
	stat->error = 0;
	for (j = 0; j <= MAX_BT_DEPTH; j++)
	{
		stat->B += stat->blktot[j];
		stat->error += stat->blkerr[j];
	}
	stat->R = 0;
	for (j = 0; j <= MAX_BT_DEPTH; j++)
	{
		stat->rectot[j] = stat->blktot[j] * stat->M[j];
		stat->R += stat->rectot[j];
	}
}


/*
 * Performs a random traversal for the sampling methods
 */
enum cdb_sc rand_traverse(double *r)
{
	sm_uc_ptr_t			pVal, pTop, pRec, pBlkBase;
	register gv_namehead		*pTarg;
	register srch_blk_status	*pCurr;
	register srch_hist		*pTargHist;
	block_id			nBlkId;
	block_id			valBlk[MAX_RECS_PER_BLK];	/* valBlk[j] := value in j-th record of current block */
	unsigned char			nLevl;
	cache_rec_ptr_t			cr;
	int				cycle;
	trans_num			tn;
	sm_uc_ptr_t			buffaddr;
	unsigned short			nRecLen;
	uint4				tmp;
	boolean_t			is_mm;
	int4				random;
	int4				rCnt;			/* number of entries in valBlk */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	is_mm = (dba_mm == cs_data->acc_meth);
	pTarg = gv_target;
	pTargHist = &gv_target->hist;
	/* The following largely mimics gvcst_search/gvcst_search_blk */
	nBlkId = pTarg->root;
	tn = cs_addrs->ti->curr_tn;
	if (NULL == (pBlkBase = t_qread(nBlkId, (sm_int_ptr_t)&cycle, &cr)))
		return (enum cdb_sc)rdfail_detail;
	nLevl = ((blk_hdr_ptr_t)pBlkBase)->levl;
	if (MAX_BT_DEPTH < (int)nLevl)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_maxlvl;
	}
	if (0 == (int)nLevl)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_badlvl;
	}
	pTargHist->depth = (int)nLevl;
	pCurr = &pTargHist->h[nLevl];
	(pCurr + 1)->blk_num = 0;
	pCurr->tn = tn;
	pCurr->cycle = cycle;
	pCurr->cr = cr;
	for (;;)
	{
		assert(pCurr->level == nLevl);
		pCurr->cse = NULL;
		pCurr->blk_num = nBlkId;
		pCurr->buffaddr = pBlkBase;
		for (	rCnt = 0, pRec = pBlkBase + SIZEOF(blk_hdr), pTop = pBlkBase + ((blk_hdr_ptr_t)pBlkBase)->bsiz;
				pRec != pTop && rCnt < MAX_RECS_PER_BLK;
				rCnt++, pRec += nRecLen		)
		{	/* enumerate records in block */
			GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);
			pVal = pRec + nRecLen - SIZEOF(block_id);
			if (nRecLen == 0)
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_badoffset;
			}
			if (pRec + nRecLen > pTop)
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_blklenerr;
			}
			GET_LONG(tmp, pVal);
			valBlk[rCnt] = tmp;
		}
		r[nLevl] = rCnt;
		/* randomly select next block */
		random = (int4)(rCnt * drand48());
		random = random & 0x7fffffff; /* to make sure that the sign bit(msb) is off */
		nBlkId = valBlk[random];
		if (is_mm && (nBlkId > cs_addrs->total_blks))
		{
			if (cs_addrs->total_blks < cs_addrs->ti->total_blks)
				return cdb_sc_helpedout;
			else
				return cdb_sc_blknumerr;
		}
		--pCurr; --nLevl;
		if (nLevl < 1)
			break;
		pCurr->tn = cs_addrs->ti->curr_tn;
		if (NULL == (pBlkBase = t_qread(nBlkId, (sm_int_ptr_t)&pCurr->cycle, &pCurr->cr)))
			return (enum cdb_sc)rdfail_detail;
		if (((blk_hdr_ptr_t)pBlkBase)->levl != nLevl)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_badlvl;
		}
	}
	return cdb_sc_normal;
}
