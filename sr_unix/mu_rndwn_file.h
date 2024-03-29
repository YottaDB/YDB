/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_RNDWN_DEFINED
#define MU_RNDWN_DEFINED

/* for FTOK_SEM_PER_ID */
#include "gtm_sem.h"

#define SHMID           2

/* We noticed the "ipcs" utility return duplicate lines (the same line got returned anywhere from 1 to even hundreds of times)
 * in the following version. Not sure when this will get fixed so as a work around we do the "sort -u" below.
 *	$ ipcs --version
 *	ipcs from util-linux 2.35.1
 */
#ifdef __linux__
#define KEY             1
#define IPCS_CMD_STR		"ipcs -m | sort -u | grep '^0x'"
#define IPCS_SEM_CMD_STR	"ipcs -s | sort -u | grep '^0x'"
#else
#define KEY             3
/* though the extra blank space is required in AIX under certain cases, we
 * are adding it for all UNIX versions to avoid another ifdef for AIX.
 */
#define IPCS_CMD_STR		"ipcs -m | sort -u | grep '^m' | sed 's/^m/m /g'"
#define IPCS_SEM_CMD_STR	"ipcs -s | sort -u | grep '^s' | sed 's/^s/s /g'"
#endif /* __linux__ */

#define SGMNTSIZ        10
#define MAX_PARM_LEN    128
#define MAX_ENTRY_LEN   1024


typedef struct shm_parms_struct
{
	ssize_t		sgmnt_siz;
        int             shmid;
        key_t           key;
} shm_parms;

#define RNDWN_ERR(str, reg)						\
{									\
	save_errno = errno;						\
	if (reg)							\
		util_out_print(str, TRUE, DB_LEN_STR(reg));		\
	else								\
		util_out_print(str, TRUE);				\
	util_out_print(STRERROR(save_errno), TRUE);			\
}

#define CONVERT_TO_NUM(ENTRY)						\
{									\
	if (!parm)							\
	{								\
		free(parm_buff);					\
		return NULL;						\
	}								\
	if (cli_is_dcm(parm))						\
		parm_buff->ENTRY = (int)(STRTOUL(parm, NULL, 10));	\
	else if (cli_is_hex(parm + 2))					\
		parm_buff->ENTRY = (int)(STRTOUL(parm, NULL, 16));	\
	else								\
	{								\
		assert(FALSE);						\
		free(parm_buff);					\
		free(parm);						\
		return NULL;						\
	}								\
	free(parm);							\
}

boolean_t		mu_rndwn_file(gd_region *reg, boolean_t standalone);
#endif
