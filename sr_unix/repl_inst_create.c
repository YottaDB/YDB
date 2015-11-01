/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_time.h"
#include "gtm_inet.h"

#include <sys/sem.h>
#include <sys/mman.h>
#include <errno.h>

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
#include "cli.h"
#include "gtmrecv.h"
#include "iosp.h"
#include "gtmio.h"
#include "gtm_logicals.h"
#include "trans_log_name.h"
#include "gtmmsg.h"
#include "repl_sem.h"
#include "repl_instance.h"
#include "gtm_rename.h"

GBLREF	boolean_t		in_repl_inst_create;	/* used by repl_inst_read/repl_inst_write */

/* Description:
 *	Creates replication instance file.
 * Parameters:
 *	None
 * Return Value:
 *	None
 */
void repl_inst_create(void)
{
	unsigned int		inst_fn_len;
	unsigned short		inst_name_len;
	int 			rename_fn_len;
	char			rename_fn[MAX_FN_LEN];
	char			inst_fn[MAX_FN_LEN + 1], inst_name[MAX_FN_LEN + 1];
	char			machine_name[MAX_MCNAMELEN], buff_unaligned[REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE + 8];
	char			*buff_8byte_aligned;
	int			status;
	struct stat		stat_buf;
	repl_inst_hdr_ptr_t	repl_instance;
	gtmsrc_lcl_ptr_t	gtmsrc_lcl_array;
	mstr			log_nam, trans_name;
	uint4			status2;

	error_def(ERR_RENAMEFAIL);
	error_def(ERR_REPLINSTACC);
	error_def(ERR_REPLINSTNMLEN);
	error_def(ERR_REPLINSTNMUNDEF);
	error_def(ERR_REPLINSTSTNDALN);
	error_def(ERR_REPLINSTUNDEF);
	error_def(ERR_TEXT);

	if (!repl_inst_get_name(inst_fn, &inst_fn_len, MAX_FN_LEN+1))
		rts_error(VARLSTCNT(1) ERR_REPLINSTUNDEF);
	buff_8byte_aligned = &buff_unaligned[0];
	buff_8byte_aligned = (char *)ROUND_UP2((int4)buff_8byte_aligned, 8);
	repl_instance = (repl_inst_hdr_ptr_t)&buff_8byte_aligned[0];
	gtmsrc_lcl_array = (gtmsrc_lcl_ptr_t)&buff_8byte_aligned[REPL_INST_HDR_SIZE];
	memset(machine_name, 0, sizeof(machine_name));
	if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
		rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to get the hostname"), errno);
	STAT_FILE(inst_fn, &stat_buf, status);
	if (-1 != status)
	{
		in_repl_inst_create = TRUE;	/* used by an assert in the call to "repl_inst_read" below */
		repl_inst_read(inst_fn, (off_t)0, (sm_uc_ptr_t)repl_instance, sizeof(repl_inst_hdr));
		in_repl_inst_create = FALSE;
		if ((INVALID_SEMID != repl_instance->jnlpool_semid) || (INVALID_SHMID != repl_instance->jnlpool_shmid)
			|| (INVALID_SEMID != repl_instance->recvpool_semid) || (INVALID_SHMID != repl_instance->recvpool_shmid))
		{
			rts_error(VARLSTCNT(4) ERR_REPLINSTSTNDALN, 2, inst_fn_len, inst_fn);
			assert(FALSE);
		}
		if (SS_NORMAL != (status = prepare_unique_name((char *)inst_fn, inst_fn_len, "", "",
				rename_fn, &rename_fn_len, &status2)))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Error preparing unique name for renaming instance file"));
			if (SS_NORMAL != status2)
				rts_error(VARLSTCNT(7) ERR_REPLINSTACC, 2, inst_fn_len, inst_fn, status, 0, status2);
			else
				rts_error(VARLSTCNT(5) ERR_REPLINSTACC, 2, inst_fn_len, inst_fn, status);
		}
		if (SS_NORMAL != (status = gtm_rename((char *)inst_fn, (int)inst_fn_len,
								(char *)rename_fn, rename_fn_len, &status2)))
		{
			if (SS_NORMAL != status2)
				rts_error(VARLSTCNT(9) ERR_RENAMEFAIL, 4, inst_fn_len, inst_fn,
						rename_fn_len, rename_fn, status, 0, status2);
			else
				rts_error(VARLSTCNT(7) ERR_RENAMEFAIL, 4, inst_fn_len, inst_fn, rename_fn_len, rename_fn, status);
		}
	} else if (ENOENT != errno) /* some error happened */
		rts_error(VARLSTCNT(5) ERR_REPLINSTACC, 2, inst_fn_len, inst_fn, errno);
	/* The instance file consists of 3 parts.
	 *	File header ("repl_inst_hdr" structure)
	 *	Array of 16 "gtmsrc_lcl" structures
	 *	Variable length array of "repl_triple" structures
	 * Of these the last part is not allocated at file creation time. The rest have to be initialized now.
	 */
	/************************** Initialize "repl_inst_hdr" section ***************************/
	memset(repl_instance, 0, sizeof(repl_inst_hdr));
	memcpy(&repl_instance->label[0], GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ-1);
	repl_instance->jnlpool_semid = INVALID_SEMID;
	repl_instance->jnlpool_shmid = INVALID_SHMID;
	repl_instance->recvpool_semid = INVALID_SEMID;
	repl_instance->recvpool_shmid = INVALID_SHMID;
	time(&repl_instance->created_time);
	/* machine_name was obtained from GETHOSTNAME above. It is an array of MAX_MCNAME_LEN (256) bytes. The actual
	 * machine name might be longer than can fit in the "created_nodename" field which is MAX_NODENAME_LEN (16) in size.
	 * Take care to copy only as much as needed leaving one character for the null-termination.
	 */
	memcpy(repl_instance->created_nodename, machine_name, MAX_NODENAME_LEN - 1);
	repl_instance->created_nodename[MAX_NODENAME_LEN - 1] = '\0';
	/* Although the maximum length of an instance name is MAX_INSTNAME_LEN-1 characters, the input buffer needs to hold a lot
	 * more since the input instance name might be longer. Hence inst_name (containing MAX_FN_LEN+1 = 257 bytes) is used.
	 */
	if (cli_present("NAME"))
	{
		inst_name_len = sizeof(inst_name);;
		if (!cli_get_str("NAME", &inst_name[0], &inst_name_len))
			rts_error(VARLSTCNT(4) ERR_TEXT, 2, RTS_ERROR_TEXT("Error parsing NAME qualifier"));
	} else
	{
		log_nam.addr = GTM_REPL_INSTNAME;
		log_nam.len = sizeof(GTM_REPL_INSTNAME) - 1;
		trans_name.addr = &inst_name[0];
		if (SS_NORMAL != trans_log_name(&log_nam, &trans_name, inst_name))
			rts_error(VARLSTCNT(1) ERR_REPLINSTNMUNDEF);
		inst_name_len = trans_name.len;
	}
	inst_name[inst_name_len] = '\0';
	if ((MAX_INSTNAME_LEN <= inst_name_len) || (0 == inst_name_len))
		rts_error(VARLSTCNT(4) ERR_REPLINSTNMLEN, 2, inst_name_len, inst_name);
	memcpy(repl_instance->this_instname, inst_name, inst_name_len);
	repl_instance->jnl_seqno = 0;
	repl_instance->root_primary_cycle = 0;
	repl_instance->num_alloc_triples = 0;
	repl_instance->num_triples = 0;
	repl_instance->crash = FALSE;
	/************************** Initialize "gtmsrc_lcl" section ***************************/
	memset(gtmsrc_lcl_array, 0, GTMSRC_LCL_SIZE);
	/************************** Write stuff to file on disk ***********************************/
	in_repl_inst_create = TRUE;	/* used by repl_inst_write to determine if O_CREAT needs to be specified in the open */
	repl_inst_write(inst_fn, (off_t)0, (sm_uc_ptr_t)repl_instance, REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
	in_repl_inst_create = FALSE;
}
