/****************************************************************
 *								*
 * Copyright (c) 2008-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_pwd.h"
#include "gtm_signal.h"	/* for SIGPROCMASK */
#include "gtm_string.h"
#include "wbox_test_init.h"

#undef	getpwuid	/* since we are going to use the system level "getpwuid" function, undef the alias to "gtm_getpwuid" */

GBLREF	boolean_t	blocksig_initialized;
GBLREF  sigset_t	block_sigsent;
GBLREF	struct		passwd getpwuid_struct;	/* cached copy of "getpwuid" to try avoid future system calls for the same "uid" */

/* This is a wrapper for the system "getpwuid" and is needed to prevent signal interrupts from occurring in the middle
 * of getpwuid since that is not signal-safe (i.e. could hold system library related locks that might prevent a signal
 * handler from running other system library calls which use the same lock).
 */
struct passwd	*gtm_getpwuid(uid_t uid)
{
	struct passwd	temp_passwd;
	struct passwd	*retval;
	sigset_t	savemask;
	char		*buff;
	size_t		buff_size;
	int		rc;
	DEBUG_ONLY(static boolean_t	first_time = TRUE;)

	assert(!first_time || (INVALID_UID == getpwuid_struct.pw_uid));	/* assert we do the INVALID_UID init in gbldefs.c */
	if (uid != getpwuid_struct.pw_uid) /* if we did not do a "getpwuid" call for this "uid", do it else return cached value */
	{
		assert(blocksig_initialized);	/* the set of blocking signals should be initialized at process startup */
		if (blocksig_initialized)	/* In pro, dont take chances and handle case where it is not initialized */
			SIGPROCMASK(SIG_BLOCK, &block_sigsent, &savemask, rc);
		buff_size = sysconf(_SC_GETPW_R_SIZE_MAX);
		if (-1 == buff_size)
			buff_size = 2048;
		buff = malloc(buff_size);
		if (NULL == buff)
		{
			return NULL;
		}
		getpwuid_r(uid, &temp_passwd, buff, buff_size, &retval);
		if (blocksig_initialized)
			SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
		if (NULL == retval)
			return NULL;	/* error or "uid" record not found */
		getpwuid_struct = *retval;
		/* Cache return from "getpwuid" call and avoid future calls to this function */
#ifdef DEBUG
		if (gtm_white_box_test_case_enabled &&
			(WBTEST_GETPWUID_CHECK_OVERWRITE == gtm_white_box_test_case_number))
		{
			/* White box test case for the issue GTM-8415. getpwuid_struct should not
		   	overwrite by calling getpwuid */
			/* uid 1 is a daemon process id */
			retval = getpwuid((uid_t)1);
			assert(STRCMP(getpwuid_struct.pw_name, retval->pw_name));
		}
		first_time = FALSE;
#endif
	}
	return &getpwuid_struct;
}
