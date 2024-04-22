/****************************************************************
 *								*
 * Copyright (c) 2014-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef HUGETLBFS_OVERRIDES_H_
#define HUGETLBFS_OVERRIDES_H_

#if ( defined(__linux__) && ( defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)) )
#	define	HUGETLB_SUPPORTED	1

	GBLREF	long gtm_os_hugepage_size;
#	define	OS_HUGEPAGE_SIZE gtm_os_hugepage_size

#	define	ADJUST_SHM_SIZE_FOR_HUGEPAGES(SRCSIZE, DSTSIZE) DSTSIZE = ROUND_UP(SRCSIZE, OS_HUGEPAGE_SIZE)
#else
	OS_PAGE_SIZE_DECLARE
#	define	ADJUST_SHM_SIZE_FOR_HUGEPAGES(SRCSIZE, DSTSIZE) DSTSIZE = ROUND_UP(SRCSIZE, OS_PAGE_SIZE)
#endif

extern int	gtm_shmget(key_t key, size_t size, int shmflg, bool lock);

#endif /* HUGETLBFS_OVERRIDES_H_ */
