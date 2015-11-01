/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __ZROUTINES_H__
#define __ZROUTINES_H__

#include "zroutinessp.h"

void zro_init(void);
void zro_load(mstr *str);
void zro_search (mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir);

#endif
