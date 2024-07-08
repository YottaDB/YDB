/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "send_msg.h"
#include "caller_id.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#include "gtm_stdlib.h"
#include "util.h"
#endif

error_def(ERR_MEMORY);
#define CALLERID ((unsigned char *)caller_id(0))

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
	unsigned char	*napage, *stpbase;
	size_t		allocbytes = size + SIZEOF(char *) + 2 * OS_PAGE_SIZE;

	/* Allocate the size requested plus one longword so that loops that index through the stringpool can go one
	 * iteration beyond the end of the stringpool without running off the end of the allocated memory region.
	 * After the requested size plus the extra longword, allocate an additional region two machine pages int4 in
	 * order to ensure that this additional region contains at least one aligned machine page; mark that aligned
	 * page non-accessible so that memory accesses too far beyond the intended end of the stringpool (caused by
	 * "runaway" loops, for example) will cause ACCVIO errors.
	 */
	stringpool.base = stringpool.free = (unsigned char *)stp_mmap(allocbytes);
	stringpool.lastallocbytes = allocbytes;
	napage = (unsigned char *)((((UINTPTR_T)stringpool.base + stringpool.lastallocbytes) & ~(OS_PAGE_SIZE - 1)) - OS_PAGE_SIZE);
	stringpool.top = stringpool.invokestpgcollevel = napage - SIZEOF(char *);
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
