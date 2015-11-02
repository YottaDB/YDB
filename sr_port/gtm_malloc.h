/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_MALLOC_H__included
#define GTM_MALLOC_H__included

/* Malloc of areas containing executable code need to be handled differently on some platforms
   (currently only Linux but likely others in the future)
*/
#ifdef __linux__
#  define GTM_TEXT_MALLOC(x) gtm_text_malloc(x)
void *gtm_text_malloc(size_t size);
void *gtm_text_malloc_dbg(size_t size);
# else
#  define GTM_TEXT_MALLOC(x) gtm_malloc(x)
#endif

void verifyFreeStorage(void);
void verifyAllocatedStorage(void);

#define VERIFY_STORAGE_CHAINS			\
{						\
	GBLREF uint4	gtmDebugLevel;		\
	if (GDL_SmFreeVerf & gtmDebugLevel)	\
		verifyFreeStorage();		\
	if (GDL_SmAllocVerf & gtmDebugLevel)	\
		verifyAllocatedStorage();	\
}

#endif /* GTM_MALLOC_H__included */
