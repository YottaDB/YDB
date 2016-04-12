/****************************************************************
 *								*
 * Copyright (c) 2006-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "gtm_utf8.h"
#ifdef UNICODE_SUPPORTED
# include "utfcgr.h"
#else
# include "utfcgr_trc.h"
#endif

#define OP_FNEXTRACT op_fnextract
#include "op_fnextract.h"

