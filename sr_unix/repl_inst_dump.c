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

#include "gtm_string.h"

#include <stddef.h>	/* for "offsetof" macro */

#include "cli.h"
#include "util.h"
#include "repl_instance.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"		/* for JNL_WHOLE_FROM_SHORT_TIME */
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "repl_inst_dump.h"

LITDEF	char	state_array[][23] = {
			"DUMMY_STATE",
			"START",
			"WAITING_FOR_CONNECTION",
			"WAITING_FOR_RESTART",
			"SEARCHING_FOR_RESTART",
			"SENDING_JNLRECS",
			"WAITING_FOR_XON",
			"CHANGING_MODE"
		};

#define	PREFIX_FILEHDR			"HDR "
#define	PREFIX_SRCLCL			"SLT #!2UL : "
#define	PREFIX_TRIPLEHIST		"HST #!7UL : "
#define	PREFIX_JNLPOOLCTL		"CTL "
#define	PREFIX_SOURCELOCAL		"SRC #!2UL : "

#define	FILEHDR_TITLE_STRING		"File Header"
#define	SRCLCL_TITLE_STRING		"Source Server Slots"
#define	TRIPLEHIST_TITLE_STRING		"History Records (triples)"
#define	JNLPOOLCTL_TITLE_STRING		"Journal Pool Control Structure"
#define	SOURCELOCAL_TITLE_STRING	"Source Server Structures"

#define	PRINT_BOOLEAN(printstr, value, index)							\
{													\
	char	*string;										\
													\
	string = ((TRUE == (value)) ? "TRUE" : ((FALSE == (value)) ? "FALSE" : "UNKNOWN"));		\
	if ((FALSE == value) || (TRUE == value))							\
	{												\
		if (-1 == (index))									\
			util_out_print(printstr, TRUE, string);						\
		else											\
			util_out_print(printstr, TRUE, (index), string);				\
	} else												\
	{												\
		if (-1 == (index))									\
			util_out_print(printstr " [0x!8XL]", TRUE, string, (value));			\
		else											\
			util_out_print(printstr " [0x!8XL]", TRUE, (index), string, (value));		\
	}												\
}

#define	PRINT_TIME(printstr, value)									\
{													\
	jnl_proc_time	whole_time;									\
	int		time_len;									\
	char		time_str[LENGTH_OF_TIME + 1];							\
													\
	if (0 != value)											\
	{												\
		JNL_WHOLE_FROM_SHORT_TIME(whole_time, value);						\
		time_len = format_time(whole_time, time_str, sizeof(time_str), SHORT_TIME_FORMAT);	\
	} else												\
	{												\
		time_len = 1;										\
		time_str[0] = '0';									\
	}												\
	util_out_print( printstr "!R20AD", TRUE, time_len, time_str);					\
}

#define	PRINT_SEM_SHM_ID(printstr, value)						\
{											\
	assert(INVALID_SEMID == INVALID_SHMID);						\
	if (INVALID_SEMID != value)							\
		util_out_print( printstr "!10UL [0x!8XL]", TRUE, value, value);		\
	else										\
		util_out_print( printstr "   INVALID", TRUE);				\
}

#define	PRINT_OFFSET_HEADER			if (print_offset) { util_out_print("Offset     Size", TRUE); }
#define	PRINT_OFFSET_PREFIX(offset, size)								\
	if (print_offset) { util_out_print("0x!8XL 0x!4XW ", FALSE, offset + section_offset, size); };

GBLREF	uint4		section_offset;		/* Used by PRINT_OFFSET_PREFIX macro in repl_inst_dump.c */

