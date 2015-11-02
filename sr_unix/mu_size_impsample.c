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
#include <math.h>

error_def(ERR_GBLNOEXIST);
error_def(ERR_MUSIZEFAIL);

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	gv_namehead		*gv_target;
GBLREF	unsigned int		t_tries;
GBLREF	int4			process_id;
GBLREF  inctn_opcode_t          inctn_opcode;

#define MAX_RECS_PER_BLK	65535
#define	MAX_RELIABLE		10000		/* Used to tweak the error estimates */
#define EPS			1e-6
#define SQR(X)			((double)(X) * (double)(X))
#define ROUND(X)		((int)((X) + 0.5)) /* c89 does not have round() and some Solaris machines uses that compiler */

typedef struct
{	/* cumulative stats */
	int4	n;				/* number of samples */
	double	W[MAX_BT_DEPTH + 1];		/* Sum of the importance values of samples for each depth level */
	double	w_mu[MAX_BT_DEPTH + 1];		/* The mean of importance values. It is used to calculate w_variance */
	double	w_variance[MAX_BT_DEPTH + 1];/* The variance of importance values. It is used to calculate effective sample size */
	double	mu[MAX_BT_DEPTH + 1];		/* mu[j] := mean of weighted r[j]'s over previous n traversals.
						   It is the expected number of records at depth j
					 	 * Note: mu_n = mu_{n-1} + w_n/W_n*(r_n - M_{n-1})
					 	 */
	double	S[MAX_BT_DEPTH + 1];		/* S[j] := sum of w_i*(r_i[j] - M[j])^2 over previous traversals.
					 	 * Note: S_n = S_{n-1} + w_n*(r_n - M_n)*(r_n - M_{n-1})
						 * Later, S values are divided by W values to give plugin estimate of the variance.
						 * Subsequently they are divided by the effective sample size to give the variance
						 * of the mean
					 	 */
	/* Final estimates */
	double	blktot[MAX_BT_DEPTH + 1];	/* estimated #blocks at each level */
	double	blkerr[MAX_BT_DEPTH + 1];	/* approximate variance of blktot */
	double	rectot[MAX_BT_DEPTH + 1];	/* estimated #records at each level */
	double	B;				/* estimated total blocks */
	double	error;				/* approximate error in estimate B */
	double	R;				/* estimated total records */
} stat_t;

STATICFNDCL void clear_vector_impsmpl(double *v);
STATICFNDCL void init_stats_impsmpl(stat_t *stat);
STATICFNDCL void finalize_stats_impsmpl(stat_t *stat);
STATICFNDCL void accum_stats_impsmpl(stat_t *stat, double *r);

STATICFNDEF void clear_vector_impsmpl(double *v)
{
	int	j;
	for (j = 0; j <= MAX_BT_DEPTH; j++)
		v[j] = 0;
}


STATICFNDEF void init_stats_impsmpl(stat_t *stat)
{
	stat->n = 0;
	clear_vector_impsmpl(stat->W);
	clear_vector_impsmpl(stat->w_mu);
	clear_vector_impsmpl(stat->w_variance);
	clear_vector_impsmpl(stat->mu);
	clear_vector_impsmpl(stat->S);
	clear_vector_impsmpl(stat->blktot);
	clear_vector_impsmpl(stat->blkerr);
	clear_vector_impsmpl(stat->rectot);
}

/*
 * Importance Sampling
 */
