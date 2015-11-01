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

#ifndef MMEMORY_included
#define MMEMORY_included

char *mcalloc(unsigned int n);
int memvcmp(void *a, int a_len, void *b, int b_len);
int memucmp(uchar_ptr_t a, uchar_ptr_t b, uint4 siz);

#endif /* MMEMORY_included */
