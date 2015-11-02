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

/*
 * This file contains functions related to Linux HugeTLB support. Its functions rely
 * on libhugetlbfs which allocates memory in Huge Pages.
 * This library is Linux only and works only on (currently) x86, AMD64 and PowerPC
 * Supported Huge Page functionality requires the following prerequisites:
 *	Linux kernel support of Huge Pages
 *	x86_64 or i386 architecture
 *	libhugetlbfs.so being installed
 *	Availability of Huge Pages through setting value to /proc/sys/vm/nr_hugepages or hugepages=<n> kernel parameter or
 *		/proc/sys/vm/nr_overcommit_hugepages
 *	In order to use shmget with Huge Pages, either the process gid should be in /proc/sys/vm/hugetlb_shm_group or the
 *		process should have CAP_IPC_LOCK
 *	In order to remap .text/.data/.bss sections, a file system of type hugetlbfs should be mounted
 *	Appropriate environmental variables should be set (refer to libhugetlbfs documentation) to enable/disable Huge Pages
 */
#include "mdef.h"

#include <dlfcn.h>
#include "gtm_string.h"

#include "get_page_size.h"
#include "hugetlbfs_overrides.h"
#undef shmget
#include "send_msg.h"
#include "wbox_test_init.h"
#ifdef DEBUG
#	define WBTEST_HUGETLB_DLSYM_ERROR "WBTEST_HUGETLB_DLSYM error"
#endif

GBLDEF long	gtm_os_hugepage_size = -1;	/* Default Huge Page size of OS. If huge pages are not supported or the
 	 	 	 	 	 	 * value doesn't fit into a *long* it will be equal to the OS page size
 	 	 	 	 	 	 */
OS_PAGE_SIZE_DECLARE

/* ptr to libhugetlbfs's overriden shmget. It uses Linux Huge Pages to back the shared segment if possible */
STATICDEF int		(*p_shmget) (key_t, size_t, int) = NULL;
/* returns default huge page size of the OS or -1 in case huge pages are not supported or their sizes doesn't
 * fit into a long. Refer to libhugetlbfs for further info. */
STATICDEF long		(*p_gethugepagesize) (void) = NULL;
STATICDEF boolean_t	hugetlb_is_attempted = FALSE;
/* all shmget declarations have already been MACROed to gtm_shmget in mdefsp.h so we need to declare the real
 * one here */
extern int 		shmget (key_t __key, size_t __size, int __shmflg);

error_def(ERR_DLLNORTN);
error_def(ERR_TEXT);

/* A MACRO in mdefsp.h (LINUX_ONLY) replaces all shmget with this function */
int gtm_shmget (key_t key, size_t size, int shmflg)
{
	assert(hugetlb_is_attempted);		/* libhugetlbfs_init must be called prior to this function */
	return p_shmget(key, size, shmflg);
}

/*
 * This function initializes libhugetlbfs if it's available. Upon dlopen() the initializing function of libhugetlbfs
 * is called. If libhugetlbfs is available gtm_shmget uses its shmget. Otherwise it falls back to the native shmget.
 * For malloc to use hugepages, it calls __morecore() hook if it needs more memory. In case libhugetlbfs is available
 * and other Huge Page conditions are met, the libhugetlbfs assigns __morecore() to a version which backs them with
 * hugepages during its initialization
 * Consult libhugetlbfs documentation for a list of HugeTLB configuration environment variables.
 */
void libhugetlbfs_init(void)
{
	char 	*error = NULL;
	void	*handle;

	assert(!hugetlb_is_attempted);
	handle = dlopen("libhugetlbfs.so", RTLD_NOW);
	GTM_WHITE_BOX_TEST(WBTEST_HUGETLB_DLOPEN, handle, NULL);
	if (NULL != handle)
	{
		/* C99 standard leaves casting from "void *" to a function pointer undefined. The assignment used
		 * below is the POSIX.1-2003 (Technical Corrigendum 1) workaround; */
		*(void **) (&p_shmget) = dlsym(handle, "shmget");
		GTM_WHITE_BOX_TEST(WBTEST_HUGETLB_DLSYM, p_shmget, NULL);
		if (NULL != p_shmget)		/* NULL value for shmget() necessarily means it was not found */
		{
			*(void **) (&p_gethugepagesize) = dlsym(handle, "gethugepagesize");
			if (NULL != p_gethugepagesize)
				gtm_os_hugepage_size = p_gethugepagesize();
			else
				error = dlerror();
		} else
			error = dlerror();
		GTM_WHITE_BOX_TEST(WBTEST_HUGETLB_DLSYM, error, WBTEST_HUGETLB_DLSYM_ERROR);
		if (error)
		{
			p_shmget = NULL;
			send_msg(VARLSTCNT(8) ERR_DLLNORTN, 2, LEN_AND_LIT("shmget from libhugetlbfs.so"), ERR_TEXT, 2,
					LEN_AND_STR(error));
		}
	}
	if (NULL == p_shmget)
		p_shmget = &shmget;			/* Fall back to using the native shmget */
	get_page_size();
	if (-1 == gtm_os_hugepage_size)
		gtm_os_hugepage_size = OS_PAGE_SIZE;
	assert(0 == (gtm_os_hugepage_size % OS_PAGE_SIZE));	/* huge pages sizes are multiples of page sizes */
	hugetlb_is_attempted = TRUE;
}
