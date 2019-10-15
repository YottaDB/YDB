/****************************************************************
 *								*
* Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#ifndef XOSHIRO_H_INCLUDED
#define XOSHIRO_H_INCLUDED

#include <stdint.h>
#include "gtm_stdio.h"
#include "gtm_common_defs.h"

uint64_t sm64_next(void);
uint64_t x256_next(void);
void x256_jump(void);
void x256_long_jump(void);

#endif /* XOSHIRO_H_INCLUDED */
