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
#include "util.h"
/* #include "cli.h" */
#include "gtmio.h"
#include "gtm_threadgbl.h"
#include "mmrhash.h"
#include "mupip_hash.h"

/* Display the 128-bit MurmurHash3 value(s) for the file(s) given on the command line.
 * For example,
 *	% mupip hash foo.m bar.m
 *	foo.m: 77c5e66fcaedebf32199d87b0f6b6d80
 *	bar.m: 4b2a2ddc6803a30f4a769d4f6d9c8bd5
 */

void mupip_hash(void)
{
	int			i, fd, status, size;
	hash128_state_t		hash_state;
	gtm_uint16		hash;
	unsigned char		hash_hex[32], readbuf[4096];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (i = 0; i < TREF(parms_cnt); i++)
	{
		OPENFILE(TAREF1(parm_ary, i), O_RDONLY, fd);
		if (fd < 0)
		{
			util_out_print("Error opening !AZ for read", TRUE, TAREF1(parm_ary, i));
			continue;
		}
		size = 0;
		HASH128_STATE_INIT(hash_state, 0);
		while (1)
		{
			DOREADRL(fd, readbuf, SIZEOF(readbuf), status);
			if (-1 == status)
			{
				util_out_print("Error reading from !AZ", TRUE, TAREF1(parm_ary, i));
				goto skipfile;
			}
			if (0 == status)
				break;
			gtmmrhash_128_ingest(&hash_state, readbuf, status);
			size += status;
		}
		gtmmrhash_128_result(&hash_state, size, &hash);
		gtmmrhash_128_hex(&hash, hash_hex);
		util_out_print("!AZ: !AD", TRUE, TAREF1(parm_ary, i), SIZEOF(hash_hex), hash_hex);
	skipfile:
		CLOSEFILE_RESET(fd, status);
	}
}
