/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef LONGCPY_INCLUDED
#define LONGCPY_INCLUDED

/*

To be one day eliminated when usage is totally replaced by the memcpy
calls it should be calling now instead. Since this is a stop-gap measure,
it is ok that this include pulls in gtm_string.h if necessary.

void longcpy(uchar_ptr_t a, uchar_ptr_t b, int4 len);

 */

#include "gtm_string.h"
#define longcpy(dst, src, len) memcpy(dst, src, len)

#endif /* LONGCPY_INCLUDED */
