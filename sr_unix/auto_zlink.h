/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef AUTO_ZLINK_INCLUDED
#define AUTO_ZLINK_INCLUDED

#include <rtnhdr.h>	/* see HDR_FILE_INCLUDE_SYNTAX comment in mdef.h for why <> syntax is needed */

void auto_zlink(int rtnhdridx);
void auto_relink_check(int rtnhdridx, int lbltblidx);
void explicit_relink_check(rhdtyp *rhd, boolean_t setproxy);

#endif
