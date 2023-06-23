/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef BMM_FIND_FREE_H_INCLUDED
#define BMM_FIND_FREE_H_INCLUDED

#include "gdsroot.h"

block_id bmm_find_free(block_id hint, uchar_ptr_t base_addr, block_id total_bits);

#endif