void	repl_inst_dump_filehdr(repl_inst_hdr_ptr_t repl_instance)
{
	char		*string;

	util_out_print("", TRUE);
	PRINT_DASHES;
	util_out_print(FILEHDR_TITLE_STRING, TRUE);
	PRINT_DASHES;
	PRINT_OFFSET_HEADER;

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, label[0]), sizeof(repl_instance->label));;
	util_out_print( PREFIX_FILEHDR "Label                                      !11AD", TRUE,
		GDS_REPL_INST_LABEL_SZ - 1, repl_instance->label);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, jnlpool_semid), sizeof(repl_instance->jnlpool_semid));;
	PRINT_SEM_SHM_ID( PREFIX_FILEHDR "Journal Pool Sem Id                         ", repl_instance->jnlpool_semid);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, jnlpool_shmid), sizeof(repl_instance->jnlpool_shmid));;
	PRINT_SEM_SHM_ID( PREFIX_FILEHDR "Journal Pool Shm Id                         ", repl_instance->jnlpool_shmid);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, recvpool_semid), sizeof(repl_instance->recvpool_semid));;
	PRINT_SEM_SHM_ID( PREFIX_FILEHDR "Receive Pool Sem Id                         ", repl_instance->recvpool_semid);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, recvpool_shmid), sizeof(repl_instance->recvpool_shmid));;
	PRINT_SEM_SHM_ID( PREFIX_FILEHDR "Receive Pool Shm Id                         ", repl_instance->recvpool_shmid);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, jnlpool_semid_ctime), sizeof(repl_instance->jnlpool_semid_ctime));
	PRINT_TIME( PREFIX_FILEHDR "Journal Pool Sem Create Time      ", repl_instance->jnlpool_semid_ctime);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, jnlpool_shmid_ctime), sizeof(repl_instance->jnlpool_shmid_ctime));
	PRINT_TIME( PREFIX_FILEHDR "Journal Pool Shm Create Time      ", repl_instance->jnlpool_shmid_ctime);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, recvpool_semid_ctime), sizeof(repl_instance->recvpool_semid_ctime));
	PRINT_TIME( PREFIX_FILEHDR "Receive Pool Sem Create Time      ", repl_instance->recvpool_semid_ctime);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, recvpool_shmid_ctime), sizeof(repl_instance->recvpool_shmid_ctime));
	PRINT_TIME( PREFIX_FILEHDR "Receive Pool Shm Create Time      ", repl_instance->recvpool_shmid_ctime);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, created_time), sizeof(repl_instance->created_time));
	PRINT_TIME( PREFIX_FILEHDR "Instance File Create Time         ", repl_instance->created_time);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, created_nodename[0]), sizeof(repl_instance->created_nodename));
	util_out_print( PREFIX_FILEHDR "Instance File Created Nodename         !R15AD", TRUE,
		LEN_AND_STR((char *)repl_instance->created_nodename));

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, this_instname[0]), sizeof(repl_instance->this_instname));
	util_out_print( PREFIX_FILEHDR "Instance Name                          !R15AD", TRUE,
		LEN_AND_STR((char *)repl_instance->this_instname));

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, jnl_seqno), sizeof(repl_instance->jnl_seqno));
	util_out_print( PREFIX_FILEHDR "Journal Sequence Number           !20@UJ [0x!16@XJ]", TRUE,
			&repl_instance->jnl_seqno, &repl_instance->jnl_seqno);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, root_primary_cycle), sizeof(repl_instance->root_primary_cycle));
	util_out_print( PREFIX_FILEHDR "Root Primary Cycle                          !10UL [0x!8XL]", TRUE,
		repl_instance->root_primary_cycle, repl_instance->root_primary_cycle);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, num_triples), sizeof(repl_instance->num_triples));
	util_out_print( PREFIX_FILEHDR "Number of used history records              !10UL [0x!8XL]", TRUE,
		repl_instance->num_triples, repl_instance->num_triples);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, num_alloc_triples), sizeof(repl_instance->num_alloc_triples));
	util_out_print( PREFIX_FILEHDR "Allocated history records                   !10UL [0x!8XL]", TRUE,
		repl_instance->num_alloc_triples, repl_instance->num_alloc_triples);

	PRINT_OFFSET_PREFIX(offsetof(repl_inst_hdr, crash), sizeof(repl_instance->crash));
	PRINT_BOOLEAN(PREFIX_FILEHDR "Crash                                       !R10AZ", repl_instance->crash, -1);
}

