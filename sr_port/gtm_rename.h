/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
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

int 	rename_file_if_exists(char *org_fn, int org_fn_len, char *rename_fn, int *rename_fn_len, uint4 *ustatus);
uint4 	gtm_rename(char *org_fn, int org_fn_len, char *rename_fn, int rename_len, uint4 *ustatus);
uint4 	prepare_unique_name(char *org_fn, int org_fn_len, char *prefix, char *suffix, char *rename_fn,
		int *rename_fn_len, uint4 *ustatus);
uint4 	append_time_stamp(char *fn, int fn_len, int *app_len, uint4 *ustatus);
void 	cre_jnl_file_intrpt_rename(int fn_len, sm_uc_ptr_t fn);

#endif
