/****************************************************************
 *								*
 *	Copyright 2005, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/****************************************************************
        DBCERTIFY - main driver

        Parse arguments, invoke required phase routine.

        Note: Most routines in this utility are self-contained
              meaning they do not reference GT.M library routines
              (with some notable exceptions). This is because
              phase-1 is going to run against live V4 databases
              and the V5 compilation will be using V5 database
              structures.
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include <signal.h>

#ifdef UNIX
#include "sig_init.h"
#else
#include "desblk.h"		/* for desblk structure */
#endif
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblkops.h"
#include "mupip_exit.h"
#include "gtmimagename.h"
#include "error.h"
#include "iosp.h"
#include "gtm_env_init.h"
#include "dbcertify.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLDEF	phase_static_area	*psa_gbl;			/* Global anchor for static area */

GBLREF  enum gtmImageTypes      image_type;
GBLREF	uint4			process_id;
GBLREF	boolean_t		gtm_utf8_mode;
#ifdef VMS
GBLREF	desblk			exi_blk;
GBLREF	int4			exi_condition;
#endif

int UNIX_ONLY(main)VMS_ONLY(dbcertify)(int argc, char **argv)
{
	/* Initialization of scaffolding we run on */
	image_type = DBCERTIFY_IMAGE;
	UNICODE_ONLY(gtm_wcswidth_fnptr = gtm_wcswidth;)
	gtm_env_init();
	psa_gbl = malloc(sizeof(*psa_gbl));
	memset(psa_gbl, 0, sizeof(*psa_gbl));
	UNIX_ONLY(err_init(dbcertify_base_ch));
	UNICODE_ONLY(
	if (gtm_utf8_mode)
		gtm_icu_init();	 /* Note: should be invoked after err_init (since it may error out) and before CLI parsing */
	)
	UNIX_ONLY(sig_init(dbcertify_signal_handler, dbcertify_signal_handler, NULL));
	VMS_ONLY(util_out_open(0));
	VMS_ONLY(SET_EXIT_HANDLER(exi_blk, dbcertify_exit_handler, exi_condition));	/* Establish exit handler */
	VMS_ONLY(ESTABLISH(dbcertify_base_ch));
	process_id = getpid();

	/* Structure checks .. */
	assert((24 * 1024) == sizeof(v15_sgmnt_data));	/* Verify V4 file header hasn't suddenly increased for some odd reason */

	/* Platform dependent method to get the option scan going and invoke necessary driver routine */
	dbcertify_parse_and_dispatch(argc, argv);
	return SS_NORMAL;
}
