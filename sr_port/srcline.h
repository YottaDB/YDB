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

#ifndef SRCLINE_H_INCLUDED
#define SRCLINE_H_INCLUDED

/* $TEXT() source status flags */
#define CHECKSUMFAIL	0x0001
#define LABELNOTFOUND	0x0002
#define ZEROLINE	0x0004
#define NEGATIVELINE	0x0008
#define AFTERLASTLINE	0x0010
#define OBJMODMISS	0x0020
#define SRCNOTFND	0x0040
#define SRCNOTAVAIL	0x0080
#define TRIGNTAVAIL	0x0100

/* Structure that holds the program source information for $TEXT/ZPRINT/etc */
typedef struct
{
	int		srcrecs;	/* Size of the mstr array */
	unsigned int	srcstat;	/* Status of the array */
	unsigned char	*srcbuff;	/* Pointer to source buffer holding all lines */
	mstr		srclines[1];	/* Array size dependent on routine */
} routine_source;

#ifdef GTM_TRIGGER
/* Macro to determine if a routine name is a trigger name - the answer is yes if
 * the routine name contains a "#" which all triggers do. Args are mstr addr and boolean_t.
 */
# define IS_TRIGGER_RTN(RTNNAME, RSLT) 												\
{ 																\
	unsigned char	*cptr, *cptr_top;											\
																\
	if (0 < (RTNNAME)->len)													\
	{															\
		for (cptr = (unsigned char *)(RTNNAME)->addr, cptr_top = cptr + (RTNNAME)->len - 1; cptr <= cptr_top; --cptr_top) \
		{														\
			if ('#' == *cptr_top)											\
				break;												\
		}														\
		RSLT = !(cptr > cptr_top);											\
	} else															\
		RSLT = FALSE;													\
}
#else
# define IS_TRIGGER_RTN(RTNNAME, RSLT) RSLT = FALSE
#endif

#endif /* SRCLINE_H_INCLUDED */
