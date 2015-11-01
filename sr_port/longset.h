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

#ifndef LONGSET_INCLUDED
#define LONGSET_INCLUDED

/*

To be one day eliminated when usage is totally replaced by the memset
calls it should be calling now instead. Since this is a stop-gap measure,
it is ok that this include pulls in gtm_string.h if necessary.

void longset(uchar_ptr_t ptr, int len, unsigned char fill);

*/

#ifndef GTM_STRINGH
#  include "gtm_string.h"
#endif
#define longset(dst, len, fill) memset(dst, fill, len)

#endif /* LONGSET_INCLUDED */
