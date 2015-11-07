/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
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

#ifdef UNIX
# define JNLSWITCH_TM_FMT 	"_%Y%j%H%M%S"   	/* yearjuliandayhoursminutesseconds */
#else
# define JNLSWITCH_TM_FMT	"|!Y4|!H04!M0!S0|"	/* 'julian' day is added by append_time_stamp() explicitly */
#endif

int 	rename_file_if_exists(char *org_fn, int org_fn_len, char *rename_fn, int *rename_fn_len, uint4 *ustatus);
uint4 	gtm_rename(char *org_fn, int org_fn_len, char *rename_fn, int rename_len, uint4 *ustatus);
uint4 	prepare_unique_name(char *org_fn, int org_fn_len, char *prefix, char *suffix, char *rename_fn, int *rename_fn_len,
				jnl_tm_t now, uint4 *ustatus);
uint4 	append_time_stamp(char *fn, int *fn_len, jnl_tm_t now);
void 	cre_jnl_file_intrpt_rename(int fn_len, sm_uc_ptr_t fn);

#endif
