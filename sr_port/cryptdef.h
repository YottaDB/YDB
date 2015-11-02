/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
				if ((lkid & 4) == 4)		\
				{				\
					GTMASSERT;		\
				}				\
				lkid++;				\
			  }					\
			}
#if defined (DEBUG) || defined (NOLICENSE)
#define LP_LICENSED(a,b,c,d,e,f,g,h,i,j)  1 /* equivalent to  SS$_NORMAL */
#if defined (VMS)
#define LP_ACQUIRE(a,b,c,d)  lp_id(d)
#else
#define LP_ACQUIRE(a,b,c,d)  1
#endif
#define LP_CONFIRM(a,b)      1
#else
#define LP_LICENSED(a,b,c,d,e,f,g,h,i,j)  lp_licensed(a,b,c,d,e,f,g,h,i,j)
#define LP_ACQUIRE(a,b,c,d)  lp_acquire(a,b,c,d)
#define LP_CONFIRM(a,b)      lp_confirm(a,b)
#endif
