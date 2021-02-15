/****************************************************************
 *								*
 * Copyright (c) 2004-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRANS_NUMERIC_H_INCLUDED
#define TRANS_NUMERIC_H_INCLUDED

uint4 trans_numeric(mstr *log, boolean_t *is_defined, boolean_t ignore_errors);
gtm_uint8 trans_numeric_64(mstr *log, boolean_t *is_defined, boolean_t ignore_errors);

#endif
