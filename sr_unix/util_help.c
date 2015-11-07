/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "util_help.h"
#include "gtm_stdio.h"  /* for snprintf() */
#include "gtm_string.h" /* for strlen() */
#include "gtm_stdlib.h" /* for SYSTEM() */
#include "gtmimagename.h" /* for struct gtmImageName */


error_def(ERR_TEXT);

#define HELP_CMD_STRING_SIZE 512
#define EXEC_GTMHELP	"$gtm_dist/mumps -run %%XCMD 'do ^GTMHELP(\"%s\",\"$gtm_dist/%shelp.gld\")'",

#define UTIL_HELP_IMAGES	5
/* We need the first two entries for compatibility */
char *utilImageGLDs[UTIL_HELP_IMAGES] =
{
#define IMAGE_TABLE_ENTRY(A,B)  B,
IMAGE_TABLE_ENTRY (INVALID_IMAGE,	"")
IMAGE_TABLE_ENTRY (GTM_IMAGE,		"gtm")
IMAGE_TABLE_ENTRY (MUPIP_IMAGE,		"mupip")
IMAGE_TABLE_ENTRY (DSE_IMAGE,		"dse")
IMAGE_TABLE_ENTRY (LKE_IMAGE,		"lke")
#undef IMAGE_TABLE_ENTRY
};

void util_help(void)
{
	int  rc;
	char *help_option;
	char help_cmd_string[HELP_CMD_STRING_SIZE];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(1 >= TREF(parms_cnt));
	assert(GTM_IMAGE < image_type && UTIL_HELP_IMAGES > image_type);
	if (0 == TREF(parms_cnt))
		help_option = utilImageGLDs[INVALID_IMAGE];
	else
	{
		assert(TAREF1(parm_ary, TREF(parms_cnt) - 1));
		assert((char *)-1L != (TAREF1(parm_ary, TREF(parms_cnt) - 1)));
		help_option = (TAREF1(parm_ary, TREF(parms_cnt) - 1));
	}
	SNPRINTF(help_cmd_string, SIZEOF(help_cmd_string),
			"$gtm_dist/mumps -run %%XCMD 'do ^GTMHELP(\"%s\",\"$gtm_dist/%shelp.gld\")'",
			help_option, utilImageGLDs[image_type]);
	rc = SYSTEM(help_cmd_string);
	if (0 != rc)
		rts_error_csa(NULL, VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("HELP command error"), rc);
}

