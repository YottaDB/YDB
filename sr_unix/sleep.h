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

#ifndef SLEEP_H
#define SLEEP_H

int m_sleep(int seconds);
int m_usleep(int useconds);
int m_nsleep(int nseconds);

#ifdef UNIX

# if !defined(_AIX) && !defined(__osf__) && !defined(__hpux) && !defined(__sparc) && !defined(_UWIN) && !defined(__linux__) && !defined(__MVS__)
#  error "Unsure of support for sleep functions on this platform"
# endif

# ifdef _AIX
   typedef struct timestruc_t m_time_t;
#  define nanosleep_func nsleep
# endif

# ifdef __sparc
   typedef struct timespec m_time_t;
#  define nanosleep_func nanosleep
# endif

# ifdef __hpux
   typedef struct timespec m_time_t;
#  define nanosleep_func nanosleep
# endif

# ifdef __osf__
   typedef struct timespec m_time_t;
#  define nanosleep_func nanosleep
# endif

# ifdef _UWIN
# include "iotcp_select.h"
# define usleep_func gtm_usleep
# endif

# ifdef __linux__
   typedef struct timespec m_time_t;
#  define nanosleep_func nanosleep
# endif

# ifdef __MVS__
   typedef struct timespec m_time_t;
#  define nanosleep_func usleep			/* m_nsleep will not work on OS390, but it is not used */
# endif

#else

# error  "Unsure of support for sleep functions on this non-UNIX platform"

#endif

#endif /* SLEEP_H */
