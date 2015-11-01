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

#include "mdef.h"

#include <sys/sem.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "gtm_stat.h"
#include "eintr_wrappers.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "gtmio.h"
#include "gtm_string.h"
#include "gtm_logicals.h"
#include "gtm_unistd.h"
#include "trans_log_name.h"
#include "gtmmsg.h"
#include "repl_sem.h"
#include "repl_instance.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;

/*
 * Description:
 *	Get the environment of replication instance.
 * Parameters:
 *	fn : repl instance file name it gets
 *	fn_len: length of fn.
 *	bufsize: the buffer size caller gives. If exceeded, it trucates file name.
 * Returns Value:
 *	TRUE, on success
 *	FALSE, otherwise.
 */
boolean_t repl_inst_get_name(char *fn, unsigned int *fn_len, unsigned int bufsize)
{
	char		temp_inst_fn[MAX_FN_LEN+1];
	mstr		log_nam, trans_name;

	error_def(ERR_TEXT);
	error_def(ERR_ZREPLINST);

	log_nam.addr = ZREPLINSTANCE;
	log_nam.len = sizeof(ZREPLINSTANCE) - 1;
	trans_name.addr = temp_inst_fn;
	if (SS_NORMAL != trans_log_name(&log_nam, &trans_name, temp_inst_fn))
		return FALSE;
	temp_inst_fn[trans_name.len] = '\0';
	if (!get_full_path(trans_name.addr, trans_name.len, fn, fn_len, bufsize))
	{
		gtm_putmsg(VARLSTCNT(8) ERR_ZREPLINST, 2, trans_name.len, trans_name.addr,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("full path could not be found"));
		return FALSE;
	}
	return TRUE;
}

/*
 * Description:
 *	Creates replication instance file. Format is same for primary and secondary.
 * Parameters:
 *	None
 *	None
 * Returns Value:
 */
void repl_inst_create(void)
{
	char 		*ptr, inst_fn[MAX_FN_LEN+1];
        char           	machine_name[MAX_MCNAMELEN];
	int		status = 0, fd;
        struct stat	stat_buf;
	repl_inst_fmt	repl_instance, temp_instance;
	unsigned int	inst_fn_len;

	error_def(ERR_REPLINSTUNDEF);
	error_def(ERR_TEXT);
	error_def(ERR_ZREPLINST);
	error_def(ERR_REPLREQRUNDOWN);

	if (!repl_inst_get_name(inst_fn, &inst_fn_len, MAX_FN_LEN+1))
		rts_error(VARLSTCNT(1) ERR_REPLINSTUNDEF);
	memset(&temp_instance, 0, sizeof(repl_inst_fmt));
	memcpy(&temp_instance.label[0], GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ);
	temp_instance.jnlpool_semid = INVALID_SEMID;
	temp_instance.jnlpool_shmid = INVALID_SHMID;
	temp_instance.recvpool_semid = INVALID_SEMID;
	temp_instance.recvpool_shmid = INVALID_SHMID;
	STAT_FILE(inst_fn, &stat_buf, status);
	if (-1 != status)
	{
		repl_inst_get(inst_fn, &repl_instance);
		if (memcmp(&temp_instance, &repl_instance, sizeof(repl_inst_fmt)))
		{
			memset(machine_name, 0, sizeof(machine_name));
			if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
				rts_error(VARLSTCNT(5) ERR_TEXT, 2,
					RTS_ERROR_TEXT("Unable to get the hostname"), errno);
			rts_error(VARLSTCNT(6) ERR_REPLREQRUNDOWN, 4, inst_fn_len, inst_fn,
				LEN_AND_STR(machine_name));
		}
	} else if (ENOENT != errno) /* some error happened */
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, inst_fn_len, inst_fn, errno);
	memset(&repl_instance, 0, sizeof(repl_inst_fmt));
	memcpy(&repl_instance.label[0], GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ);
	repl_instance.jnlpool_semid = INVALID_SEMID;
	repl_instance.jnlpool_shmid = INVALID_SHMID;
	repl_instance.recvpool_semid = INVALID_SEMID;
	repl_instance.recvpool_shmid = INVALID_SHMID;
	ptr = (char *)&repl_instance;
	OPENFILE3(inst_fn, O_CREAT | O_RDWR, 0666, fd);
	if (-1 == fd)
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, inst_fn_len, inst_fn, errno);
	LSEEKWRITE(fd, (off_t)(0), ptr, sizeof(repl_inst_fmt), status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, inst_fn_len, inst_fn, status);
	CLOSEFILE(fd, status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, inst_fn_len, inst_fn, status);
}


