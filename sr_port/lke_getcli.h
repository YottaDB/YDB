/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
	mstr *one_lock, bool *memory, bool *nocrit, boolean_t *exact, int *repeat, boolean_t *integ);
int lke_getki(char* src, int srclen, char* outptr);

#ifdef VMS
#define HEXPID
#endif

#endif
