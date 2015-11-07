/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
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

GBLREF	boolean_t	in_repl_inst_create;	/* used by repl_inst_read/repl_inst_write */
GBLREF	uint4		process_id;

error_def(ERR_FILEEXISTS);
error_def(ERR_FILERENAME);
error_def(ERR_LOGTOOLONG);
error_def(ERR_RENAMEFAIL);
error_def(ERR_REPLINSTACC);
error_def(ERR_REPLINSTNMLEN);
error_def(ERR_REPLINSTNMUNDEF);
error_def(ERR_REPLINSTSTNDALN);
error_def(ERR_TEXT);

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
	int			idx, status;
	struct stat		stat_buf;
	repl_inst_hdr_ptr_t	repl_instance;
	gtmsrc_lcl_ptr_t	gtmsrc_lcl_array;
	mstr			log_nam, trans_name;
	uint4			status2;
	jnl_tm_t		now;

	if (!repl_inst_get_name(inst_fn, &inst_fn_len, MAX_FN_LEN + 1, issue_rts_error))
		GTMASSERT;	/* rts_error should have been issued by repl_inst_get_name */
	/* Although the maximum length of an instance name is MAX_INSTNAME_LEN-1 characters, the input buffer needs to hold a lot
	 * more since the input instance name might be longer. Hence inst_name (containing MAX_FN_LEN+1 = 257 bytes) is used.
	 */
	inst_name_len = 0;
	if (cli_present("NAME"))
	{
		inst_name_len = SIZEOF(inst_name);
		if (!cli_get_str("NAME", &inst_name[0], &inst_name_len))
			rts_error(VARLSTCNT(4) ERR_TEXT, 2, RTS_ERROR_TEXT("Error parsing NAME qualifier"));
	} else
	{
		log_nam.addr = GTM_REPL_INSTNAME;
		log_nam.len = SIZEOF(GTM_REPL_INSTNAME) - 1;
		trans_name.addr = &inst_name[0];
		if (SS_NORMAL != (status = TRANS_LOG_NAME(&log_nam, &trans_name, inst_name, SIZEOF(inst_name),
								dont_sendmsg_on_log2long)))
		{
			if (SS_LOG2LONG == status)
				rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, log_nam.len, log_nam.addr, SIZEOF(inst_name) - 1);
			else
				rts_error(VARLSTCNT(1) ERR_REPLINSTNMUNDEF);
		}
		inst_name_len = trans_name.len;
	}
	if ((MAX_INSTNAME_LEN <= inst_name_len) || (0 == inst_name_len))
		rts_error(VARLSTCNT(4) ERR_REPLINSTNMLEN, 2, inst_name_len, inst_name);
	inst_name[inst_name_len] = '\0';
	buff_8byte_aligned = &buff_unaligned[0];
	buff_8byte_aligned = (char *)ROUND_UP2((INTPTR_T)buff_8byte_aligned, 8);
	repl_instance = (repl_inst_hdr_ptr_t)&buff_8byte_aligned[0];
	gtmsrc_lcl_array = (gtmsrc_lcl_ptr_t)&buff_8byte_aligned[REPL_INST_HDR_SIZE];
	memset(machine_name, 0, SIZEOF(machine_name));
	if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
		rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to get the hostname"), errno);
	STAT_FILE(inst_fn, &stat_buf, status);
	if (-1 != status)
	{
		if (cli_present("NOREPLACE"))	/* the file exists, so error out */
			rts_error(VARLSTCNT(4) ERR_FILEEXISTS, 2, inst_fn_len, inst_fn);
		in_repl_inst_create = TRUE;	/* used by an assert in the call to "repl_inst_read" below */
		repl_inst_read(inst_fn, (off_t)0, (sm_uc_ptr_t)repl_instance, SIZEOF(repl_inst_hdr));
		in_repl_inst_create = FALSE;
		if ((INVALID_SEMID != repl_instance->jnlpool_semid) || (INVALID_SHMID != repl_instance->jnlpool_shmid)
			|| (INVALID_SEMID != repl_instance->recvpool_semid) || (INVALID_SHMID != repl_instance->recvpool_shmid))
		{
			rts_error(VARLSTCNT(4) ERR_REPLINSTSTNDALN, 2, inst_fn_len, inst_fn);
			assert(FALSE);
		}
		JNL_SHORT_TIME(now);
		if (SS_NORMAL != (status = prepare_unique_name((char *)inst_fn, inst_fn_len, "", "",
				rename_fn, &rename_fn_len, now, &status2)))
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
		} else	/* successfully renamed the existing file; print a message */
			gtm_putmsg(VARLSTCNT(6) ERR_FILERENAME, 4, inst_fn_len, inst_fn, rename_fn_len, rename_fn);

	} else if (ENOENT != errno) /* some error happened */
		rts_error(VARLSTCNT(5) ERR_REPLINSTACC, 2, inst_fn_len, inst_fn, errno);
	/* The instance file consists of 3 parts.
	 *	File header ("repl_inst_hdr" structure)
	 *	Array of 16 "gtmsrc_lcl" structures
	 *	Variable length array of "repl_histinfo" structures
	 * Of these the last part is not allocated at file creation time. The rest have to be initialized now.
	 */
	/************************** Initialize "repl_inst_hdr" section ***************************/
	memset(repl_instance, 0, SIZEOF(repl_inst_hdr));
	memcpy(&repl_instance->label[0], GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ-1);
	repl_instance->replinst_minorver = GDS_REPL_INST_MINOR_LABEL;
	repl_instance->is_little_endian = GTM_IS_LITTLE_ENDIAN;
	repl_instance->is_64bit = GTM_IS_64BIT;
	repl_instance->jnlpool_semid = INVALID_SEMID;
	repl_instance->jnlpool_shmid = INVALID_SHMID;
	repl_instance->recvpool_semid = INVALID_SEMID;
	repl_instance->recvpool_shmid = INVALID_SHMID;
	/********* initialize "inst_info" structure member of "repl_inst_hdr" ***********/
	/* machine_name was obtained from GETHOSTNAME above. It is an array of MAX_MCNAMELEN (256) bytes. The actual
	 * machine name might be longer than can fit in the "created_nodename" field which is MAX_NODENAME_LEN (16) in size.
	 * Take care to copy only as much as needed leaving one character for the null-termination.
	 */
	assert(MAX_NODENAME_LEN <= MAX_MCNAMELEN); /* '=' is valid since we have space to store MAX_NODENAME_LEN characters */
	memcpy(repl_instance->inst_info.created_nodename, machine_name, MAX_NODENAME_LEN);
	/* if machine_name is less than MAX_NODENAME_LEN then set the last valid character of created_nodename array to '\0' which
	 * is relied by repl_inst_dump_filehdr
	 */
	if (MAX_NODENAME_LEN > STRLEN(machine_name))
		repl_instance->inst_info.created_nodename[MAX_NODENAME_LEN - 1] = '\0';
	DBG_CHECK_CREATED_NODENAME(repl_instance->inst_info.created_nodename);
	memcpy(repl_instance->inst_info.this_instname, inst_name, inst_name_len);
	JNL_SHORT_TIME(repl_instance->inst_info.created_time);
	assert(process_id == getpid());
	repl_instance->inst_info.creator_pid = process_id;
	/* repl_instance->lms_group_info should be initialized to NULL at this point.
	 * That is the case already because of the memset above. So nothing more needed for now.
	 */
	repl_instance->jnl_seqno = 0;
	repl_instance->root_primary_cycle = 0;
	repl_instance->num_histinfo = 0;
	repl_instance->num_alloc_histinfo = 0;
	repl_instance->crash = FALSE;
	repl_instance->was_rootprimary = FALSE;
	repl_instance->is_supplementary = cli_present("SUPPLEMENTARY");
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		repl_instance->last_histinfo_num[idx] = INVALID_HISTINFO_NUM;
	/* strm_seqno[] and strm_group_info[] are already initialized to 0 as part of the memset above. Nothing more needed
	 * except the 0th stream seqno. This needs to be set to 1 so the first local update done on a supplementary instance
	 * correctly uses the stream seqno of 1. For non-zero stream #s, a UPDATERESYNC= startup of the receiver to be done
	 * anyways from a supplementary root primary instance and so that will initialize the strm_seqno[] to a non-zero
	 * value before any updates from that stream occur.
	 */
	if (repl_instance->is_supplementary)
		repl_instance->strm_seqno[0] = 1;	/* Initialize 0th stream starting sequence number */
	/************************** Initialize "gtmsrc_lcl" section ***************************/
	memset(gtmsrc_lcl_array, 0, GTMSRC_LCL_SIZE);
	/************************** Write stuff to file on disk ***********************************/
	in_repl_inst_create = TRUE;	/* used by repl_inst_write to determine if O_CREAT needs to be specified in the open */
	repl_inst_write(inst_fn, (off_t)0, (sm_uc_ptr_t)repl_instance, REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
	in_repl_inst_create = FALSE;
}
