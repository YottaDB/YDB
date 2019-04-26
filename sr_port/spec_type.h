/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define MAX_SPEC_TYPE_LEN	10
#define MAX_COLL_TYPE		1	/*The highest collation type supported
					 *Any changes require corresponding changes to sr_port/get_spec.c */

#define COLL_SPEC		1	/* current default collation spec */

/* GVT leaf node collation header offsets as enum values*/
enum coll_spec_offsets
{
	COLL_SPEC_OFFSET,	/* first  byte of collation record : collation record specification number */
	COLL_NCT_OFFSET,	/* second byte of collation record : numeric collation for this global name */
	COLL_ACT_OFFSET,	/* third  byte of collation record : alternative collation for this global name */
	COLL_VER_OFFSET,	/* fourth byte of collation record : collation library version for this global name */
	COLL_SPEC_LEN		/* length of the collation record stored in the directory tree per global name */
};
