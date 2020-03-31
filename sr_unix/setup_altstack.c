/****************************************************************
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/mman.h>
#include "gtm_signal.h"
#include "gtm_stdio.h"

#include "invocation_mode.h"
#include "gtmimagename.h"
#include "sig_init.h"
#include "generic_signal_handler.h"
#include "io.h"
#include "gtmio.h"
#include "sighnd_debug.h"

GBLREF	stack_t			oldaltstack;
GBLREF	char			*altstackptr;
OS_PAGE_SIZE_DECLARE

/* For call-ins and simple (threaded) api, if an alternate stack is defined, see if it is big enough for
 * us to use. As an example, Go (1.12.6 currently) allocates a 32K alternate stack which is completely
 * inappropriate for YDB since jnl_file_close() alone takes 67K in a single stack variable. If the stack
 * is too small, supply a larger one.
 */
void setup_altstack()
{
	int	rc, save_errno;
	stack_t	newaltstack;

	if (MUMPS_CALLIN & invocation_mode)
	{
		rc = sigaltstack(NULL, &oldaltstack);	/* Get current alt stack definition */
		if (0 != rc)
		{
			save_errno = errno;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("sigaltstack()"), CALLFROM,
				      save_errno);
		}
		DBGSIGHND((stderr, "sig_init: Alt stack definition: 0x"lvaddr",  flags: 0x%08lx,  size: %d\n",
			   oldaltstack.ss_sp, oldaltstack.ss_flags, oldaltstack.ss_size));
		if ((0 < oldaltstack.ss_size) && (YDB_ALTSTACK_SIZE > oldaltstack.ss_size))
		{	/* Altstack exists but is too small - make one larger that is aligned (both start and length) */
			assert(0 == (YDB_ALTSTACK_SIZE & (OS_PAGE_SIZE - 1)));	/* Make sure len is page aligned */
			altstackptr = mmap(NULL, YDB_ALTSTACK_SIZE + (OS_PAGE_SIZE * 2), (PROT_READ + PROT_WRITE + PROT_EXEC),
					   (MAP_PRIVATE + MAP_ANONYMOUS), -1, 0);
			if (MAP_FAILED == altstackptr)
			{
				save_errno = errno;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("mmap()"), CALLFROM,
					      save_errno);
			}
			rc = mprotect(altstackptr, OS_PAGE_SIZE, PROT_READ);	/* Protect the first page (bottom) of stack */
			if (0 != rc)
			{
				save_errno = errno;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("mprotect()"), CALLFROM,
					      save_errno);
			}
			/* Protect the last page (top) of stack */
			rc = mprotect(altstackptr + YDB_ALTSTACK_SIZE + OS_PAGE_SIZE, OS_PAGE_SIZE, PROT_READ);
			if (0 != rc)
			{
				save_errno = errno;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("mprotect()"), CALLFROM,
					      errno);
			}
			newaltstack.ss_sp = altstackptr + OS_PAGE_SIZE;
			newaltstack.ss_flags = 0;
			newaltstack.ss_size = YDB_ALTSTACK_SIZE;
			rc = sigaltstack(&newaltstack, NULL);
			if (0 != rc)
			{
				save_errno = errno;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("sigaltstack()"),
					      CALLFROM, save_errno);
			}
			DBGSIGHND((stderr, "sig_init: Changing alt stack to: 0x"lvaddr",  flags: 0x%08lx,  size: %d\n",
				   newaltstack.ss_sp, newaltstack.ss_flags, newaltstack.ss_size));
		}
	}
}
