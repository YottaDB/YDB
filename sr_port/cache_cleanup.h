/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CACHE_CLEANUP_DEFINED
#define CACHE_CLEANUP_DEFINED

#define IF_INDR_FRAME_CLEANUP_CACHE_ENTRY(frame_pointer)						\
{													\
	/* See if unwinding an indirect frame*/								\
	if (frame_pointer->flags & SFF_INDCE)								\
		cache_cleanup(frame_pointer);								\
}

#define IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer)					\
{													\
	/* See if unwinding an indirect frame*/								\
	if (frame_pointer->flags & SFF_INDCE)								\
	{												\
		cache_cleanup(frame_pointer);								\
                frame_pointer->flags &= SFF_INDCE_OFF;							\
	}												\
}

void cache_cleanup(stack_frame *sf);

#endif
