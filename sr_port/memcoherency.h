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

#ifndef MEMCOHERENCY_H_INCLUDED
#define MEMCOHERENCY_H_INCLUDED

#ifdef __alpha

#include <c_asm.h>

/* Read Alpha Architecture Reference Manual, edited by Richard L Sites,
 * Chapter "System Architecture and Programming Implications" for memory
 * coherency issues and behavior of "mb" instruction (memory barrier)
 */

GBLREF	int	num_additional_processors;

#define COMMIT_SHM_UPDATES											\
{														\
	if (num_additional_processors)	/* for Uniprocessor systems, no need for "memory barrier"	*/	\
		asm("mb");		/* as memory is always coherent					*/	\
}

#define INVALIDATE_DATA_CACHE											\
{														\
	if (num_additional_processors)	/* for Uniprocessor systems, no need for "memory barrier"	*/	\
		asm("mb");		/* as memory is always coherent					*/	\
}

#else

#define COMMIT_SHM_UPDATES
#define INVALIDATE_DATA_CACHE

#endif

#endif /* MEMCOHERENCY_H_INCLUDED */
