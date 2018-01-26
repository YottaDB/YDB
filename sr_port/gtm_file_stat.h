/****************************************************************
 *								*
 * Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_FILE_STAT_INCLUDED
#define GTM_FILE_STAT_INCLUDED

/* Note that FILE_READONLY also implies FILE_PRESENT and the callers can use this information if necessary */
#define FILE_NOT_FOUND 0
#define FILE_PRESENT 1
#define FILE_READONLY 2
#define FILE_STAT_ERROR 4

int gtm_file_stat(mstr *file, mstr *def, mstr *ret, boolean_t check_prv, uint4 *status);

#endif /* GTM_FILE_STAT_INCLUDED */
