/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

#ifdef __linux__
#define KEY             1
#define IPCS_CMD_STR		"ipcs -m | grep '^0x'"
#define IPCS_SEM_CMD_STR	"ipcs -s | grep '^0x'"
#else
#define KEY             3
/* though the extra blank space is required in AIX under certain cases, we
 * are adding it for all UNIX versions to avoid another ifdef for AIX.
 */
#define IPCS_CMD_STR		"ipcs -m | grep '^m' | sed 's/^m/m /g'"
#define IPCS_SEM_CMD_STR	"ipcs -s | grep '^s' | sed 's/^s/s /g'"
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

boolean_t mu_rndwn_file(gd_region *reg, boolean_t standalone);
#endif
