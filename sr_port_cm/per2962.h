/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* per2962.h - definitions needed to test PER 2962 */

#define PER2962_LOG_ENTRIES	500
#define PER2962_LOG_ENTRY_SIZE	5


#define PER2962_INSERT(ID, arg1, arg2, arg3, arg4)				\
	per2962_circ_log[per2962_in + 0] = (long)(ID);				\
	per2962_circ_log[per2962_in + 1] = (long)(arg1);			\
	per2962_circ_log[per2962_in + 2] = (long)(arg2);			\
	per2962_circ_log[per2962_in + 3] = (long)(arg3);			\
	per2962_circ_log[per2962_in + 4] = (long)(arg4);			\
										\
	per2962_in += PER2962_LOG_ENTRY_SIZE;					\
	if (per2962_in > (PER2962_LOG_ENTRIES - 1)*PER2962_LOG_ENTRY_SIZE)	\
	{									\
		per2962_in = 0;							\
		per2962_lost++;							\
	}
