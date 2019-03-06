/****************************************************************
 *								*
 * Copyright (c) 2012-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUPIP_SIZE_H_INCLUDED
#define MUPIP_SIZE_H_INCLUDED

#include "cdb_sc.h"

void mupip_size(void);
int4 mu_size_arsample(glist *gl_ptr, uint M, int seed);
int4 mu_size_impsample(glist *gl_ptr, int4 M, int4 seed);
int4 mu_size_scan(glist *gl_ptr, int4 level);
enum cdb_sc mu_size_rand_traverse(double *r, double *a);

#define EPS			1e-6
#define MAX_RECS_PER_BLK	65535
#define ROUND(X)		((int)((X) + 0.5)) /* c89 does not have round() and some Solaris machines uses that compiler */
#define SQR(X)			((double)(X) * (double)(X))

#define BLK_LOOP(rCnt, pRec, pBlkBase, pTop, nRecLen)						\
		for (rCnt = 0, pTop = pBlkBase + ((blk_hdr_ptr_t)pBlkBase)->bsiz,		\
			pRec = pBlkBase + SIZEOF(blk_hdr);					\
			rCnt < MAX_RECS_PER_BLK && (pRec != pTop); rCnt++, pRec += nRecLen)

#define CLEAR_VECTOR(v)								\
{										\
	int	J;								\
										\
	for (J = 0; MAX_BT_DEPTH >= J; J++)					\
		v[J] = 0;							\
}

#endif
