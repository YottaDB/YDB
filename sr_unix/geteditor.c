/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stat.h"

#include "eintr_wrappers.h"
#include "geteditor.h"

GBLDEF mstr editor;

void geteditor(void)
{
	char		*edt, **pedt;
	short		len;
	int		stat_res, iter;
	struct stat	edt_stat;
	char		*editor_list[] =
			{
				"/usr/bin/vi",
				"/usr/ucb/vi",
				"/bin/vi",		/* Linux */
				0			/* this array should be terminated by a 0 */
			};

	edt = GETENV("EDITOR");
	pedt = &editor_list[0];
	do {
		STAT_FILE(edt, &edt_stat, stat_res);
		if (!stat_res)
			break;
		edt = *pedt++;
	} while (edt);

	if (edt)
	{
		len = strlen(edt) + 1;	/* for zero */
		editor.len = len - 1;	/* but not for mstr */
		editor.addr = (char*) malloc(len);	/* must be zero term */
		memcpy(editor.addr, edt, len);
	}
	else
		editor.len = 0;
}
