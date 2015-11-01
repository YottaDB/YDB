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

#ifndef __FGNCAL_H__
#define __FGNCAL_H__

mval *fgncal_lookup(mval *x);
void fgncal_unwind(void);
void fgncal_rundown(void);

#include "fgncalsp.h"

#endif
