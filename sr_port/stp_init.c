/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "mem_access.h"

GBLREF spdesc	stringpool;
OS_PAGE_SIZE_DECLARE

static unsigned char	*lasttop = 0;

void stp_init(unsigned int size)
{
	unsigned char	*na_page[2];

	if (lasttop != 0)
	{
		na_page[0] = na_page[1]
			   = stringpool.top + SIZEOF(char *);
		reset_access(na_page, stringpool.prvprt);
	}

	/* Allocate the size requested plus one longword so that loops that index through the stringpool can go one
	   iteration beyond the end of the stringpool without running off the end of the allocated memory region.
	   After the requested size plus the extra longword, allocate an additional region two machine pages int4 in
	   order to ensure that this additional region contains at least one aligned machine page; mark that aligned
	   page non-accessible so that memory accesses too far beyond the intended end of the stringpool (caused by
	   "runaway" loops, for example) will cause ACCVIO errors.
	*/
	stringpool.base = stringpool.free
                = (unsigned char *)malloc(size + SIZEOF(char *) + 2 * OS_PAGE_SIZE);

        na_page[0] = na_page[1]
                   = (unsigned char *)
                ((((UINTPTR_T)stringpool.base + size + SIZEOF(char *) + 2 * OS_PAGE_SIZE) & ~(OS_PAGE_SIZE - 1)) - OS_PAGE_SIZE);
	stringpool.lasttop = lasttop;
	lasttop = stringpool.top
		= na_page[0] - SIZEOF(char *);

	set_noaccess (na_page, &stringpool.prvprt);

	return;
}
