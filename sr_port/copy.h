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

/* If unaligned access is supported UNALIGNED_ACCESS_SUPPORTED has to be defined in the appropriate mdefsp.h
 * On platforms where unaligned data is not legal,
 * the macros are defined using a series of character moves */

#ifdef UNALIGNED_ACCESS_SUPPORTED
/* Unsigned versions are different from signed ones as we may get sign extension problems when promotion is needed */
#define GET_LONGP(X,Y)	(*(int4 *)(X) = *(int4 *)(Y))
#define GET_SHORTP(X,Y)	(*(short *)(X) = *(short *)(Y))
#define GET_LONG(X,Y)	((X) = *(int4 *)(Y))
#define GET_ULONG(X,Y)	((X) = *(uint4 *)(Y))
#define GET_SHORT(X,Y)	((X) = *(short *)(Y))
#define GET_USHORT(X,Y)	((X) = *(unsigned short *)(Y))
#define GET_CHAR(X,Y)	((X) = *(unsigned char *)(Y))
#define REF_CHAR(Y)	(*(unsigned char *)Y)
#define PUT_ZERO(X)	((X) = 0)
#define PUT_LONG(X,Y)	(*(int4*)(X) = (Y))
#define PUT_ULONG(X,Y)	(*(uint4*)(X) = (Y))
#define PUT_SHORT(X,Y)	(*(short*)(X) = (Y))
#define PUT_USHORT(X,Y)	(*(unsigned short*)(X) = (Y))
#define PUT_CHAR(X,Y)	(*(unsigned char *)(X) = (Y))
#else
#include <sys/types.h>
#define GET_LONGP(X,Y)	(*(caddr_t)(X)     = *(caddr_t)(Y), \
			 *((caddr_t)(X)+1) = *((caddr_t)(Y)+1), \
			 *((caddr_t)(X)+2) = *((caddr_t)(Y)+2), \
			 *((caddr_t)(X)+3) = *((caddr_t)(Y)+3))

#define GET_SHORTP(X,Y)	(*(caddr_t)(X) = *(caddr_t)(Y), *((caddr_t)(X)+1) = *((caddr_t)(Y)+1))

/* Unsigned versions are same as the signed ones as we do char by char */
#define GET_LONG(X,Y)	(*(caddr_t)(&X)     = *(caddr_t)(Y), \
			 *((caddr_t)(&X)+1) = *((caddr_t)(Y)+1), \
			 *((caddr_t)(&X)+2) = *((caddr_t)(Y)+2), \
			 *((caddr_t)(&X)+3) = *((caddr_t)(Y)+3))

#define GET_ULONG	GET_LONG

#define GET_SHORT(X,Y)	(*(caddr_t)(&X) = *(caddr_t)(Y), *((caddr_t)(&X)+1) = *((caddr_t)(Y)+1))

#define GET_USHORT	GET_SHORT

#define GET_CHAR(X,Y)	(*(caddr_t)(&X) = *(caddr_t)(Y))
#define REF_CHAR(Y)	(*(caddr_t)(Y))

#define PUT_ZERO(X)	(memset((caddr_t)&(X), 0, SIZEOF(X)))

#define PUT_LONG(X,Y)	(*(caddr_t)(X)     = *(caddr_t)(&Y), \
			 *((caddr_t)(X)+1) = *((caddr_t)(&Y)+1), \
			 *((caddr_t)(X)+2) = *((caddr_t)(&Y)+2), \
			 *((caddr_t)(X)+3) = *((caddr_t)(&Y)+3))

#define PUT_ULONG	PUT_LONG

#define PUT_SHORT(X,Y)	(*(caddr_t)(X) = *(caddr_t)(&Y), *((caddr_t)(X)+1) = *((caddr_t)(&Y)+1))

#define PUT_USHORT	PUT_SHORT

#define PUT_CHAR(X,Y)	(*(caddr_t)(X) = *(caddr_t)(&Y))
#endif /*UNALIGNED_ACCESS_SUPPORTED*/
