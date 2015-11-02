/****************************************************************
 *                                                              *
 *      Copyright 2011 Fidelity Information Services, Inc       *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#define STRUCT_STAT_HAS_ST_GEN		defined(__osf__) || defined(_AIX)

#include "mdef.h"
#include "gtm_ipc.h"
#include "mmrhash.h"

#include <sys/types.h>
#include <gtm_stat.h>
#include <eintr_wrappers.h>

key_t
gtm_ftok(const char *path, int id)
{
    int rc;
    struct stat statbuf;
    uint32_t key = 0;

    STAT_FILE(path, &statbuf, rc);
    if (rc < 0)
    {
        return (key_t)-1;
    }

    MurmurHash3_x86_32(&statbuf.st_dev, sizeof statbuf.st_dev, key, &key);
    MurmurHash3_x86_32(&statbuf.st_ino, sizeof statbuf.st_ino, key, &key);
#if STRUCT_STAT_HAS_ST_GEN
    MurmurHash3_x86_32(&statbuf.st_gen, sizeof statbuf.st_gen, key, &key);
#endif

    /* substitute the id for the top 8 bits of the hash */
    key &= 0x00ffffff;
    key |= (id & 0xff) << 24;

    return (key_t)key;
}
