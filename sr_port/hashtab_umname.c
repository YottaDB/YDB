/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "send_msg.h"
#include "gdsfhead.h"

#define UMNAME_HASH
#include "hashtab_umname.h"
/* The below include generates the hash table routines for the "mname" hash type */
#include "hashtab_implementation.h"

#undef UMNAME_HASH
