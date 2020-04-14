/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_unistd.h"
#include "gtm_limits.h"

#include "eintr_wrappers.h"
#include "geteditor.h"
#include "wbox_test_init.h"

GBLDEF mstr editor;

void geteditor(void)
{
	char		*edt, **pedt;
	size_t		len;
	int		iter;
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
		if (0 == ACCESS(edt, (F_OK|X_OK)))	/* File exists and is executable, use it */
			break;
		edt = *pedt++;
	} while (edt);
	WBTEST_ASSIGN_ONLY(WBTEST_BADEDITOR_GETEDITOR, edt, 0);
	if (edt)
	{
		len = strlen(edt);
		if (PATH_MAX < len)			/* 4SCA: access() ensures path is < PATH_MAX */
			len = PATH_MAX;
		editor.len = (mstr_len_t)len;
		if (PATH_MAX >= len)			/* 4SCA: Increment behind a guard */
			len++;
		editor.addr = (char*) malloc(len);	/* must be null terminated */
		memcpy(editor.addr, edt, len);
	} else
		editor.len = 0;
}
