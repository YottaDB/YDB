/****************************************************************
 *								*
 * Copyright (c) 2012-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "change_reg.h"
#include "gtm_time.h"
#include "mvalconv.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "hashtab_int4.h"
#include "cws_insert.h"
#include <math.h>

error_def(ERR_GBLNOEXIST);
error_def(ERR_MUSIZEFAIL);

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	gv_namehead		*gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	int			muint_adj;
GBLREF	int4			mu_int_adj[];
GBLREF	int4			process_id;
GBLREF	unsigned int		t_tries;

#define MAX_RECS_PER_BLK	65535
#define	MAX_RELIABLE		10000		/* Used to tweak the error estimates */

typedef struct
{	/* cumulative stats */
	int4	n;				/* number of samples */
	double	W[MAX_BT_DEPTH + 1];		/* Sum of the importance values of samples for each depth level */
	double	w_mu[MAX_BT_DEPTH + 1];		/* The mean of importance values. It is used to calculate w_variance */
	double	w_variance[MAX_BT_DEPTH + 1];	/* The variance of importance values; used to calculate effective sample size */
	double	mu[MAX_BT_DEPTH + 1];		/* mu[j] := mean of weighted r[j]'s over previous n traversals.
						 * It is the expected number of records at depth j
					 	 * Note: mu_n = mu_{n-1} + w_n/W_n*(r_n - M_{n-1})
					 	 */
	double	S[MAX_BT_DEPTH + 1];		/* S[j] := sum of w_i*(r_i[j] - M[j])^2 over previous traversals.
					 	 * Note: S_n = S_{n-1} + w_n*(r_n - M_n)*(r_n - M_{n-1})
						 * Later, S values are divided by W values to give plugin estimate of the variance.
						 * Subsequently they are divided by the effective sample size to give the variance
						 * of the mean
					 	 */
	double	A[MAX_BT_DEPTH + 1];		/* A[j] := mean of a[j] over previous n traversals]; see note on mu */
	/* Final estimates */
	double	AT;				/* estimated total adjacency */
	double	blktot[MAX_BT_DEPTH + 1];	/* estimated #blocks at each level */
	double	blkerr[MAX_BT_DEPTH + 1];	/* approximate variance of blktot */
	double	rectot[MAX_BT_DEPTH + 1];	/* estimated #records at each level */
	double	B;				/* estimated total blocks */
	double	error;				/* approximate error in estimate B */
	double	R;				/* estimated total records */
} stat_t;

STATICFNDCL void finalize_stats_impsmpl(stat_t *stat);
STATICFNDCL void accum_stats_impsmpl(stat_t *stat, double *r, double *a);

/* macro makes it convenient to manange initialization with changes to stat_t */
#define INIT_STATS(stat)		\
{						\
	stat.n = 0;				\
	CLEAR_VECTOR(stat.W);			\
	CLEAR_VECTOR(stat.w_mu);		\
	CLEAR_VECTOR(stat.w_variance);		\
	CLEAR_VECTOR(stat.mu);			\
	CLEAR_VECTOR(stat.S);			\
	CLEAR_VECTOR(stat.blktot);		\
	CLEAR_VECTOR(stat.blkerr);		\
	CLEAR_VECTOR(stat.rectot);		\
	CLEAR_VECTOR(stat.A);			\
}

 /* Importance Sampling */
int4 mu_size_impsample(glist *gl_ptr, int4 M, int4 seed)
{
	boolean_t		tn_aborted;
	double			a[MAX_BT_DEPTH + 1];	/* a[j] is # of adjacent block pointers in level j block of cur traversal */
	double			r[MAX_BT_DEPTH + 1];	/* r[j] is #records in level j block of current traversal */
	enum cdb_sc		status;
	int			k, h;
	stat_t			rstat;
	trans_num		ret_tn;
	unsigned int		lcl_t_tries;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	inctn_opcode = inctn_invalid_op;
	/* set gv_target/gv_currkey/gv_cur_region/cs_addrs/cs_data to correspond to <globalname,reg> in gl_ptr */
	DO_OP_GVNAME(gl_ptr);
	if (0 == gv_target->root)
	{       /* Global does not exist (online rollback). Not an error. */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_GBLNOEXIST, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
		return EXIT_NRM;
	}
	if (!seed)
		seed = (int4)(time(0) * process_id);
	srand48(seed);
	/* do M random traversals */
	INIT_STATS(rstat);
	for (k = 1; k <= M; k++)
	{
		if (mu_ctrlc_occurred || mu_ctrly_occurred)
			return EXIT_ERR;
		t_begin(ERR_MUSIZEFAIL, 0);
		for (;;)
		{
			CLEAR_VECTOR(r);
			CLEAR_VECTOR(a);
			if (cdb_sc_normal != (status = mu_size_rand_traverse(r, a)))			/* WARNING: assignment */
			{
				assert(UPDATE_CAN_RETRY(t_tries, status));
				t_retry(status);
				continue;
			}
			gv_target->clue.end = 0;
			gv_target->hist.h[0] = gv_target->hist.h[1];				/* No level 0 block to validate */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			if ((trans_num)0 == (ret_tn = t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED)))	/* WARNING: assignment */
			{
				ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
				if (tn_aborted)
				{	/* Global does not exist (online rollback). Not an error. */
					gtm_putmsg_csa(CSA_ARG(NULL)
						VARLSTCNT(4) ERR_GBLNOEXIST, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);
					return EXIT_NRM;
				}
				continue;
			}
			accum_stats_impsmpl(&rstat, r, a);
			break;
		}
	}
	finalize_stats_impsmpl(&rstat);
	/* display rstat data
	 * Showing the error as 2 standard deviations which is a 95% confidence interval for the
	 * mean number of blocks at each level
	 */
	util_out_print("Number of generated samples = !UL", FLUSH, rstat.n);
	util_out_print("Level          Blocks        Adjacent           2 sigma(+/-)", FLUSH);
	for (h = MAX_BT_DEPTH; (0 <= h) && (rstat.blktot[h] < EPS); h--);
	for ( ; h > 0; h--)
		util_out_print("!5UL !15UL !15UL !15UL ~ !3UL%", FLUSH, h, (int)ROUND(rstat.blktot[h]),
				(int)ROUND(mu_int_adj[h]),
				(int)ROUND(sqrt(rstat.blkerr[h]) * 2),
				(int)ROUND(sqrt(rstat.blkerr[h]) * 2 / rstat.blktot[h] * 100.0)
				);
	util_out_print("!5UL !15UL !15UL !15UL ~ !3UL%", FLUSH, h, (int)ROUND(rstat.blktot[h]),
			(int)ROUND(mu_int_adj[h]),
			(int)ROUND(sqrt(rstat.blkerr[h]) * 2),
			(int)ROUND(sqrt(rstat.blkerr[h]) * 2 / rstat.blktot[h] * 100.0)
			);
	util_out_print("Total !15UL !15UL !15UL ~ !3UL%", FLUSH, (int)ROUND(rstat.B),
			(int)ROUND(rstat.AT),
			(int)ROUND(sqrt(rstat.error) * 2),
			(int)ROUND(sqrt(rstat.error) * 2 / rstat.B * 100.0)
			);
	return EXIT_NRM;
}

