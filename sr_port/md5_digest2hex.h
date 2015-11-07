/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MD5_DIGEST2HEX_INCLUDED
#define MD5_DIGEST2HEX_INCLUDED

#include "md5hash.h"

#define MD5_HEXSTR_LENGTH	33

void md5_digest2hex(char hexstr[MD5_HEXSTR_LENGTH], const unsigned char digest[MD5_DIGEST_LENGTH]);

#endif /* MD5_DIGEST2HEX_INCLUDED */