void	repl_inst_dump_gtmsrclcl(gtmsrc_lcl_ptr_t gtmsrclcl_ptr)
{
	int		idx;
	boolean_t	first_time = TRUE;

	for (idx = 0; idx < NUM_GTMSRC_LCL; idx++, gtmsrclcl_ptr++)
	{
		if (('\0' == gtmsrclcl_ptr->secondary_instname[0])
				&& (0 == gtmsrclcl_ptr->resync_seqno)
				&& (0 == gtmsrclcl_ptr->connect_jnl_seqno))
			continue;
		if (first_time)
		{
			util_out_print("", TRUE);
			first_time = FALSE;
			PRINT_DASHES;
			util_out_print(SRCLCL_TITLE_STRING, TRUE);
			PRINT_DASHES;
		} else
			PRINT_DASHES;
		PRINT_OFFSET_HEADER;

		PRINT_OFFSET_PREFIX(offsetof(gtmsrc_lcl, secondary_instname[0]), sizeof(gtmsrclcl_ptr->secondary_instname));
		util_out_print( PREFIX_SRCLCL "Secondary Instance Name          !R15AD", TRUE, idx,
			LEN_AND_STR((char *)gtmsrclcl_ptr->secondary_instname));

		PRINT_OFFSET_PREFIX(offsetof(gtmsrc_lcl, resync_seqno), sizeof(gtmsrclcl_ptr->resync_seqno));
		util_out_print( PREFIX_SRCLCL "Resync Sequence Number      !20@UJ [0x!16@XJ]", TRUE, idx,
			&gtmsrclcl_ptr->resync_seqno, &gtmsrclcl_ptr->resync_seqno);

		PRINT_OFFSET_PREFIX(offsetof(gtmsrc_lcl, connect_jnl_seqno), sizeof(gtmsrclcl_ptr->connect_jnl_seqno));
		util_out_print( PREFIX_SRCLCL "Connect Sequence Number     !20@UJ [0x!16@XJ]", TRUE, idx,
			&gtmsrclcl_ptr->connect_jnl_seqno, &gtmsrclcl_ptr->connect_jnl_seqno);
		section_offset += sizeof(gtmsrc_lcl);
	}
}

void	repl_inst_dump_triplehist(char *inst_fn, int4 num_triples)
{
	int4		idx;
	off_t		offset;
	repl_triple	curtriple;
	jnl_proc_time	whole_time;
	int		time_len;
	char		time_str[LENGTH_OF_TIME + 1];
	boolean_t	first_time = TRUE;

	for (idx = 0; idx < num_triples; idx++, offset += sizeof(repl_triple))
	{
		if (first_time)
		{
			util_out_print("", TRUE);
			first_time = FALSE;
			PRINT_DASHES;
			util_out_print(TRIPLEHIST_TITLE_STRING, TRUE);
			PRINT_DASHES;
			offset = REPL_INST_TRIPLE_OFFSET;
		} else
			PRINT_DASHES;
		repl_inst_read(inst_fn, (off_t)offset, (sm_uc_ptr_t)&curtriple, sizeof(repl_triple));
		PRINT_OFFSET_HEADER;

		PRINT_OFFSET_PREFIX(offsetof(repl_triple, root_primary_instname[0]), sizeof(curtriple.root_primary_instname));
		util_out_print(PREFIX_TRIPLEHIST "Root Primary Instance Name  !R15AD", TRUE, idx,
			LEN_AND_STR((char *)curtriple.root_primary_instname));

		PRINT_OFFSET_PREFIX(offsetof(repl_triple, start_seqno), sizeof(curtriple.start_seqno));
		util_out_print(PREFIX_TRIPLEHIST "Start Sequence Number  !20@UJ [0x!16@XJ]", TRUE, idx,
			&curtriple.start_seqno, &curtriple.start_seqno);

		PRINT_OFFSET_PREFIX(offsetof(repl_triple, root_primary_cycle), sizeof(curtriple.root_primary_cycle));
		util_out_print(PREFIX_TRIPLEHIST "Root Primary Cycle               !10UL [0x!8XL]", TRUE, idx,
			curtriple.root_primary_cycle, curtriple.root_primary_cycle);

		PRINT_OFFSET_PREFIX(offsetof(repl_triple, created_time), sizeof(curtriple.created_time));
		JNL_WHOLE_FROM_SHORT_TIME(whole_time, curtriple.created_time);
		time_len = format_time(whole_time, time_str, sizeof(time_str), SHORT_TIME_FORMAT);
		util_out_print(PREFIX_TRIPLEHIST "Creation Time          "TIME_DISPLAY_FAO, TRUE, idx,
			time_len, time_str);

		PRINT_OFFSET_PREFIX(offsetof(repl_triple, rcvd_from_instname[0]), sizeof(curtriple.rcvd_from_instname));
		util_out_print(PREFIX_TRIPLEHIST "Received from Instance      !R15AD", TRUE, idx,
			LEN_AND_STR((char *)curtriple.rcvd_from_instname));
		section_offset += sizeof(repl_triple);
	}
}