STATICFNDEF void accum_stats_impsmpl(stat_t *stat, double *r, double *a)
{
	double		mu0, w_mu0, w[MAX_BT_DEPTH + 1] /* importance */;
	int		k, l, root_level;

	++stat->n;
	for (l = MAX_BT_DEPTH; (0 <= l) && (r[l] < EPS); l--)
		w[l] = 0;
	root_level = l;
	assert(0 <= root_level);
	w[root_level] = 1;
	for (k = l - 1; 2 <= l; k--, l--)
		w[k] = w[l] * r[l];			/* NOTE: consider using log to avoid overflow if it becomes an issue */
	w[0] = 0;					/* computing #blks (e.g #recs in lvl 1+), not #recs in lvl 0+ */
	for (l = 1; l <= root_level; l++)
	{
		stat->W[l] += w[l];
		w_mu0 = stat->w_mu[l];
		stat->w_mu[l] += (w[l] - w_mu0) / stat->n;
		stat->w_variance[l] += (w[l] - stat->w_mu[l]) * (w[l] - w_mu0);
		mu0 = stat->mu[l];
		stat->mu[l] += (w[l] / stat->W[l] * (r[l] - stat->mu[l]));
		stat->S[l] += (w[l] * (r[l] - stat->mu[l]) * (r[l] - mu0));
		stat->A[l] += (w[l] / stat->W[l] * (a[l] - stat->A[l]));
	}
}

STATICFNDEF void finalize_stats_impsmpl(stat_t *stat)
{
	double		ess; /* effective sample size */
	int		k, l;

	for (l = 1; MAX_BT_DEPTH >= l; l++)
		if (stat->W[l] > 0)
		{
			/* ess = n / ( 1 + Var( w/mu(w) ) ).
			 * This comes from effective sample size for importance sampling in the literature
			 */
			ess = stat->n / ( 1 + (stat->w_variance[l] / stat->n) / SQR(stat->w_mu[l]) );
			/* Variance of the mean (mean referes to avg number of records per block) is
			 * Var(R)/N where N is effective sample size
			 */
			stat->S[l] /= stat->W[l];
			stat->S[l] /= (ess + 1);
		}
	stat->W[0] = stat->n;				/* for arithmetic below */
	/* Note: stat->mu[0] should remain zero since we don't maintain it. Also stat->mu[1] should be > EPS.
	 * So "l" is guaranteed to be at least 1 at the end of the for loop. Assert that.
	 * In "pro" we be safe and add the "(0 < l)" check in the for loop below to prevent "l" from becoming negative.
	 */
	assert(0 == stat->mu[0]);
	assert(EPS > 0);
	assert(EPS < 1);
	assert(1 <= stat->mu[1]);
	for (l = MAX_BT_DEPTH; (0 < l) && (stat->mu[l] < EPS); l--)
		;
	assert(0 < l);
	stat->AT = stat->blkerr[l] = stat->error = stat->R = 0;
	stat->B = stat->blktot[l] = 1;
	for (k = l - 1 ; 0 < l; k--, l--)
	{
		stat->blktot[k] = stat->blktot[l] * stat->mu[l];
		/* Var(XY) assuming X and Y are independent = E[X]^2*Var(Y) + E[Y]^2*Var(X) + Var(X)*Var(Y) */
		stat->blkerr[k] = SQR(stat->mu[l]) * stat->blkerr[l] + SQR(stat->blktot[l]) * stat->S[l]
					+ stat->blkerr[l] * stat->S[l];
		stat->B += stat->blktot[k];
		mu_int_adj[k] = stat->blktot[l] * stat->A[l];
		stat->AT += mu_int_adj[k];
		stat->error += stat->blkerr[k];
		stat->rectot[k] = stat->blktot[k] * stat->mu[k];
		stat->R += stat->rectot[k];
	}
}
