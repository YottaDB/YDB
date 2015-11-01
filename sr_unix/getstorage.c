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

#include "gtm_ulimit.h"
#include "gtm_unistd.h"

#if defined(__MVS__) || defined(__linux__)
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include "getstorage.h"

#define ERRSTR "getrlimit()"
error_def(ERR_SYSCALL);

int4	getstorage(void)
{
        struct rlimit	rl;
        int 		save_errno;
	unsigned int	cur_sbrk;
        rlim_t		size;

        if (0 != getrlimit(RLIMIT_DATA,&rl))
        {
                save_errno = errno;
                rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL(ERRSTR),
                          CALLFROM,
                          save_errno);
        }
	cur_sbrk = (unsigned int)sbrk(0); /* Two step conversion to eliminate warnings */
	size = rl.rlim_cur - cur_sbrk;
#ifdef INT8_SUPPORTED
        if(MAXPOSINT4 < size)
                size = MAXPOSINT4;
#endif
        return (int4)size;
}

#else

int4	getstorage(void)
{
	return (int4)(ulimit(3,0) - (ulimit_t)sbrk(0));
}

#endif
