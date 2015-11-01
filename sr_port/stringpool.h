/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

void stp_expand_array(void);
void stp_gcol(int space_needed);
void stp_move(char *from, char *to);
void stp_init(unsigned int size);
void s2pool(mstr *a);
