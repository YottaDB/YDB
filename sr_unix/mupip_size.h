/****************************************************************
 *								*
 * Copyright (c) 2012-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2022 YottaDB LLC and/or its subsidiaries.	*
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

#define	RETURN_IF_ABNORMAL_STATUS(STATUS)		\
{							\
	if (cdb_sc_normal != STATUS)			\
	{						\
		assert(CDB_STAGNATE > t_tries);		\
		return STATUS;				\
	}						\
}

/* Input variables
 *	REC points to the record header (start of some record in the block).
 *	REC_LEN is the length of the record and has already been validated by caller to be within the bounds of the block.
 *	BUFF is a pointer to a char[] array.
 * Output variables
 *	BUFF is filled with the bytes corresponding to the uncompressed key portion from the record.
 *	STATUS : Set to cdb_sc_normal for normal return. Set to something else for abnormal return.
 * Caller has to check STATUS on return and restart if not cdb_sc_normal.
 */
#define GET_KEY_CPY_BUFF(REC_HDR, REC_LEN, BUFF, STATUS)								\
MBSTART {														\
	int				key_size;									\
	unsigned short			rec_cmpc;									\
	uchar_ptr_t			key_base, ptr, top;								\
															\
	rec_cmpc = EVAL_CMPC((rec_hdr_ptr_t)REC_HDR);									\
	key_base = REC_HDR + SIZEOF(rec_hdr);										\
	top = REC_HDR + REC_LEN - 1;	/* - 1 to leave room for for 2nd null terminator key byte */			\
	STATUS = cdb_sc_blkmod;	/* Assume the block contents changed to start. Fix it if we find a valid key. */	\
	for ( ptr = key_base; ptr < top; )										\
	{														\
		if ((KEY_DELIMITER == *ptr++) && (KEY_DELIMITER == *ptr++))						\
		{	/* Found a valid key within record limits. Set normal status. */				\
			STATUS = cdb_sc_normal;										\
			break;												\
		}													\
	}														\
	if (cdb_sc_normal == STATUS)											\
	{														\
		key_size = (int)(ptr - key_base);									\
		if (SIZEOF(BUFF) >= (rec_cmpc + key_size))								\
		{	/* Key will fit within limits of BUFF allocation. Proceed to copy key into BUFF. */		\
			memcpy(BUFF + rec_cmpc, key_base, key_size);							\
		} else													\
		{	/* Key does not fit within limits of BUFF. Must be a restartable situation. */			\
			STATUS = cdb_sc_blkmod;										\
		}													\
	}														\
	/* else: Did not find a valid key within record limits. Return abnormal status. */				\
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
