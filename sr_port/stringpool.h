/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
	unsigned char *base, *free, *top, *lasttop, prvprt, *invokestpgcollevel;
} spdesc;

void	stp_expand_array(void);
void	stp_gcol(size_t space_needed);										/* BYPASSOK */
void	stp_move(char *from, char *to);
void	stp_init(size_t size);
void	s2pool(mstr *a);
void	s2pool_align(mstr *string);
void	s2pool_concat(mval *dst, mstr *a);	/* concatenates strings "dst->str" + "a" and stores result in "dst->str" */

#ifdef DEBUG
void		stp_vfy_mval(void);
boolean_t	is_stp_space_available(ssize_t space_needed);
#endif

#ifdef DEBUG
#define	IS_STP_SPACE_AVAILABLE(SPC)	is_stp_space_available((ssize_t)SPC)
#else
#define	IS_STP_SPACE_AVAILABLE(SPC)	IS_STP_SPACE_AVAILABLE_PRO((ssize_t)SPC)
#endif

GBLREF	spdesc		stringpool;

#define	IS_STP_SPACE_AVAILABLE_PRO(SPC)	((stringpool.free + SPC) <= stringpool.invokestpgcollevel)
#define	IS_IN_STRINGPOOL(PTR, LEN)		\
		((((unsigned char *)PTR + (int)(LEN)) <= stringpool.top) && ((unsigned char *)PTR >= stringpool.base))
#define	IS_AT_END_OF_STRINGPOOL(PTR, LEN)		(((unsigned char *)PTR + (int)(LEN)) == stringpool.free)
#define	INVOKE_STP_GCOL(SPC)		stp_gcol(SPC);								/* BYPASSOK */

#ifdef DEBUG
GBLREF	boolean_t	stringpool_unusable;
GBLREF	boolean_t	stringpool_unexpandable;
#define	DBG_MARK_STRINGPOOL_USABLE		{ assert(stringpool_unusable); stringpool_unusable = FALSE; }
#define	DBG_MARK_STRINGPOOL_UNUSABLE		{ assert(!stringpool_unusable); stringpool_unusable = TRUE; }
#define	DBG_MARK_STRINGPOOL_EXPANDABLE		{ assert(stringpool_unexpandable); stringpool_unexpandable = FALSE; }
#define	DBG_MARK_STRINGPOOL_UNEXPANDABLE	{ assert(!stringpool_unexpandable); stringpool_unexpandable = TRUE; }
#else
#define	DBG_MARK_STRINGPOOL_USABLE
#define	DBG_MARK_STRINGPOOL_UNUSABLE
#define	DBG_MARK_STRINGPOOL_EXPANDABLE
#define	DBG_MARK_STRINGPOOL_UNEXPANDABLE
#endif

#define	ENSURE_STP_FREE_SPACE(SPC)						\
{										\
	int	lcl_spc_needed;							\
										\
	/* Note down space needed in local to avoid multiple computations */	\
	lcl_spc_needed = SPC;							\
	if (!IS_STP_SPACE_AVAILABLE(lcl_spc_needed))				\
		INVOKE_STP_GCOL(lcl_spc_needed);				\
	assert(IS_STP_SPACE_AVAILABLE(lcl_spc_needed));				\
}

#define	ADD_TO_STPARRAY(PTR, PTRARRAY, PTRARRAYCUR, PTRARRAYTOP, TYPE)					\
{													\
	GBLREF mstr	**stp_array;									\
	GBLREF int	stp_array_size;									\
													\
	if (NULL == PTRARRAY)										\
	{												\
		if (NULL == stp_array)									\
		{											\
			/* Same initialization as is in stp_gcol_src.h */				\
			stp_array = (mstr **)malloc((stp_array_size = STP_MAXITEMS) * SIZEOF(mstr *));	\
		}											\
		PTRARRAYCUR = PTRARRAY = (TYPE **)stp_array;						\
		PTRARRAYTOP = PTRARRAYCUR + stp_array_size;						\
	} else if (PTRARRAYCUR >= PTRARRAYTOP)								\
	{												\
		stp_expand_array();									\
		PTRARRAYCUR = (TYPE **)stp_array + (PTRARRAYCUR - PTRARRAY);				\
		PTRARRAY = (TYPE **)stp_array;								\
		PTRARRAYTOP = PTRARRAY + stp_array_size;						\
	}												\
	*PTRARRAYCUR++ = PTR;										\
}

#define	COPY_ARG_TO_STRINGPOOL(DST, KEYEND, KEYSTART)				\
MBSTART {									\
	int	keylen;								\
										\
	keylen = (unsigned char *)(KEYEND) - (unsigned char *)(KEYSTART);	\
	ENSURE_STP_FREE_SPACE(keylen);						\
	assert(stringpool.top - stringpool.free >= keylen);			\
	memcpy(stringpool.free, (KEYSTART), keylen);				\
	(DST)->mvtype = (MV_STR);						\
	(DST)->str.len = keylen;						\
	(DST)->str.addr = (char *)stringpool.free;				\
	stringpool.free += keylen;						\
} MBEND

