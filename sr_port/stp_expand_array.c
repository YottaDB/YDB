/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "longcpy.h"

GBLREF mstr **stp_array;
GBLREF int stp_array_size;

void stp_expand_array(void)
{
	mstr **a;
	int n;

	n = stp_array_size;
	stp_array_size += STP_MAXITEMS;
	a = stp_array;
	stp_array = (mstr **) malloc(stp_array_size * SIZEOF(mstr *));
	longcpy((uchar_ptr_t)stp_array, (uchar_ptr_t)a, n * SIZEOF(mstr *));
	free(a);
	return;
}
