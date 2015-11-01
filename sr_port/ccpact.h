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

/*
 *	CCPACT.H is now obsolete;  the code has been moved into CCP.H.
 *	The file can be NIXed once all inclusions of it have been removed.
 */

#if 0

#define CCP_TABLE_ENTRY(A,B,C,D) A,

enum action_code_enum
{
#include "ccpact_tab.h"
	CCPACTION_COUNT
};

#undef CCP_TABLE_ENTRY

/* ccpact_tab auxvalue types to discriminate unions */
#define CCTVNUL 0	/* no value */
#define CCTVSTR 1	/* string */
#define CCTVMBX 2	/* mailbox id */
#define CCTVFIL 3	/* file id */
#define CCTVDBP 4	/* data_base header pointer */
#define CCTVFAB 5	/* pointer to a FAB */

#endif
