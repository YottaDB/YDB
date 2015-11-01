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

/* str_match.h */

#define MAX_N_TEMPLATES	256

typedef struct
{
	unsigned short	n_subs;
	mstr		sub[MAX_N_TEMPLATES];
} template_struct;

bool str_match(char *ori, unsigned short orilen, char *template, unsigned short template_len);