void	repl_inst_dump_jnlpoolctl(jnlpool_ctl_ptr_t jnlpool_ctl)
{
	char	*string;

	util_out_print("", TRUE);
	PRINT_DASHES;
	util_out_print(JNLPOOLCTL_TITLE_STRING, TRUE);
	PRINT_DASHES;
	PRINT_OFFSET_HEADER;

	assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));	/* The following offsetof calculations depend on this */
	PRINT_OFFSET_PREFIX(offsetof(replpool_identifier, label[0]), sizeof(jnlpool_ctl->jnlpool_id.label));
	util_out_print( PREFIX_JNLPOOLCTL "Label                                      !11AD", TRUE,
		GDS_LABEL_SZ - 1, jnlpool_ctl->jnlpool_id.label);

	PRINT_OFFSET_PREFIX(offsetof(replpool_identifier, pool_type), sizeof(jnlpool_ctl->jnlpool_id.pool_type));
	string = (JNLPOOL_SEGMENT == jnlpool_ctl->jnlpool_id.pool_type) ? "JNLPOOL" :
			((RECVPOOL_SEGMENT == jnlpool_ctl->jnlpool_id.pool_type) ? "RECVPOOL" : "UNKNOWN");
	if (MEMCMP_LIT(string, "UNKNOWN"))
		util_out_print( PREFIX_JNLPOOLCTL "Type                            !R22AZ", TRUE, string);
	else
	{
		util_out_print( PREFIX_JNLPOOLCTL "Type                            !R22AZ [0x!8XL]", TRUE,
			string, jnlpool_ctl->jnlpool_id.pool_type);
	}

	PRINT_OFFSET_PREFIX(offsetof(replpool_identifier, now_running[0]), sizeof(jnlpool_ctl->jnlpool_id.now_running));
	util_out_print( PREFIX_JNLPOOLCTL "GT.M Version                    !R22AZ", TRUE, jnlpool_ctl->jnlpool_id.now_running);

	PRINT_OFFSET_PREFIX(offsetof(replpool_identifier, instfilename[0]), sizeof(jnlpool_ctl->jnlpool_id.instfilename));
	if (22 >= strlen(jnlpool_ctl->jnlpool_id.instfilename))
	{
		util_out_print( PREFIX_JNLPOOLCTL "Instance file name              !R22AZ",
			TRUE, jnlpool_ctl->jnlpool_id.instfilename);
	} else
		util_out_print( PREFIX_JNLPOOLCTL "Instance file name      !AZ", TRUE, jnlpool_ctl->jnlpool_id.instfilename);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, start_jnl_seqno), sizeof(jnlpool_ctl->start_jnl_seqno));
	util_out_print( PREFIX_JNLPOOLCTL "Start Journal Seqno               !20@UJ [0x!16@XJ]", TRUE,
		&jnlpool_ctl->start_jnl_seqno, &jnlpool_ctl->start_jnl_seqno);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, jnl_seqno), sizeof(jnlpool_ctl->jnl_seqno));
	util_out_print( PREFIX_JNLPOOLCTL "Journal Seqno                     !20@UJ [0x!16@XJ]", TRUE,
		&jnlpool_ctl->jnl_seqno, &jnlpool_ctl->jnl_seqno);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, last_triple_seqno), sizeof(jnlpool_ctl->last_triple_seqno));
	util_out_print( PREFIX_JNLPOOLCTL "Last Triple Seqno                 !20@UJ [0x!16@XJ]", TRUE,
		&jnlpool_ctl->last_triple_seqno, &jnlpool_ctl->last_triple_seqno);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, jnldata_base_off), sizeof(jnlpool_ctl->jnldata_base_off));
	util_out_print( PREFIX_JNLPOOLCTL "Journal Data Base Offset                    !10UL [0x!8XL]", TRUE,
		jnlpool_ctl->jnldata_base_off, jnlpool_ctl->jnldata_base_off);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, jnlpool_size), sizeof(jnlpool_ctl->jnlpool_size));
	util_out_print( PREFIX_JNLPOOLCTL "Journal Pool Size (in bytes)                !10UL [0x!8XL]", TRUE,
		jnlpool_ctl->jnlpool_size, jnlpool_ctl->jnlpool_size);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, early_write_addr), sizeof(jnlpool_ctl->early_write_addr));
	util_out_print( PREFIX_JNLPOOLCTL "Early Write Offset                !20@UJ [0x!16@XJ]", TRUE,
		&jnlpool_ctl->early_write_addr, &jnlpool_ctl->early_write_addr);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, write_addr), sizeof(jnlpool_ctl->write_addr));
	util_out_print( PREFIX_JNLPOOLCTL "Absolute Write Offset             !20@UJ [0x!16@XJ]", TRUE,
		&jnlpool_ctl->write_addr, &jnlpool_ctl->write_addr);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, write), sizeof(jnlpool_ctl->write));
	util_out_print( PREFIX_JNLPOOLCTL "Relative Write Offset                       !10UL [0x!8XL]", TRUE,
		jnlpool_ctl->write, jnlpool_ctl->write);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, upd_disabled), sizeof(jnlpool_ctl->upd_disabled));
	PRINT_BOOLEAN(PREFIX_JNLPOOLCTL "Updates Disabled                !R22AZ", jnlpool_ctl->upd_disabled, -1);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, primary_is_dualsite), sizeof(jnlpool_ctl->primary_is_dualsite));
	PRINT_BOOLEAN(PREFIX_JNLPOOLCTL "Primary is Dual Site            !R22AZ", jnlpool_ctl->primary_is_dualsite, -1);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, secondary_is_dualsite), sizeof(jnlpool_ctl->secondary_is_dualsite));
	PRINT_BOOLEAN(PREFIX_JNLPOOLCTL "Secondary is Dual Site          !R22AZ", jnlpool_ctl->secondary_is_dualsite, -1);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, lastwrite_len), sizeof(jnlpool_ctl->lastwrite_len));
	util_out_print( PREFIX_JNLPOOLCTL "Last Write Length (in bytes)                !10UL [0x!8XL]", TRUE,
		jnlpool_ctl->lastwrite_len, jnlpool_ctl->lastwrite_len);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, send_losttn_complete), sizeof(jnlpool_ctl->send_losttn_complete));
	PRINT_BOOLEAN(PREFIX_JNLPOOLCTL "Send LostTN Complete            !R22AZ", jnlpool_ctl->send_losttn_complete, -1);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, primary_instname[0]), sizeof(jnlpool_ctl->primary_instname));
	util_out_print( PREFIX_JNLPOOLCTL "Primary Instance Name             !R20AZ",
		TRUE, jnlpool_ctl->primary_instname);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, max_zqgblmod_seqno), sizeof(jnlpool_ctl->max_zqgblmod_seqno));
	util_out_print( PREFIX_JNLPOOLCTL "Zqgblmod Seqno                    !20@UJ [0x!16@XJ]", TRUE,
		&jnlpool_ctl->max_zqgblmod_seqno, &jnlpool_ctl->max_zqgblmod_seqno);

	PRINT_OFFSET_PREFIX(offsetof(jnlpool_ctl_struct, this_proto_ver), sizeof(jnlpool_ctl->this_proto_ver));
	util_out_print( PREFIX_JNLPOOLCTL "Source Server Protocol Version                     !3UL [0x!2XB]", TRUE,
			jnlpool_ctl->this_proto_ver, jnlpool_ctl->this_proto_ver);
}

