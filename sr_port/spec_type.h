/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define MAX_SPEC_TYPE_LEN	10
#define LAST_TYPE_DEFINED	2

#define COLL_SPEC_LEN		4	/* length of the collation record stored in the directory tree per global name */
#define COLL_SPEC		1	/* first byte of collation record : hardcoded type of 1 */
#define COLL_NCT_OFFSET		1	/* second byte of collation record : numeric collation for this global name */
#define	COLL_ACT_OFFSET		2	/* third  byte of collation record : alternative collation for this global name */
#define	COLL_VER_OFFSET		3	/* fourth byte of collation record : collation library version for this global name */
