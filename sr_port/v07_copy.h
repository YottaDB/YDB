/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* This set of macros is used where data may be unaligned;
 * on platforms where unaligned data is not legal,
 * they must be superceded by an equivalent, but legal technique,
 * such as a series of character moves */

#define GET_LONGP(X,Y) (*(int4 *)(X) = *(int4 *)(Y))
#define GET_SHORTP(X,Y) (*(short *)(X) = *(short *)(Y))
#define GET_LONG(X,Y) ((X) = *(int4 *)(Y))
#define GET_SHORT(X,Y) ((X) = *(short *)(Y))
#define GET_USHORT(X,Y) ((X) = *(unsigned short *)(Y))
#define GET_CHAR(X,Y) ((X) = *(unsigned char *(Y))
#define REF_CHAR(Y) (*(unsigned char *)Y)
#define PUT_ZERO(X) (X = 0)
#define PUT_LONG(X,Y) (*(int4*)(X) = (Y))
#define PUT_SHORT(X,Y) (*(short*)(X) = (Y))
#define PUT_USHORT(X,Y) (*(unsigned short*)(X) = (Y))
#define PUT_CHAR(X,Y) (*(unsigned char *)(X) = (Y))
