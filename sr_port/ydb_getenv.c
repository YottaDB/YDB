/****************************************************************
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "gtm_common_defs.h"	/* Not including mdef.h since this is needed by the encryption plugin */

#include <errno.h>

#include <stdlib.h>	/* Not including gtm_stdlib.h since this is needed by the encryption plugin */
#include <stdio.h>	/* Not including gtm_stdio.h since this is needed by the encryption plugin */
#include "ydb_getenv.h"

#undef assert	/* Do not want this to pull in heavyweight rts_error set of functions. Instead use C standard assert */
#include <assert.h>	/* We still want to use "assert" below so use C standard facility which is not as heavyweight */

/* This function is similar to "getenv" except that it is given an "envindx" instead of an env var name.
 * For a given index "envindx", this function checks in the ydbenvname[] table if that index has a non-zero string literal
 * and if so checks if an environment variable with that name is defined and if so uses that. If not, it checks in the
 * gtmenvname[] table if the same index has a non-zero string literal and if so uses that as the environment variable
 * to check if it is defined. This way, ydb* takes precedence over gtm* if both exist.
 * If "suffix" is non-NULL, that string is added to the environment variable name and the "getenv" is done on the resulting string.
 * If return value is not NULL and is_ydb_env_match is non-NULL,
 *	*is_ydb_env_match is set to TRUE if the ydb* env var matched and set to FALSE if the gtm* env var matched.
 *
 * Note: This module currently does not set the ydb* env var to be the same as the gtm* env var (like "ydb_trans_log_name" does)
 * as that involves handling errors which this is currently not set up to do. That is because this is invoked from the encryption
 * plugin and suid programs like "gtmsecshr" and so we want it to not pull in any more functions than absolutely necessary.
 * Adding a call to "rts_error_csa" would pull in a lot more than desirable. Therefore, it is the caller who has to take care
 * of issuing any appropriate errors or doing any ydb* "setenv" calls.
 */
char *ydb_getenv(ydbenvindx_t envindx, mstr *suffix, boolean_t *is_ydb_env_match)
{
	char		*envnamestr[2], *ret;
	const char	*envstr;
	int		i, nbytes;
	char		envwithsuffix[1024];	/* We don't expect the name of an env var to be longer than 1K */

	envnamestr[0] = (char *)ydbenvname[envindx];
	envnamestr[1] = (char *)gtmenvname[envindx];
	/* Assert that there is always a non-null ydb* env var. Converse is not always true. */
	assert('\0' != envnamestr[0][0]);
	for (i = 0; i < 2; i++)
	{
		envstr = (const char *)envnamestr[i];
		if ('\0' == envstr[0])
			continue;
		envstr++;	/* to skip leading $ in env var name in ydbenvname/gtmenvname array */
		if (NULL != suffix)
		{
			/* Note: Not using SNPRINTF macro here since "ydb_getenv" is used by gtmsecshr_wrapper, encryption plugin
			 * etc. and using SNPRINTF causes a lot more functions to be pulled into those executables and bloats
			 * their size, all because of this macro. Hence duplicating the macro code (EINTR handling) below.
			 */
			do
			{	/* Note: Not using SIZEOF because it requires "gtm_sizeof.h", yet another header file
				 * that needs to be installed as part of the encryption plugin sources.
				 */
				nbytes = snprintf(envwithsuffix, sizeof(envwithsuffix), "%s%.*s",
									envstr, suffix->len, suffix->addr);
				if ((-1 != nbytes) || (EINTR != errno))
					break;
				/* The following function invocation is commented out because "ydb_getenv" is also used by the
				 * encryption plugin which does not have access to the below function.
				 * That said, it is okay not to invoke it since there is no indefinite sleep loop here
				 * that can cause a process to not terminate in a timely fashion even though it was say
				 * sent a process-terminating signal like SIGTERM.
				 *
				 * eintr_handling_check();
				 */
			} while (TRUE);
			/* For the same reasons mentioned above for "eintr_handling_check()", the following macro is also not
			 * invoked here. But needs to be invoked by the caller of "ydb_getenv()".
			 *
			 * DEFERRED_SIGNAL_HANDLING_CHECK;
			 */
			if ((0 > nbytes) || (nbytes >= sizeof(envwithsuffix)))
			{	/* Error return from SNPRINTF or output was truncated. Move on to next env var */
				continue;
			}
			envstr = (const char *)envwithsuffix;
			assert('\0' == envstr[nbytes]);
		}
		ret = getenv(envstr);
		if (NULL == ret)
			continue;
		if (NULL != is_ydb_env_match)
			*is_ydb_env_match = (0 == i) ? TRUE : FALSE;
		return ret;
	}
	return NULL;
}
