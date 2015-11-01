/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MOVTC_INCLUDED
#define MOVTC_INCLUDED

void movtc(register int length, register unsigned char *inbuf, const unsigned char table[],
	register unsigned char *outbuf);

#endif /* MOVTC_INCLUDED */