void	repl_inst_dump_gtmsourcelocal(gtmsource_local_ptr_t gtmsourcelocal_ptr)
{
	int		idx;
	char		*string;
	boolean_t	first_time = TRUE;

	for (idx = 0; idx < NUM_GTMSRC_LCL; idx++, gtmsourcelocal_ptr++)
	{
		if (('\0' == gtmsourcelocal_ptr->secondary_instname[0])
				&& (0 == gtmsourcelocal_ptr->read_jnl_seqno)
				&& (0 == gtmsourcelocal_ptr->connect_jnl_seqno))
			continue;
		if (first_time)
		{
			util_out_print("", TRUE);
			first_time = FALSE;
			PRINT_DASHES;
			util_out_print(SOURCELOCAL_TITLE_STRING, TRUE);
			PRINT_DASHES;
		} else
			PRINT_DASHES;
		PRINT_OFFSET_HEADER;

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, secondary_instname[0]),
			sizeof(gtmsourcelocal_ptr->secondary_instname));
		util_out_print( PREFIX_SOURCELOCAL "Secondary Instance Name         !R16AZ", TRUE, idx,
			(char *)gtmsourcelocal_ptr->secondary_instname);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, gtmsource_pid), sizeof(gtmsourcelocal_ptr->gtmsource_pid));
		util_out_print( PREFIX_SOURCELOCAL "Source Server Pid                     !10UL", TRUE, idx,
			gtmsourcelocal_ptr->gtmsource_pid);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, mode), sizeof(gtmsourcelocal_ptr->mode));
		string = (GTMSOURCE_MODE_PASSIVE == gtmsourcelocal_ptr->mode) ? "PASSIVE" :
				((GTMSOURCE_MODE_ACTIVE == gtmsourcelocal_ptr->mode) ? "ACTIVE" : "UNKNOWN");
		if (MEMCMP_LIT(string, "UNKNOWN"))
			util_out_print( PREFIX_SOURCELOCAL "Source Server Mode        !R22AZ", TRUE, idx, string);
		else
		{
			util_out_print( PREFIX_SOURCELOCAL "Source Server Mode        !R22AZ [0x!8XL]", TRUE, idx,
				string, gtmsourcelocal_ptr->mode);
		}

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, gtmsource_state), sizeof(gtmsourcelocal_ptr->gtmsource_state));
		string = ((0 <= gtmsourcelocal_ptr->gtmsource_state)
				&& (GTMSOURCE_NUM_STATES > gtmsourcelocal_ptr->gtmsource_state))
					? (char *)state_array[gtmsourcelocal_ptr->gtmsource_state] : "UNKNOWN";
		if (MEMCMP_LIT(string, "UNKNOWN"))
			util_out_print( PREFIX_SOURCELOCAL "Processing State          !R22AZ", TRUE, idx, string);
		else
		{
			util_out_print( PREFIX_SOURCELOCAL "Processing State          !R22AZ [0x!8XL]", TRUE, idx,
				string, gtmsourcelocal_ptr->gtmsource_state);
		}

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, gtmsrc_lcl_array_index),
			sizeof(gtmsourcelocal_ptr->gtmsrc_lcl_array_index));
		util_out_print( PREFIX_SOURCELOCAL "Slot Index                       !15UL", TRUE, idx,
			gtmsourcelocal_ptr->gtmsrc_lcl_array_index);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, remote_proto_ver),
			sizeof(gtmsourcelocal_ptr->remote_proto_ver));
		util_out_print( PREFIX_SOURCELOCAL "Receiver Server Protocol Version             !3UL [0x!2XB]", TRUE, idx,
			gtmsourcelocal_ptr->remote_proto_ver, gtmsourcelocal_ptr->remote_proto_ver);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, read_state), sizeof(gtmsourcelocal_ptr->read_state));
		string = (READ_POOL == gtmsourcelocal_ptr->read_state) ? "POOL" :
				((READ_FILE == gtmsourcelocal_ptr->read_state) ? "FILE" : "UNKNOWN");
		if (MEMCMP_LIT(string, "UNKNOWN"))
			util_out_print( PREFIX_SOURCELOCAL "Currently Reading from     !R21AZ", TRUE, idx, string);
		else
		{
			util_out_print( PREFIX_SOURCELOCAL "Currently Reading from     !R21AZ [0x!8XL]", TRUE, idx,
				string, gtmsourcelocal_ptr->read_state);
		}

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, read), sizeof(gtmsourcelocal_ptr->read));
		util_out_print( PREFIX_SOURCELOCAL "Relative Read Offset                  !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->read, gtmsourcelocal_ptr->read);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, read_addr), sizeof(gtmsourcelocal_ptr->read_addr));
		util_out_print( PREFIX_SOURCELOCAL "Absolute Read Offset        !20@UJ [0x!16@XJ]", TRUE, idx,
			&gtmsourcelocal_ptr->read_addr, &gtmsourcelocal_ptr->read_addr);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, read_jnl_seqno), sizeof(gtmsourcelocal_ptr->read_jnl_seqno));
		util_out_print( PREFIX_SOURCELOCAL "Resync Sequence Number      !20@UJ [0x!16@XJ]", TRUE, idx,
			&gtmsourcelocal_ptr->read_jnl_seqno, &gtmsourcelocal_ptr->read_jnl_seqno);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, connect_jnl_seqno),
			sizeof(gtmsourcelocal_ptr->connect_jnl_seqno));
		util_out_print( PREFIX_SOURCELOCAL "Connect Sequence Number     !20@UJ [0x!16@XJ]", TRUE, idx,
			&gtmsourcelocal_ptr->connect_jnl_seqno, &gtmsourcelocal_ptr->connect_jnl_seqno);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, num_triples), sizeof(gtmsourcelocal_ptr->num_triples));
		util_out_print( PREFIX_SOURCELOCAL "Number of Triples                     !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->num_triples, gtmsourcelocal_ptr->num_triples);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, next_triple_num), sizeof(gtmsourcelocal_ptr->next_triple_num));
		util_out_print( PREFIX_SOURCELOCAL "Next Triple Number                    !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->next_triple_num, gtmsourcelocal_ptr->next_triple_num);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, next_triple_seqno),
			sizeof(gtmsourcelocal_ptr->next_triple_seqno));
		util_out_print( PREFIX_SOURCELOCAL "Next Triple Seqno           !20@UJ [0x!16@XJ]", TRUE, idx,
			&gtmsourcelocal_ptr->next_triple_seqno, &gtmsourcelocal_ptr->next_triple_seqno);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, last_flush_resync_seqno),
			sizeof(gtmsourcelocal_ptr->last_flush_resync_seqno));
		util_out_print( PREFIX_SOURCELOCAL "Last Flush Resync Seqno     !20@UJ [0x!16@XJ]", TRUE, idx,
			&gtmsourcelocal_ptr->last_flush_resync_seqno, &gtmsourcelocal_ptr->last_flush_resync_seqno);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, send_new_triple), sizeof(gtmsourcelocal_ptr->send_new_triple));
		PRINT_BOOLEAN( PREFIX_SOURCELOCAL "Send New Triple                         !R8AZ",
			gtmsourcelocal_ptr->send_new_triple, idx);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, send_losttn_complete),
			sizeof(gtmsourcelocal_ptr->send_losttn_complete));
		PRINT_BOOLEAN( PREFIX_SOURCELOCAL "Send LostTN Complete                    !R8AZ",
			gtmsourcelocal_ptr->send_losttn_complete, idx);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, secondary_host[0]),
			sizeof(gtmsourcelocal_ptr->secondary_host));
		if (20 >= strlen(gtmsourcelocal_ptr->secondary_host))
		{
			util_out_print( PREFIX_SOURCELOCAL "Secondary HOSTNAME          !R20AZ",
				TRUE, idx, gtmsourcelocal_ptr->secondary_host);
		} else
		{
			util_out_print( PREFIX_SOURCELOCAL "Secondary HOSTNAME          !AZ",
				TRUE, idx, gtmsourcelocal_ptr->secondary_host);
		}

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, secondary_inet_addr),
			sizeof(gtmsourcelocal_ptr->secondary_inet_addr));
		util_out_print( PREFIX_SOURCELOCAL "Secondary INET Address                !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->secondary_inet_addr, gtmsourcelocal_ptr->secondary_inet_addr);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, secondary_port), sizeof(gtmsourcelocal_ptr->secondary_port));
		util_out_print( PREFIX_SOURCELOCAL "Secondary Port                        !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->secondary_port, gtmsourcelocal_ptr->secondary_port);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, log_interval), sizeof(gtmsourcelocal_ptr->log_interval));
		util_out_print( PREFIX_SOURCELOCAL "Log Interval                          !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->log_interval, gtmsourcelocal_ptr->log_interval);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, log_file[0]), sizeof(gtmsourcelocal_ptr->log_file));
		if (20 >= strlen(gtmsourcelocal_ptr->log_file))
			util_out_print( PREFIX_SOURCELOCAL "Log File                    !R20AZ",
				TRUE, idx, gtmsourcelocal_ptr->log_file);
		else
			util_out_print( PREFIX_SOURCELOCAL "Log File                    !AZ",
				TRUE, idx, gtmsourcelocal_ptr->log_file);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, changelog), sizeof(gtmsourcelocal_ptr->changelog));
		util_out_print( PREFIX_SOURCELOCAL "Changelog                             !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->changelog, gtmsourcelocal_ptr->changelog);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, statslog), sizeof(gtmsourcelocal_ptr->statslog));
		util_out_print( PREFIX_SOURCELOCAL "Statslog                              !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->statslog, gtmsourcelocal_ptr->statslog);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, statslog_file[0]), sizeof(gtmsourcelocal_ptr->statslog_file));
		if (20 >= strlen(gtmsourcelocal_ptr->log_file))
		{
			util_out_print( PREFIX_SOURCELOCAL "Statslog File               !R20AZ",
				TRUE, idx, gtmsourcelocal_ptr->statslog_file);
		} else
		{
			util_out_print( PREFIX_SOURCELOCAL "Statslog File               !AZ",
				TRUE, idx, gtmsourcelocal_ptr->statslog_file);
		}

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT]),
			sizeof(gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT]));
		util_out_print( PREFIX_SOURCELOCAL "Connect Parms Hard Tries Count        !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT],
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT]);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD]),
			sizeof(gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD]));
		util_out_print( PREFIX_SOURCELOCAL "Connect Parms Hard Tries Period       !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD],
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD]);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]),
			sizeof(gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]));
		util_out_print( PREFIX_SOURCELOCAL "Connect Parms Soft Tries Period       !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD],
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, connect_parms[GTMSOURCE_CONN_ALERT_PERIOD]),
			sizeof(gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_ALERT_PERIOD]));
		util_out_print( PREFIX_SOURCELOCAL "Connect Parms Alert Period            !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_ALERT_PERIOD],
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_ALERT_PERIOD]);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD]),
			sizeof(gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD]));
		util_out_print( PREFIX_SOURCELOCAL "Connect Parms Heartbeat Period        !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD],
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD]);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT]),
			sizeof(gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT]));
		util_out_print( PREFIX_SOURCELOCAL "Connect Parms Heartbeat Max Wait      !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT],
			gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT]);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, shutdown), sizeof(gtmsourcelocal_ptr->shutdown));
		util_out_print( PREFIX_SOURCELOCAL "Shutdown State                        !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->shutdown, gtmsourcelocal_ptr->shutdown);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, shutdown_time), sizeof(gtmsourcelocal_ptr->shutdown_time));
		util_out_print( PREFIX_SOURCELOCAL "Shutdown Time in seconds              !10UL [0x!8XL]", TRUE, idx,
			gtmsourcelocal_ptr->shutdown_time, gtmsourcelocal_ptr->shutdown_time);

		PRINT_OFFSET_PREFIX(offsetof(gtmsource_local_struct, filter_cmd[0]), sizeof(gtmsourcelocal_ptr->filter_cmd));
		if (20 >= strlen(gtmsourcelocal_ptr->filter_cmd))
		{
			util_out_print( PREFIX_SOURCELOCAL "Filter Command              !R20AZ",
				TRUE, idx, gtmsourcelocal_ptr->filter_cmd);
		} else
		{
			util_out_print( PREFIX_SOURCELOCAL "Filter Command              !AZ",
				TRUE, idx, gtmsourcelocal_ptr->filter_cmd);
		}
		PRINT_DASHES;
	}
}
