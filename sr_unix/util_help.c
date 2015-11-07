/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "util_help.h"
#include "gtm_limits.h" /* for GTM_PATH_MAX */
#include "gtm_stdio.h"  /* for snprintf() */
#include "gtm_string.h" /* for strlen() */
#include "gtm_stdlib.h" /* for SYSTEM() */
#include "gtmimagename.h" /* for struct gtmImageName */

GBLREF	char			gtm_dist[GTM_PATH_MAX];
GBLREF	boolean_t		gtm_dist_ok_to_use;
LITREF	gtmImageName		gtmImageNames[];

error_def(ERR_TEXT);
error_def(ERR_GTMDISTUNVERIF);

#define HELP_CMD_STRING_SIZE 256 + GTM_PATH_MAX + GTM_PATH_MAX
#define EXEC_GTMHELP	"%s/mumps -run %%XCMD 'do ^GTMHELP(\"%s\",\"%s/%shelp.gld\")'",

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
	if (!gtm_dist_ok_to_use)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GTMDISTUNVERIF, 4, LEN_AND_STR(gtm_dist),
				gtmImageNames[image_type].imageNameLen, gtmImageNames[image_type].imageName);
	if (0 == TREF(parms_cnt))
		help_option = utilImageGLDs[INVALID_IMAGE];
	else
	{
		assert(TAREF1(parm_ary, TREF(parms_cnt) - 1));
		assert((char *)-1L != (TAREF1(parm_ary, TREF(parms_cnt) - 1)));
		help_option = (TAREF1(parm_ary, TREF(parms_cnt) - 1));
	}
	/* if help_cmd_string is not long enough, the following command will fail */
	SNPRINTF(help_cmd_string, SIZEOF(help_cmd_string), EXEC_GTMHELP
			gtm_dist, help_option, gtm_dist, utilImageGLDs[image_type]);
	rc = SYSTEM(help_cmd_string);
	if (0 != rc)
		rts_error_csa(NULL, VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("HELP command error"), rc);
}

