/****************************************************************
 *								*
 * Copyright (c) 2012-2019 Fidelity National Information	*
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


#define EPS			1e-6
#define MAX_RECS_PER_BLK	65535
#define ROUND(X)		((int)((X) + 0.5)) /* c89 does not have round() and some Solaris machines uses that compiler */
#define	SQR(X)			((double)(X) * (double)(X))

#define BLK_LOOP(rCnt, pRec, pBlkBase, pTop, nRecLen, lDone)					\
		for (rCnt = 0, pTop = pBlkBase + ((blk_hdr_ptr_t)pBlkBase)->bsiz,		\
			pRec = pBlkBase + SIZEOF(blk_hdr);					\
			rCnt < MAX_RECS_PER_BLK && (pRec != pTop) && (!lDone); rCnt++, pRec += nRecLen)

#define CLEAR_VECTOR(v)								\
{										\
	int	J;								\
										\
	for (J = 0; MAX_BT_DEPTH >= J; J++)					\
		v[J] = 0;							\
}

#define GET_KEY_CPY_BUFF(KEY_BASE, REC_CMPC, PTR, FIRST_KEY, NAME_LEN, KEY_SIZE, BUFF, BUFF_LENGTH, REC_LEN)	\
MBSTART {													\
		for (PTR = key_base;  ;)									\
		{												\
			if (KEY_DELIMITER == *PTR++)								\
			{											\
				if (FIRST_KEY)									\
				{										\
					FIRST_KEY = FALSE;							\
					NAME_LEN = (int)(PTR - key_base);					\
				}										\
				if (KEY_DELIMITER == *PTR++)							\
					break;									\
			}											\
		}												\
		KEY_SIZE = (int)(PTR - KEY_BASE);								\
		memcpy(BUFF + REC_CMPC, KEY_BASE, KEY_SIZE);							\
		BUFF_LENGTH = REC_CMPC + KEY_SIZE;								\
		REC_LEN = BUFF_LENGTH;										\
} MBEND

#define CHECK_COLL_KEY(GL_PTR, NULL_COLL_KEY)						\
MBSTART {										\
		if (GL_PTR->reg->std_null_coll && (!NULL_COLL_KEY))			\
		{									\
			GTM2STDNULLCOLL(mu_start_key->base, mu_start_key->end);		\
			if (mu_end_key)							\
				GTM2STDNULLCOLL(mu_end_key->base, mu_end_key->end);	\
			NULL_COLL_KEY = TRUE;						\
		}									\
		if (!(GL_PTR->reg->std_null_coll) && (NULL_COLL_KEY))			\
		{									\
			STD2GTMNULLCOLL(mu_start_key->base, mu_start_key->end);		\
			if (mu_end_key)							\
				STD2GTMNULLCOLL(mu_end_key->base, mu_end_key->end);	\
			NULL_COLL_KEY = FALSE;						\
		}									\
} MBEND
enum size_cumulative_type
{
	BLK,
	REC,
	ADJ,
	CUMULATIVE_TYPE_MAX
};

void mupip_size(void);
int4 mu_size_arsample(glist *gl_ptr, uint M, int seed);
int4 mu_size_impsample(glist *gl_ptr, int4 M, int4 seed);
int4 mu_size_scan(glist *gl_ptr, int4 level);
enum cdb_sc mu_size_rand_traverse(double *r, double *a);
#endif
