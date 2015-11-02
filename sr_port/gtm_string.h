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

/* If this is not the vax, define string.h. This is because the Vax
   has its own built-in instructions for string manipulation.
*/
#ifndef GTM_STRINGH
#define GTM_STRINGH

#ifndef __vax
#  include <string.h>
#endif

#define STRERROR	strerror
#define STRCASECMP	strcasecmp
#define STRNCASECMP	strncasecmp

#endif
