/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GETFREE_INLINE_BLK_INCLUDED
#define GETFREE_INLINE_BLK_INCLUDED

#define	RETURN_IF_FREE(VALID, PTR, BASE_ADDR)			\
MBSTART{							\
	if (valid)						\
		return return_if_free(VALID, PTR, BASE_ADDR);	\
} MBEND

static inline block_id return_if_free(u_char valid, u_char *ptr, u_char *base_addr)
{
	int4	bits;

	assert(valid);
	if (valid & BLK_FREE)	/* check block 0 of the set of 4 in the byte for free */
		bits = 0;
	else if (valid & BLK_ONE_FREE)
		bits = 1;
	else if (valid & BLK_TWO_FREE)
		bits = 2;
	else
		bits = 3;
	return (int4)((ptr - base_addr) * (BITS_PER_UCHAR / BML_BITS_PER_BLK) + bits);
}

#endif /* GETFREE_INLINE_BLK_INCLUDED */
