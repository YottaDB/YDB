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

#ifndef MMEMORY_INCLUDED
#define MMEMORY_INCLUDED

#include <stddef.h>
#include "gtm_malloc.h"		/* Violates imbedded include standard but otherwise need to update nearly 60 modules */

#define MC_DSBLKSIZE ((8 * 1024) - offsetof(storElem, userStorage))	/* Total (real) alloc will be for 8K */

/* The header of the memory block allocated by mcalloc */
typedef struct mcalloc_hdr_struct
{
	struct mcalloc_hdr_struct *link;	/* pointer to the next block */
	int4		size;			/* size of the usable area in this block */
	GTM64_ONLY(int filler;)			/* The data(data[0]) on 64-bit platforms should begin on 8-byte boundary */
	char		data[1];		/* beginning of the allocatable area (NOTE: should be last member) */
} mcalloc_hdr;

#define MCALLOC_HDR_SZ	offsetof(mcalloc_hdr, data[0])

char *mcalloc(unsigned int n);
int memvcmp(void *a, int a_len, void *b, int b_len);
int memucmp(uchar_ptr_t a, uchar_ptr_t b, uint4 siz);

#endif /* MMEMORY_INCLUDED */
