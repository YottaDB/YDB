/****************************************************************
 *								*
 * Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

#ifndef __ZROUTINES_H__
#define __ZROUTINES_H__

#include <rtnhdr.h>	/* see HDR_FILE_INCLUDE_SYNTAX comment in mdef.h for why <> syntax is needed */
#include "zroutinessp.h"

#define ZRO_MAX_ENTS		4096
#define ZRO_TYPE_COUNT		1
#define ZRO_TYPE_OBJECT		2
#define ZRO_TYPE_SOURCE		3
#define ZRO_TYPE_OBJLIB		4

void zro_init(void);
void zro_load(mstr *str);

#endif