int4 mu_size_impsample(mval *gn, int4 M, int4 seed)
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

	/* do M random traversals */
	init_stats_impsmpl(&rstat);
	for (k = 1; k <= M; k++)
	{
		if (mu_ctrlc_occurred || mu_ctrly_occurred)
			return EXIT_ERR;
		t_begin(ERR_MUSIZEFAIL, 0);
		for (;;)
		{
			clear_vector_impsmpl(r);
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
			accum_stats_impsmpl(&rstat, r);
			break;
		}
	}
	finalize_stats_impsmpl(&rstat);

	/* display rstat data
	 * Showing the error as 2 standard deviations which is a 95% confidence interval for the
	 * mean number of blocks at each level
	 */
	util_out_print("Number of generated samples = !UL", FLUSH, rstat.n);
	util_out_print("Level          Blocks           2 sigma(+/-)", FLUSH);
	for (h = MAX_BT_DEPTH; (h >= 0) && (rstat.blktot[h] < EPS); h--);
	for ( ; h > 0; h--)
		util_out_print("!5UL !15UL !15UL ~ !3UL%", FLUSH, h, (int)ROUND(rstat.blktot[h]),
				(int)ROUND(sqrt(rstat.blkerr[h])*2),
				(int)ROUND(sqrt(rstat.blkerr[h])*2/rstat.blktot[h]*100.0)
				);
	util_out_print("!5UL !15UL !15UL ~ !3UL%", FLUSH, h, (int)ROUND(rstat.blktot[h]),
			(int)ROUND(sqrt(rstat.blkerr[h])*2),
			(int)ROUND(sqrt(rstat.blkerr[h])*2/rstat.blktot[h]*100.0)
			);
	util_out_print("Total !15UL !15UL ~ !3UL%", FLUSH, (int)ROUND(rstat.B),
			(int)ROUND(sqrt(rstat.error)*2),
			(int)ROUND(sqrt(rstat.error)*2/rstat.B*100.0)
			);

	return EXIT_NRM;
}


STATICFNDEF void accum_stats_impsmpl(stat_t *stat, double *r)
{
	int		l, root_level, n;
	double	mu0, w_mu0, w[MAX_BT_DEPTH + 1] /* importance */;

	++stat->n;
	for (l = MAX_BT_DEPTH; (l >= 0) && (r[l] < EPS); l--)
		w[l] = 0;
	root_level = l;
	assert(root_level >= 0);
	w[root_level] = 1;
	for (l = root_level - 1; l >= 1; l--)
		w[l] = w[l + 1] * r[l + 1];		/* TODO consider using log to avoid overflow if it becomes an issue */
	w[0] = 0;					/* computing #blks (e.g #recs in lvl 1+), not #recs in lvl 0+ */

	for (l = 1; l <= root_level; l++)
	{
		stat->W[l] += w[l];
		w_mu0 = stat->w_mu[l];
		stat->w_mu[l] += (w[l] - stat->w_mu[l])/stat->n;
		stat->w_variance[l] += (w[l] - stat->w_mu[l])*(w[l] - w_mu0);
		mu0 = stat->mu[l];
		stat->mu[l] += w[l]/stat->W[l]*(r[l] - stat->mu[l]);
		stat->S[l] += w[l]*(r[l] - stat->mu[l])*(r[l] - mu0);
	}
}


STATICFNDEF void finalize_stats_impsmpl(stat_t *stat)
{
	int		h;
	double	ess; /* effective sample size */

	for (h = 1; h <= MAX_BT_DEPTH; h++)
		if (stat->W[h] > 0)
		{
			/* ess = n / ( 1 + Var( w/mu(w) ) ).
			 * This comes from effective sample size for importance sampling in the literature*/
			ess = stat->n / ( 1 + (stat->w_variance[h]/stat->n)/SQR(stat->w_mu[h]) );
			/* Variance of the mean (mean referes to avg number of records per block) is
			 * Var(R)/N where N is effective sample size */
			stat->S[h] /= stat->W[h];
			stat->S[h] /= (ess + 1);
		}
	stat->W[0] = stat->n;				/* for arithmetic below */
	for (h = MAX_BT_DEPTH; (h >= 0) && (stat->mu[h] < EPS); h--);
	assert(h >= 0);				/* stat->mu[0] should remain zero */
	stat->blktot[h] = 1;
	stat->blkerr[h] = 0;
	for (h-- ; h >= 0; h--)
	{
		stat->blktot[h] = stat->blktot[h + 1] * stat->mu[h + 1];
		/* Var(XY) assuming X and Y are independent = E[X]^2*Var(Y) + E[Y]^2*Var(X) + Var(X)*Var(Y) */
		stat->blkerr[h] = SQR(stat->mu[h + 1])*stat->blkerr[h + 1] + SQR(stat->blktot[h + 1])*stat->S[h + 1]
					+ stat->blkerr[h + 1]*stat->S[h + 1];
	}
	stat->B = 0;
	stat->error = 0;
	for (h = 0; h <= MAX_BT_DEPTH; h++)
	{
		stat->B += stat->blktot[h];
		stat->error += stat->blkerr[h];
	}
	stat->R = 0;
	for (h = 0; h <= MAX_BT_DEPTH; h++)
	{
		stat->rectot[h] = stat->blktot[h] * stat->mu[h];
		stat->R += stat->rectot[h];
	}
}
