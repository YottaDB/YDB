/****************************************************************
 *								*
 * Copyright 2001, 2006 Fidelity Information Services, Inc	*
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

#ifndef LOGICAL_TRUTH_VALUE_H_INCLUDED
#define LOGICAL_TRUTH_VALUE_H_INCLUDED

#define LOGICAL_TRUE	"TRUE"
#define LOGICAL_YES	"YES"

#define LOGICAL_FALSE	"FALSE"
#define LOGICAL_NO	"NO"

#include "ydb_logicals.h"	/* for "ydbenvindx_t" */

boolean_t ydb_logical_truth_value(ydbenvindx_t envindx, boolean_t negate, boolean_t *is_defined);

#endif /* LOGICAL_TRUTH_VALUE_H_INCLUDED */
