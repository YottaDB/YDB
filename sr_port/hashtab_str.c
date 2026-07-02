/****************************************************************
 *								*
 * Copyright (c) 2009-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "min_max.h"
#include "gtm_string.h"
#include "error.h"
#include "send_msg.h"
#include "gtmmsg.h"

#define STRING_HASH
#include "hashtab_str.h"
/* The below include generates the hash table routines for the literal hash type */
#include "hashtab_implementation.h"
#undef STRING_HASH
