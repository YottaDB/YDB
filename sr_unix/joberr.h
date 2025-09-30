/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
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

typedef struct joberr_msg_struct{
char		*msg;
int		len;
}joberr_msg;

/* The follwoing array is indexed by values from the enum joberr_t from jobsp.h. */
LITDEF joberr_msg joberrs[] = {
#	define	JOBERR_TABLE_ENTRY(JOBERR_ENUM, JOBERR_STRING)	{ LIT_AND_LEN(JOBERR_STRING) },
#	include "joberr_table.h"
#	undef JOBERR_TABLE_ENTRY
};

