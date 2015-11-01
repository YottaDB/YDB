/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_MALLOC_H__included
#define GTM_MALLOC_H__included

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

#endif /* GTM_MALLOC_H_included */
