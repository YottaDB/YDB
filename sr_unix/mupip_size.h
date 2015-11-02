/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __MUPIP_SIZE_H__
#define __MUPIP_SIZE_H__

#include "cdb_sc.h"

void mupip_size(void);
int4 mu_size_arsample(mval *gn, uint M, boolean_t ar, int seed);
int4 mu_size_impsample(mval *gn, int4 M, int4 seed);
int4 mu_size_scan(mval *gn, int4 level);
enum cdb_sc rand_traverse(double *r);

#endif
