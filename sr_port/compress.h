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

#ifndef COMPRESS_INCLUDED
#define COMPRESS_INCLUDED

int compress(uint4 patcode, unsigned char patmask, void *strlit_buff, unsigned char length,
	bool infinite);                /***type int added***/

#endif
