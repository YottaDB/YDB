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

#include "mdef.h"

#include "copy.h"
#include "eintr_wrappers.h"
#include "gtm_string.h"
#include "io.h"
#include "gtmio.h"

#include "md5hash.h"
#include "md5_digest2hex.h"

/*
 * Construct printable 32-character MD5 checksum string from raw 128-bit digest array.
 */
void md5_digest2hex(char hexstr[MD5_HEXSTR_LENGTH], const unsigned char digest[MD5_DIGEST_LENGTH])
{
	int 	i, tlen;
	char	*bptr;

	bptr = (char *)&hexstr[0];
	/* TODO - improve efficiency to avoid syscall using i2hexl() */
	for (i = 0; i < MD5_DIGEST_LENGTH; i++)
	{
		tlen = SPRINTF(bptr, "%02x", digest[i]);
		assert(2 == tlen);
		bptr += tlen;
	}
	assert('\0' == hexstr[MD5_HEXSTR_LENGTH - 1]);
}
