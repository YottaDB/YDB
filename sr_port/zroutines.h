/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZROUTINES_H_INCLUDED
#define ZROUTINES_H_INCLUDED

#include "zroutinessp.h"

#define ZRO_MAX_ENTS		4096
#define ZRO_TYPE_COUNT		1
#define ZRO_TYPE_OBJECT		2
#define ZRO_TYPE_SOURCE		3
#define ZRO_TYPE_OBJLIB		4

void zro_init(void);
void zro_load(mstr *str);

#endif
