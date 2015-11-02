/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SLEEP_H
#define SLEEP_H

/* Note: GT.M code *MUST*NOT* make use of the sleep() function because use of the sleep() function
   causes problems with GT.M's timers on some platforms. Specifically, the sleep() function
   causes the SIGARLM handler to be silently deleted on Solaris systems (through Solaris 9 at least).
   This leads to lost timer pops and has the potential for system hangs. The proper long sleep mechanism
   is hiber_start which can be accessed through the LONG_SLEEP macro defined in mdef.h.
 */

int m_sleep(int seconds);
int m_usleep(int useconds);
int m_nsleep(int nseconds);

#ifdef UNIX

# if !defined(_AIX) && !defined(__osf__) && !defined(__hpux) && !defined(__sparc) && !defined(_UWIN) && !defined(__linux__)
#   if !defined(__MVS__) && !defined(__CYGWIN__)
#      error "Unsure of support for sleep functions on this platform"
#   endif
# endif

# ifdef _AIX
   typedef struct timestruc_t m_time_t;
#  define nanosleep_func nsleep
# endif

# if defined(__sparc) || defined(__hpux) || defined(__osf__) || defined (__linux__) || defined (__CYGWIN__)
   typedef struct timespec m_time_t;
#  define nanosleep_func nanosleep
# endif

# ifdef _UWIN
# include "iotcp_select.h"
# define usleep_func gtm_usleep
# endif

# ifdef __MVS__
   typedef struct timespec m_time_t;
#  define nanosleep_func usleep			/* m_nsleep will not work on OS390, but it is not used */
# endif

#else

# error  "Unsure of support for sleep functions on this non-UNIX platform"

#endif

#endif /* SLEEP_H */
