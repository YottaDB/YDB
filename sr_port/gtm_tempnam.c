/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_tempnam.h"

/**************************************************************
 * gtm_tempnam()
 * Generates a temporary file name and places it in fullname
 * Note that fullname should be malloced by the caller
 * It is assumed to be of proper size (MAX_FN_LEN + 1)
 *
 **************************************************************/

void gtm_tempnam(char *dir, char *prefix, char *fullname)
{
	static	int	temp_file_counter = 0;
	int		len;
	char		*ptr, def_prefix[] = "GTM_TEMP_";

	if (NULL == dir)
	{
		len = SIZEOF(SCRATCH_DIR) - 1;
		memcpy(fullname, SCRATCH_DIR, len);
	} else
	{
		len = STRLEN(dir);
		memcpy(fullname, dir, len);
	}
	ptr = fullname + len;
	if (NULL == prefix)
	{
		prefix = def_prefix;
		len = SIZEOF(def_prefix) - 1;
		memcpy(ptr, def_prefix, len);
	} else
	{
		len = STRLEN(prefix);
		memcpy(ptr, prefix, len);
	}
	ptr += len;
	SPRINTF(ptr, "_%d.tmp", temp_file_counter++);
}
