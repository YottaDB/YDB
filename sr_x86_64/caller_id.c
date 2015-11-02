/****************************************************************
 *								*
 *	Copyright 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* caller_id routine called from CRIT_TRACE macro to
   return the return address of our caller allowing CRIT_TRACE
   (used in various semaphore routines) to determine who was
   calling those semaphore routines and for what purpose and
   when. This is a system dependent type of operation and is
   generally implemented in assembly language.
   Presently 32bit linux system has its own implementation in
   assembly. Similar implementation will not work on x86_64
   since register rbp is also used as M Frame pointer in its
   assembly files.
   This particular implementation will work only on Linux x86_64 system
   due to its dependency on "backtrace" function call which is not
   available on all Unix flovours.*/

#include <execinfo.h>
#include "gtm_stdlib.h"
#include "caller_id.h"

#define MAX_TRACE_DEPTH		3
/*We need the callers caller of caller_id */
#define RETURN_ADDRESS_DEPTH	2

caddr_t caller_id(void)
{
	void *trace[MAX_TRACE_DEPTH];
	int trace_size;

	trace_size = backtrace(trace, MAX_TRACE_DEPTH);

/* backtrace will return call stack with address.*/
	if (trace_size >= RETURN_ADDRESS_DEPTH)
		return (caddr_t)trace[RETURN_ADDRESS_DEPTH];
	else
		return NULL;
}
