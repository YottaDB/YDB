/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef HUGETLBFS_OVERRIDES_H_
#define HUGETLBFS_OVERRIDES_H_

#if defined(__linux__) && ( defined(__i386__) || defined(__x86_64__) )
#	define HUGETLB_SUPPORTED	1
#endif

GBLREF long	gtm_os_hugepage_size;
#define		OS_HUGEPAGE_SIZE gtm_os_hugepage_size

extern int	gtm_shmget(key_t __key, size_t __size, int __shmflg);
void 		libhugetlbfs_init(void);

#endif /* HUGETLBFS_OVERRIDES_H_ */
