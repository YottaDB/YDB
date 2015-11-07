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

#ifndef QUAD2ASC_INCLUDED
#define QUAD2ASC_INCLUDED

int quad2asc(int4 mantissa[], char exponent, unsigned char *outaddr,
	unsigned short outaddrlen, unsigned short *actual_len);		/***type int added***/

#endif /* QUAD2ASC_INCLUDED */
