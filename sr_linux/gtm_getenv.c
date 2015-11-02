/****************************************************************
*                                                              *
*      Copyright 2007 Fidelity Information Services, Inc *
*                                                              *
*      This source code contains the intellectual property     *
*      of its copyright holder(s), and is made available       *
*      under a license.  If you do not know the terms of       *
*      the license, please stop and do not read further.       *
*                                                              *
****************************************************************/

#ifdef __CYGWIN__

/* In Cygwin 1.54-2 and gdb 6.5.50, the Cygwin environment variables	*
 * are not passed to the program being debugged properly.  getenv()	*
 * only sees the Windows variables.					*
*/

#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

extern char **environ;		/* array of pointers, last has NULL */

char *gtm_getenv(char *varname)
{
	char	*eq, **p;
	size_t	len;

	if (NULL == environ || NULL == varname)
		return NULL;
	len = strlen(varname);
	for (p = environ; *p; p++)
	{
		eq = strchr(*p, '=');
		if (eq && (*p + len) == eq)
		{
			if (!strncasecmp(varname, *p, len))	/* environ names are upcased */
				return (eq + 1);
		}
	}
	return NULL;
}
#endif
