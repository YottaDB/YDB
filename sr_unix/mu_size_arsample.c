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
#include "longset.h"	    /* needed for cws_insert.h */
#include "hashtab_int4.h"
#include "cws_insert.h"
#include "min_max.h"
#include <math.h>

error_def(ERR_GBLNOEXIST);
error_def(ERR_MUSIZEFAIL);

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	gv_namehead		*gv_target;
GBLREF	inctn_opcode_t	  	inctn_opcode;
GBLREF	int			muint_adj;
GBLREF	int4			mu_int_adj[];
GBLREF	int4			process_id;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned int		t_tries;

#define APPROX_F_MAX		500		/* Approximate upper bound for the number of records per index block in
						 * a database. The estimated max fanning factor is initially APPROX_F_MAX.
						 * After DYNAMIC_F_MAX samples, we try to get a closer approximation, by
						 * dynamically adjusting the estimated max fanning factor to be EXTRA_F_MAX
						 * more than the maximum fanning observed ("fanning" is number of records in
						 * a block a particular level). We want a closer approximation so that we reject
						 * fewer samples, and therefore get a size estimation more quickly
						 */
#define	DYNAMIC_F_MAX		10		/* Choice of these two constants relates to choice of APPROX_F_MAX - how? */
#define EXTRA_F_MAX		50

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
	double	A[MAX_BT_DEPTH + 1];		/* A[j] := mean of a[j] over previous n traversals]; see note on M */
	/* Final estimates */
	double	blktot[MAX_BT_DEPTH + 1];	/* estimated #blocks at each level */
	double	blkerr[MAX_BT_DEPTH + 1];	/* approximate variance of blktot */
	double	rectot[MAX_BT_DEPTH + 1];	/* estimated #records at each level */
	double	B;				/* estimated total blocks */
	double	error;				/* approximate error in estimate B */
	double	R;				/* estimated total records */
	double	AT;				/* estimated total adjacency */
} stat_t;

/* macro makes it convenient to manange initialization with changes to stat_t */
#define INIT_STATS(stat)							\
{										\
	int	J;								\
										\
	stat.n = 0;								\
	for (J = 0; MAX_BT_DEPTH >= J; J++)					\
	{									\
		stat.f_max[J] = APPROX_F_MAX;					\
		stat.r_max[J] = 1;						\
	}									\
	CLEAR_VECTOR(stat.N);							\
	CLEAR_VECTOR(stat.M);							\
	CLEAR_VECTOR(stat.S);							\
	CLEAR_VECTOR(stat.blktot);						\
	CLEAR_VECTOR(stat.blkerr);						\
	CLEAR_VECTOR(stat.rectot);						\
	CLEAR_VECTOR(stat.A);							\
}

STATICFNDCL void accum_stats_ar(stat_t *stat, double *r, double *a);
STATICFNDCL void finalize_stats_ar(stat_t *stat);

int4 mu_size_arsample(glist *gl_ptr, uint4 M, int seed)
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
			CLEAR_VECTOR(a);
			if (cdb_sc_normal != (status = mu_size_rand_traverse(r, a)))			/* WARNING assignment */
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
			accum_stats_ar(&rstat, r, a);
			break;
		}
	}
	finalize_stats_ar(&rstat);
	/* display rstat data
	 * Showing error as 2 standard deviations which is a 95% confidence interval for the mean number of blocks at each level
	 */
	util_out_print("!/Number of generated samples = !UL", FLUSH, rstat.n);
	util_out_print("Number of accepted samples = !UL", FLUSH, rstat.N[1]);
	util_out_print("Level          Blocks        Adjacent           2 sigma(+/-)      % Accepted", FLUSH);
	for (h = MAX_BT_DEPTH; (0 <= h) && (rstat.blktot[h] < EPS); h--)
		;
	for ( ; h > 0; h--)
		util_out_print("!5UL !15UL !15UL !15UL ~ !3UL% !15UL", FLUSH, h, (int)ROUND(rstat.blktot[h]),
				(int)ROUND(sqrt(rstat.blkerr[h]) * 2),
				(int)ROUND(mu_int_adj[h]),
				(int)ROUND(sqrt(rstat.blkerr[h]) * 2 / rstat.blktot[h] * 100),
				(int)ROUND(100.0 * rstat.N[h] / rstat.n)
				);
	util_out_print("!5UL !15UL !15UL !15UL ~ !3UL%             N/A", FLUSH, h, (int)ROUND(rstat.blktot[h]),
			(int)ROUND(mu_int_adj[h]),
			(int)ROUND(sqrt(rstat.blkerr[h]) * 2),
			(int)ROUND(sqrt(rstat.blkerr[h]) * 2 / rstat.blktot[h] * 100.0)
			);
	util_out_print("Total !15UL !15UL !15UL ~ !3UL%             N/A", FLUSH, (int)ROUND(rstat.B),
			(int)ROUND(rstat.AT),
			(int)ROUND(sqrt(rstat.error) * 2),
			(int)ROUND(sqrt(rstat.error) * 2 / rstat.B * 100.0)
			);
	return EXIT_NRM;
}