/*
 * Description:
 *	Reads the replication file contents in the structure *repl_instance.
 * Parameters:
 *	inst_fn: Instance file name.
 * Returns Value:
 *	None.
 */
void repl_inst_get(char *inst_fn, repl_inst_fmt *repl_instance)
{
	char 		*ptr;
	int		status, fd;

	error_def(ERR_TEXT);
	error_def(ERR_ZREPLINST);
	error_def(ERR_REPLINSTINCORRV);

	ptr = (char *)repl_instance;
	if (-1 == (fd = (OPEN(inst_fn, O_RDONLY))))
	{
		status = errno;
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, LEN_AND_STR(inst_fn), status);
	}
	LSEEKREAD(fd, (off_t)(0), ptr, sizeof(repl_inst_fmt), status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, LEN_AND_STR(inst_fn), status);
	if (memcmp(&repl_instance->label[0], GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ))
		rts_error(VARLSTCNT(1) ERR_REPLINSTINCORRV);
	CLOSEFILE(fd, status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, LEN_AND_STR(inst_fn), status);
}

/*
 * Description:
 *	Writes the structure *repl_instance into replication instance file.
 * Parameters:
 *	inst_fn: Instance file name.
 * Returns Value:
 *	None.
 */
void repl_inst_put(char *inst_fn, repl_inst_fmt *repl_instance)
{
	char 		*ptr;
	int		status, fd;

	error_def(ERR_TEXT);
	error_def(ERR_ZREPLINST);

	ptr = (char *)repl_instance;
	if (-1 == (fd = (OPEN(inst_fn, O_RDWR))))
	{
		status = errno;
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, LEN_AND_STR(inst_fn), status);
	}
	LSEEKWRITE(fd, (off_t)(0), ptr, sizeof(repl_inst_fmt), status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, LEN_AND_STR(inst_fn), status);
	CLOSEFILE(fd, status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_ZREPLINST, 2, LEN_AND_STR(inst_fn), status);
}

/*
 * Description:
 *	Reset journal pool id in replication instance file.
 * Parameters:
 *	None
 * Returns Value:
 *	None
 */
void repl_inst_jnlpool_reset(void)
{
	repl_inst_fmt	repl_instance;
	unix_db_info	*udi;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	repl_inst_get((char *)udi->fn, &repl_instance);
	repl_instance.jnlpool_semid = INVALID_SEMID;
	repl_instance.jnlpool_shmid = INVALID_SHMID;
	repl_instance.jnlpool_semid_ctime = 0;
	repl_instance.jnlpool_shmid_ctime = 0;
	repl_inst_put((char *)udi->fn, &repl_instance);
}

/*
 * Description:
 *	Reset receiver pool id in replication instance file.
 * Parameters:
 *	None
 * Returns Value:
 *	None
 */
void repl_inst_recvpool_reset(void)
{
	repl_inst_fmt	repl_instance;
	unix_db_info	*udi;

	udi = FILE_INFO(recvpool.recvpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	repl_inst_get((char *)udi->fn, &repl_instance);
	repl_instance.recvpool_semid = INVALID_SEMID;
	repl_instance.recvpool_shmid = INVALID_SHMID;
	repl_instance.recvpool_semid_ctime = 0;
	repl_instance.recvpool_shmid_ctime = 0;
	repl_inst_put((char *)udi->fn, &repl_instance);
}
