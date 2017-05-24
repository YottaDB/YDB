/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GTM_RENAME_
#define __GTM_RENAME_
/* gtm_rename.h */

#define RENAME_SUCCESS 0
#define RENAME_NOT_REQD 1
#define RENAME_FAILED 2

#define JNLSWITCH_TM_FMT 	"_%Y%j%H%M%S"   /* yearjuliandayhoursminutesseconds */
#define JNLSWITCH_TM_FMT_LEN 	14   		/* SIZE of string produced by STRFTIME(JNLSWITCH_TM_FNT) */

int 	rename_file_if_exists(char *org_fn, int org_fn_len, char *rename_fn, int *rename_fn_len, uint4 *ustatus);
uint4 	gtm_rename(char *org_fn, int org_fn_len, char *rename_fn, int rename_len, uint4 *ustatus);
uint4 	prepare_unique_name(char *org_fn, int org_fn_len, char *prefix, char *suffix, char *rename_fn, int *rename_fn_len,
				jnl_tm_t now, uint4 *ustatus);

#endif
