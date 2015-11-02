/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

typedef struct
{
	unsigned char *base, *free, *top, *lasttop, prvprt;
} spdesc;

void	stp_expand_array(void);
void	stp_gcol(int space_needed);										/* BYPASSOK */
void	stp_move(char *from, char *to);
void	stp_init(unsigned int size);
void	s2pool(mstr *a);
void	s2pool_align(mstr *string);

#ifdef DEBUG
void		stp_vfy_mval(void);
boolean_t	is_stp_space_available(int space_needed);
#endif

#ifdef DEBUG
#define	IS_STP_SPACE_AVAILABLE(SPC)	is_stp_space_available(SPC)
#else
#define	IS_STP_SPACE_AVAILABLE(SPC)	IS_STP_SPACE_AVAILABLE_PRO(SPC)
#endif

GBLREF	spdesc		stringpool;

#define	IS_STP_SPACE_AVAILABLE_PRO(SPC)	((stringpool.free + SPC) <= stringpool.top)
#define	IS_IN_STRINGPOOL(PTR, LEN)		\
		((((unsigned char *)PTR + (int)(LEN)) <= stringpool.top) && ((unsigned char *)PTR >= stringpool.base))
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
