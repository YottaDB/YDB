/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Generate a core if we need one and haven't done it yet */

#include "mdef.h"
#include "error.h"
#include "gtmdbglvl.h"
#include "libyottadb_int.h"

GBLREF boolean_t	created_core;
GBLREF boolean_t	dont_want_core;
GBLREF boolean_t	need_core;
GBLREF uint4		ydbDebugLevel;
#ifdef DEBUG
GBLREF boolean_t	multi_thread_in_use;
GBLREF boolean_t	simpleThreadAPI_active;
#endif

/* Create our own version of the DUMP macro that does not include stack overflow. This
   error is handled better inside mdb_condition_handler which should be the top level
   handler whenever that error is raised. I would add an assert for that but this would
   force mdb_condition_handler to be included in all the images we build forcing them
   to be larger than they should be by pulling in the transfer table referenced in
   mdb_condition_handler. Not doing the dump here does not prevent the core from occuring,
   it just delays where it would occur should ERR_STACKOFLOW be signaled from a utility
   routine for some reason. Note that the DUMP macro below is defined in error.h and is
   expanded as part of the DUMPABLE macro below (10/2000 se).

   Since ERR_STACKOFLOW has the type of fatal, we must explicitly check that this error
   is NOT ERR_STACKOFLOW. 1/2001 se.
   The ERR_MEMORY error now gets same treatment as ERR_STACKOFLOW 2008-01-11 se.
*/
#undef DUMP
#define DUMP	(   SIGNAL == (int)ERR_ASSERT		\
		 || SIGNAL == (int)ERR_GTMASSERT	\
		 || SIGNAL == (int)ERR_GTMASSERT2	\
		 || SIGNAL == (int)ERR_GTMCHECK)	/* BYPASSOK */

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);

void ch_cond_core(void)
{
	boolean_t	cond_core_signal;

	cond_core_signal = (ERR_STACKOFLOW == SIGNAL) || (ERR_MEMORY == SIGNAL);
	if (DUMPABLE
		&& ((cond_core_signal && (GDL_DumpOnStackOFlow & ydbDebugLevel) && !IS_SIMPLEAPI_MODE) || !cond_core_signal)
		&& !SUPPRESS_DUMP)
	{
		need_core = TRUE;
#		ifdef DEBUG
		if (simpleThreadAPI_active || multi_thread_in_use)
		{	/* Either of the conditions in the "if" check imply this process has more than one thread active.
			 * If we do a "fork_n_core", we would only get the C-stack of the current thread (since a "fork"
			 * does not inherit the C-stack of all active threads). Therefore in debug builds at least,
			 * dump a core right away so we have more information for debugging.
			 */
			DUMP_CORE;	/* will not return */
		}
#		endif
		gtm_fork_n_core();
	}

}
