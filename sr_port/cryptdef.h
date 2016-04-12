/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

typedef struct
{
	uint4		sid[16];
	uint4		exp_date;
	unsigned char	gtm_serial[8];
} gtm_id_block;

typedef struct
{
	gtm_id_block	plaintext;
	unsigned char	key[SIZEOF(gtm_id_block)];
	gtm_id_block	cryptext;
} gtm_id_struct;

#define CRYPT_CHKSYSTEM { if (!licensed) 			\
			  {					\
				assertpro((lkid & 4) != 4);	\
				lkid++;				\
			  }					\
			}
#if defined (DEBUG) || defined (NOLICENSE)
#define LP_LICENSED(a,b,c,d,e,f,g,h,i,j)  1
#define LP_ACQUIRE(a,b,c,d)  1
#define LP_CONFIRM(a,b)      1
#else
#define LP_LICENSED(a,b,c,d,e,f,g,h,i,j)  lp_licensed(a,b,c,d,e,f,g,h,i,j)
#define LP_ACQUIRE(a,b,c,d)  lp_acquire(a,b,c,d)
#define LP_CONFIRM(a,b)      lp_confirm(a,b)
#endif
