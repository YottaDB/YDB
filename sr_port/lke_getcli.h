/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef LKE_GETCLI_H
#define LKE_GETCLI_H

int4 lke_getcli(bool *all, bool *wait, bool *inta, int4 *pid, mstr *region, mstr *node,
	mstr *one_lock, bool *memory, bool *nocrit, boolean_t *exact);
int lke_getki(char* src, int srclen, char* outptr);

#ifdef VMS
#define HEXPID
#endif

#endif
