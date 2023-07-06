/****************************************************************
<<<<<<< HEAD
 *								*
 * Copyright (c) 2011-2020 Fidelity National Information	*
=======
 *                                                              *
 * Copyright (c) 2011-2023 Fidelity National Information	*
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_ipc.h"
#include "mmrhash.h"

#include <sys/types.h>
#include <gtm_stat.h>
#include <eintr_wrappers.h>

unsigned int gtm_stat_hash(struct stat *statbuf)
{
	hash128_state_t		state;
	gtm_uint16		out16;
	uint4			key = 0;

	/* This needs to be the classic MurmurHash3 for compatibility with prior versions */
	MurmurHash3_x86_32(&statbuf->st_dev, sizeof statbuf->st_dev, key, &key);
	MurmurHash3_x86_32(&statbuf->st_ino, sizeof statbuf->st_ino, key, &key);
	return key;
}

key_t gtm_ftok(const char *path, int id)
{
	int		rc;
	struct stat	statbuf;
	uint4		key;

	STAT_FILE(path, &statbuf, rc);
	if (rc < 0)
	{
		return (key_t)-1;
	}
	key = gtm_stat_hash(&statbuf);
	/* substitute the id for the top 8 bits of the hash */
	key &= 0x00ffffff;
	key |= (id & 0xff) << 24;

	return (key_t)key;
}
