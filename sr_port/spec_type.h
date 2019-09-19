/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* The global modifier record in the directory tree can contain an optional (currently 4-byte) collation specification
 * right after the 4-byte root block number. There is just one such 4-byte collation spec record possible after the root block.
 */
#define MAX_SPEC_TYPE_LEN	10
#define MAX_COLL_TYPE		1	/* The highest collation type supported */
#define COLL_SPEC		1	/* Current default collation spec */

/* GVT leaf node collation header offsets as enum values */
enum coll_spec_offsets
{
	COLL_SPEC_OFFSET,	/* first  byte of collation record : collation record specification number */
	COLL_NCT_OFFSET,	/* second byte of collation record : numeric collation for this global name */
	COLL_ACT_OFFSET,	/* third  byte of collation record : alternative collation for this global name */
	COLL_VER_OFFSET,	/* fourth byte of collation record : collation library version for this global name */
	COLL_SPEC_LEN		/* length of the collation record stored in the directory tree per global name */
};

/* This macro sets collation information (nct, act and ver) of a GVT/global based on information found in a record
 * corresponding to a leaf level block in the directory tree (record pointer SPEC_REC_ADDR, record length SPEC_REC_LEN).
 * RET is set to TRUE if a valid collation record was found, FALSE otherwise.
 */
#define	GET_GVT_COLL_INFO(GVT, SPEC_REC_ADDR, SPEC_REC_LEN, RET)		\
MBSTART {									\
	uchar_ptr_t	spec_rec_ptr;						\
										\
	spec_rec_ptr = SPEC_REC_ADDR;						\
	if ((COLL_SPEC_LEN == SPEC_REC_LEN) && (COLL_SPEC == *spec_rec_ptr))	\
	{									\
		GVT->nct = *(spec_rec_ptr + COLL_NCT_OFFSET);			\
		GVT->act = *(spec_rec_ptr + COLL_ACT_OFFSET);			\
		GVT->ver = *(spec_rec_ptr + COLL_VER_OFFSET);			\
		RET = TRUE;							\
	} else									\
	{									\
		GVT->nct = 0;							\
		GVT->act = 0;							\
		GVT->ver = 0;							\
		RET = FALSE;							\
	}									\
} MBEND;
