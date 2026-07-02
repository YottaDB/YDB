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

typedef mstr desc_struct;

static_assert(SIZEOF(desc_struct) == SIZEOF(mstr), "desc_struct and mstr size mismatch");

/* Currently, the maximum number of argument placeholders in a message is 16. Certain types of placeholders (such as !AD) require
 * two arguments, length and address, to be passed to the corresponding output function (normally, rts_error, send_msg, or putmsg).
 * We are being safe and taking the maximum number of placeholders as 17, doubling the number for length-address types.
 */
#define MAX_FAO_PARMS 34

/* Since @... type parameters involve 8-byte values, we need an additional slot per each such value on 32-bit platforms, define the
 * number of INTPTR_T-typed slots appropriately for both 64- and 32-bit architectures.
 */
#ifdef GTM64
#  define NUM_OF_FAO_SLOTS			MAX_FAO_PARMS
#else
#  define NUM_OF_FAO_SLOTS			((MAX_FAO_PARMS + 1) / 2 * 3)
#endif
