/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <ssdef.h>
#endif
#ifdef UNIX
#include <signal.h>
#endif

#include "error.h"
#include "jobexam_process.h"
#include "gtmdbglvl.h"
#include "create_fatal_error_zshow_dmp.h"

LITDEF mval gtmfatal_error_filename = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, SIZEOF(GTMFATAL_ERROR_DUMP_FILENAME) - 1,
							  GTMFATAL_ERROR_DUMP_FILENAME, 0, 0);

GBLREF	int4		exi_condition;
GBLREF	uint4		gtmDebugLevel;
GBLREF	volatile int4	gtmMallocDepth;

/* On VMS, SIGNAL is sig->chf$l_sig_name (a parameter to mdb_condition_handler) so needs to be passed in */
void create_fatal_error_zshow_dmp(int4 signal, boolean_t repeat_error)
{
	mval	dummy_mval;
	int4	save_SIGNAL;	/* On UNIX this is exi_condition */

	UNIX_ONLY(PRN_ERROR);
	if (UNIX_ONLY(0 == gtmMallocDepth && ((SIGBUS != exi_condition && SIGSEGV != exi_condition) ||
					      (GDL_ZSHOWDumpOnSignal & gtmDebugLevel)))
	    VMS_ONLY((SS$_ACCVIO != signal) || (GDL_ZSHOWDumpOnSignal & gtmDebugLevel)))
	{	/* If dumpable condition, create traceback file of M stack info and such */
		/* On Unix, we need to push out our error now before we potentially overlay it in jobexam_process() */
		/* Create dump file */
		UNIX_ONLY(save_SIGNAL = SIGNAL); 		/* Signal might be modified by jobexam_process() */
		jobexam_process((mval *)&gtmfatal_error_filename, &dummy_mval);
		UNIX_ONLY(SIGNAL = save_SIGNAL);
	}
}