STATICFNDCL void accum_stats_ar(stat_t *stat, double *r, double *a)
{
	double		random, M0, accept[MAX_BT_DEPTH + 1];
	int		j, depth, n;

	++stat->n;
	for (j = MAX_BT_DEPTH; (0 <= j) && (r[j] < EPS); j--)
		accept[j] = 0;
	depth = j;
	assert(0 <= depth);				/* r[0] should remain zero since we don't maintain it */
	accept[depth] = 1;				/* always accept the root */
	for (; 2 <= j; j--)
		accept[j - 1] = accept[j] * ((j == depth) ? 1 : (r[j] / stat->f_max[j]));
	accept[0] = 0;					/* computing #blks (e.g #recs in lvl 1+), not #recs in lvl 0+ */
	random = drand48();
	for (j = 0; MAX_BT_DEPTH >= j; j++)
	{
		if (random < accept[j])
		{
			n = ++stat->N[j];
			M0 = stat->M[j];
			stat->M[j] += ((r[j] - M0) / n);
			stat->S[j] += (r[j] - stat->M[j]) * (r[j] - M0);
			if (n > DYNAMIC_F_MAX)
				stat->f_max[j] = stat->r_max[j] + EXTRA_F_MAX;
			stat->A[j] += ((a[j] - stat->A[j]) / n);
		}
		stat->r_max[j] = MAX(stat->r_max[j], r[j]);
	}
}

STATICFNDCL void finalize_stats_ar(stat_t *stat)
{
	int	j, k;

	for (j = 0; MAX_BT_DEPTH >= j; j++)
		/* Variance of the mean (mean referes to avg number of records per block) is Var(R)/N where N is samples size */
		if (stat->N[j] > 0)
		{
			stat->S[j] /= stat->N[j];
			stat->S[j] /= stat->N[j];
		}
	stat->N[0] = stat->n;				/* for arithmetic below */
	/* Note: stat->M[0] should remain zero since we don't maintain it. Also stat->M[1] should be > EPS.
	 * So "j" is guaranteed to be at least 1 at the end of the for loop. Assert that.
	 * In "pro" we be safe and add the "(0 < j)" check in the for loop below to prevent "j" from becoming negative.
	 */
	assert(0 == stat->M[0]);
	assert(EPS > 0);
	assert(EPS < 1);
	assert(1 <= stat->M[1]);
	for (j = MAX_BT_DEPTH; (0 < j) && (stat->M[j] < EPS); j--)
		;
	assert(0 < j);
	mu_int_adj[j] = stat->AT = stat->blkerr[j] = stat->error = 0;
	stat->B = stat->blktot[j] = 1;
	for (k = j - 1; j > 0; j--, k--)
	{
		if (0 == stat->M[j])
			stat->M[j] = EPS;		/* remove any chance of division by zero */
		stat->blktot[k] = stat->blktot[j] * stat->M[j];
		stat->B += stat->blktot[k];
		mu_int_adj[k] = stat->blktot[j] * stat->A[j];
		stat->AT += mu_int_adj[k];
		/* Var(XY) assuming X and Y are independent = E[X]^2*Var(Y) + E[Y]^2*Var(X) + Var(X)*Var(Y) */
		stat->blkerr[k] = SQR(stat->M[j]) * stat->blkerr[j]
						+ SQR(stat->blktot[j]) * stat->S[j] + stat->blkerr[j] * stat->S[j];
		stat->error += stat->blkerr[k];
	}
	stat->R = 0;
	for (j = 0; MAX_BT_DEPTH >= j; j++)
	{
		stat->rectot[j] = stat->blktot[j] * stat->M[j];
		stat->R += stat->rectot[j];
	}
}
