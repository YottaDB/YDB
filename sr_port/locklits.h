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

#define MAX_LCKARG 253
#define LOCKED 1
#define ZALLOCATED 2
#define LCK_REQUEST 4
#define ZAL_REQUEST 8
#define PENDING 16
#define COMPLETE 32
#define INCREMENTAL 0x40
#define NEW 0x80

#define DEAD 2
#define PART_DEAD 1
#define NOT_DEAD 0
#define NOT_THERE -1

#define LOCK_SELF_WAKE 100
