/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
<<<<<<< HEAD
||||||| parent of 07a11a26bb (GT.M V7.1-004)
#include "mem_access.h"
=======
#include "mem_access.h"
#include "send_msg.h"
#include "caller_id.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#include "gtm_stdlib.h"
#include "util.h"
#endif

error_def(ERR_MEMORY);
#define CALLERID ((unsigned char *)caller_id(0))
>>>>>>> 07a11a26bb (GT.M V7.1-004)

GBLREF spdesc	stringpool;
GBLREF size_t	totalRmalloc, totalRallocGta, zmalloclim;
OS_PAGE_SIZE_DECLARE

void *stp_mmap(size_t size)
{
	void	*stpbase, *dummy;
	int	save_errno;

	assert(0 < size);
	stpbase = (unsigned char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (MAP_FAILED == stpbase)
	{
		save_errno = errno;
		if (ENOMEM == save_errno)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_MEMORY, 2, size, CALLERID, save_errno);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("mmap()"), CALLFROM, save_errno);
	}
	if ((0 < zmalloclim) && ((size + totalRmalloc + totalRallocGta) > zmalloclim))
	{	/* If we reach the zalloc limit use the gtm_malloc_src mechanism to issue a MALLOCCRIT warning */
		dummy = (void *)malloc(size);
		free(dummy);
	}
	totalRallocGta += size;
	return stpbase;
}

void stp_init(size_t size)
{
<<<<<<< HEAD
||||||| parent of 07a11a26bb (GT.M V7.1-004)
	unsigned char	*na_page[2];

	if (lasttop != 0)
	{
		na_page[0] = na_page[1]
			   = stringpool.top + SIZEOF(char *);
		reset_access(na_page, stringpool.prvprt);
	}
=======
	unsigned char	*napage, *stpbase;
	size_t		allocbytes = size + SIZEOF(char *) + 2 * OS_PAGE_SIZE;

>>>>>>> 07a11a26bb (GT.M V7.1-004)
	/* Allocate the size requested plus one longword so that loops that index through the stringpool can go one
	 * iteration beyond the end of the stringpool without running off the end of the allocated memory region.
	 */
<<<<<<< HEAD
	size = ROUND_UP2(size, 8);
	stringpool.base = stringpool.free = (unsigned char *)malloc(size);
	stringpool.lasttop = lasttop;
	lasttop = stringpool.top = stringpool.invokestpgcollevel = stringpool.base + size;
||||||| parent of 07a11a26bb (GT.M V7.1-004)
	stringpool.base = stringpool.free
                = (unsigned char *)malloc(size + SIZEOF(char *) + 2 * OS_PAGE_SIZE);
        na_page[0] = na_page[1]
                   = (unsigned char *)
                ((((UINTPTR_T)stringpool.base + size + SIZEOF(char *) + 2 * OS_PAGE_SIZE) & ~(OS_PAGE_SIZE - 1)) - OS_PAGE_SIZE);
	stringpool.lasttop = lasttop;
	lasttop = stringpool.top = stringpool.invokestpgcollevel
		= na_page[0] - SIZEOF(char *);
	set_noaccess (na_page, &stringpool.prvprt);
=======
	stringpool.base = stringpool.free = (unsigned char *)stp_mmap(allocbytes);
	stringpool.lastallocbytes = allocbytes;
	napage = (unsigned char *)((((UINTPTR_T)stringpool.base + stringpool.lastallocbytes) & ~(OS_PAGE_SIZE - 1)) - OS_PAGE_SIZE);
	stringpool.top = stringpool.invokestpgcollevel = napage - SIZEOF(char *);
>>>>>>> 07a11a26bb (GT.M V7.1-004)
	stringpool.gcols = 0;
	return;
}

void stp_fini(unsigned char *strpool_base, size_t size)
{
	int		save_errno;
#	ifdef DEBUG
	FILE		*pf;
	char		buff[50], line[20], *fgets_res;
	unsigned int	rss_before, rss_after;

	if (WBTEST_ENABLED(WBTEST_MUNMAP_FREE) && (2 == gtm_white_box_test_case_count))
	{
		SNPRINTF(buff, SIZEOF(buff), "ps -p %u -o rssize | grep -v RSS", getpid());
		pf = POPEN(buff ,"r");
		memset(line, 0, SIZEOF(line));
		FGETS(line, SIZEOF(line), pf, fgets_res);
		assert(NULL != fgets_res);
		rss_before = STRTOUL(line, NULL, 10);		/* use a decimal(10) base */
		munmap(strpool_base, size);
		pf = POPEN(buff ,"r");
		memset(line, 0, SIZEOF(line));
		FGETS(line, SIZEOF(line), pf, fgets_res);
		assert(NULL != fgets_res);
		rss_after = STRTOUL(line, NULL, 10);		/* use a decimal(10) base */
		if (rss_before > rss_after)
		{
			if (size < stringpool.lastallocbytes)
				util_out_print("TEST-I-MEM : after expansion munmap() returned !UL kB to the OS",
						TRUE, (rss_before - rss_after));
			else
				util_out_print("TEST-I-MEM : after contraction munmap() returned !UL kB to the OS",
						TRUE, (rss_before - rss_after));
		}
		totalRallocGta -= size;
		return;
	}
#	endif
	if (-1 == munmap(strpool_base, size))
	{
		save_errno = errno;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("munmap()"), CALLFROM, save_errno);
	}
	totalRallocGta -= size;
	return;
}
