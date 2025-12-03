/****************************************************************
*								*
 * Copyright (c) 2006-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_ipc.h"
#include <sys/sem.h>

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
#include "repl_inst_dump.h"
#include "is_proc_alive.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "ftok_sems.h"
#include "gtm_sem.h"
#include "gtmsource_srv_latch.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;		/* Used to access the gtmsource_local structures (slots) */
GBLREF	boolean_t		in_repl_inst_edit;	/* used by an assert in repl_inst_read/repl_inst_write */
GBLREF	boolean_t		detail_specified;	/* set to TRUE if -DETAIL is specified */
GBLREF	uint4			section_offset;		/* Used by PRINT_OFFSET_PREFIX macro in repl_inst_dump.c */

error_def(ERR_JNLPOOLSETUP);
error_def(ERR_MUPCLIERR);
error_def(ERR_SIZENOTVALID8);

void	mupcli_get_offset_size_value(uint4 *offset, uint4 *size, gtm_uint64_t *value, boolean_t *value_present)
{
	if (!cli_get_hex("OFFSET", offset))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
	if (!cli_get_hex("SIZE", size))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
	if (!((SIZEOF(char) == *size) || (SIZEOF(short) == *size) || (SIZEOF(int4) == *size) || (SIZEOF(gtm_int64_t) == *size)))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_SIZENOTVALID8);
	if (0 > (int4)*size)
	{
		util_out_print("Error: SIZE specified cannot be negative", TRUE);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
	}
	if (0 != (*offset % *size))
	{
		util_out_print("Error: OFFSET [0x!XL] should be a multiple of Size [!UL]", TRUE, *offset, *size);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
	}
	if (CLI_PRESENT == cli_present("VALUE"))
	{
		*value_present = TRUE;
		if (!cli_get_hex64("VALUE", value))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
	} else
		*value_present = FALSE;
}

void	mupcli_edit_offset_size_value(sm_uc_ptr_t buff, uint4 offset, uint4 size, gtm_uint64_t value, boolean_t value_present)
{
	char		temp_str[MAX_REPL_OPMSG_LEN], temp_str1[MAX_REPL_OPMSG_LEN];
	gtm_uint64_t	old_value = -1;

	memset(temp_str, 0, MAX_REPL_OPMSG_LEN);
	memset(temp_str1, 0, MAX_REPL_OPMSG_LEN);
	if (SIZEOF(char) == size)
	{
		SNPRINTF(temp_str, MAX_REPL_OPMSG_LEN, "!UB [0x!XB]");
		old_value = *(sm_uc_ptr_t)buff;
	}
	else if (SIZEOF(short) == size)
	{
		SNPRINTF(temp_str, MAX_REPL_OPMSG_LEN, "!UW [0x!XW]");
		old_value = *(sm_ushort_ptr_t)buff;
	}
	else if (SIZEOF(int4) == size)
	{
		SNPRINTF(temp_str, MAX_REPL_OPMSG_LEN, "!UL [0x!XL]");
		old_value = *(sm_uint_ptr_t)buff;
	}
	else if (SIZEOF(gtm_int64_t) == size)
	{
		SNPRINTF(temp_str, MAX_REPL_OPMSG_LEN, "!@UQ [0x!@XQ]");
		old_value = *(qw_num_ptr_t)buff;
	}
	assert(-1 != old_value);
	if (value_present)
	{
		if (SIZEOF(char) == size)
			*(sm_uc_ptr_t)buff = (unsigned char)value;
		else if (SIZEOF(short) == size)
			*(sm_ushort_ptr_t)buff = (unsigned short)value;
		else if (SIZEOF(int4) == size)
			*(sm_uint_ptr_t)buff = (unsigned int)value;
		else if (SIZEOF(gtm_int64_t) == size)
			*(qw_num_ptr_t)buff = value;
	} else
		value = old_value;
	SNPRINTF(temp_str1, MAX_REPL_OPMSG_LEN, "Offset !UL [0x!XL] : Old Value = %s : New Value = %s : Size = !UB [0x!XB]",
		temp_str, temp_str);
	if (SIZEOF(int4) >= size)
		util_out_print(temp_str1, TRUE, offset, offset, (uint4)old_value, (uint4)old_value,
			(uint4)value, (uint4)value, size, size);
	else
		util_out_print(temp_str1, TRUE, offset, offset, &old_value, &old_value,
			&value, &value, size, size);
}

/* Description:
 *	Edits or displays the contents of a replication instance file.
 * Parameters:
 	None
 * Return Value:
 	None
 */
void	repl_inst_edit(void)
{
	unsigned short		inst_fn_len, instname_len;
	char			inst_fn[MAX_FN_LEN + 1], buff_unaligned[REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE + 8];
	char			instname[MAX_INSTNAME_LEN];
	char			*buff;
	repl_inst_hdr_ptr_t	repl_instance;
	gtmsrc_lcl_ptr_t	gtmsrclcl_ptr;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
	uint4			offset, size;
	gtm_uint64_t		value;
	boolean_t		value_present, name_present, change_present, jnlpool_available, need_inst_access, need_lock;
	int			idx, num_slots_cleaned;
	int 			status, save_errno;
	int			qdbrundown_status, cleanslots_status;
	off_t			off;
	unix_db_info		*udi = NULL;
	union semun		semarg;
	struct semid_ds		semstat;

	in_repl_inst_edit = IN_REPL_INST_EDIT_TRUE;	/* Indicate to "repl_inst_read" and "jnlpool_init"
							 * we are in MUPIP REPLIC -EDITINSTANCE
							 */
	inst_fn_len = MAX_FN_LEN;
	if (!cli_get_str("INSTFILE", inst_fn, &inst_fn_len) || (0 == inst_fn_len))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
	inst_fn[inst_fn_len] = '\0';
	buff = &buff_unaligned[0];
	buff = (char *)ROUND_UP2((INTPTR_T)buff, 8);
	/* Make sure journal pool is initially not open while we're in  MUPIP REPLIC -EDITINSTANCE */
	assert(NULL == jnlpool);
	jnlpool_available = FALSE;
	if (CLI_PRESENT == cli_present("SHOW"))
	{
		detail_specified = (CLI_PRESENT == cli_present("DETAIL"));
		repl_inst_read(inst_fn, (off_t)0, (sm_uc_ptr_t)buff, REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
		util_out_print("GTM-I-MUREPLSHOW, SHOW output for replication instance file !AD", TRUE, inst_fn_len, inst_fn);
		repl_instance = (repl_inst_hdr_ptr_t)&buff[0];
		section_offset = 0;
		repl_inst_dump_filehdr(repl_instance);
		section_offset = REPL_INST_HDR_SIZE;
		repl_inst_dump_gtmsrclcl((gtmsrc_lcl_ptr_t)&buff[REPL_INST_HDR_SIZE]);
		section_offset = REPL_INST_HISTINFO_START;
		repl_inst_dump_history_records(inst_fn, repl_instance->num_histinfo);
	}
	change_present = (CLI_PRESENT == cli_present("CHANGE"));
	name_present = (CLI_PRESENT == cli_present("NAME"));
	qdbrundown_status = cli_present("QDBRUNDOWN");
	cleanslots_status = cli_present("CLEANSLOTS");
	/* Check if need to access and modify the instance file */
	need_inst_access = change_present || name_present || qdbrundown_status || cleanslots_status;
	need_lock = FALSE;
	if (need_inst_access)
	{
		need_lock = TRUE;
		repl_inst_read(inst_fn, (off_t)0, (sm_uc_ptr_t)buff, REPL_INST_HDR_SIZE);
		repl_instance = (repl_inst_hdr_ptr_t)&buff[0];
		semarg.buf = &semstat;
		/* Check if primary instance has potentially crashed.
		 * If so, set need_lock to FALSE so we can avoid calling
		 * jnlpool_init() and avoid a REPLREQROLLBACK error.
		 */
		if ((INVALID_SEMID != repl_instance->jnlpool_semid)
				&& (-1 == semctl(repl_instance->jnlpool_semid, DB_CONTROL_SEM, IPC_STAT, semarg)))
			need_lock = FALSE;
	}
	if (need_lock)
	{
		assert(NULL == jnlpool);
		/* Attach to journal pool so we can obtain locks and access slots */
		jnlpool_init(GTMRELAXED, (boolean_t)FALSE, (boolean_t *)NULL, NULL);
		assert(NULL != jnlpool);
		jnlpool_available = ((NULL != jnlpool->repl_inst_filehdr)
					&& (INVALID_SEMID != jnlpool->repl_inst_filehdr->jnlpool_semid)
					&& (INVALID_SHMID != jnlpool->repl_inst_filehdr->jnlpool_shmid));
		assert(NULL != jnlpool->jnlpool_dummy_reg);
		udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
		assert((NULL != udi) && udi->grabbed_ftok_sem);
		if (jnlpool_available)
		{
			assert(!udi->grabbed_access_sem);
			/* Journal pool shared memory and semaphore is available
			 * so grab the journal pool access semaphore
			 */
			status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
			if (SS_NORMAL != status)
			{
				save_errno = errno;
				ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented, TRUE);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error with journal pool access semaphore, no changes were made"),
					save_errno);
			}
			udi->grabbed_access_sem = TRUE;
			udi->counter_acc_incremented = TRUE;
			/* Now that we have the access semaphore, we
			 * can release the ftok and then do grab_lock.
			 */
			ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented, FALSE);
			gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
			for (idx = 0; idx < NUM_GTMSRC_LCL; idx++, gtmsourcelocal_ptr++)
				grab_gtmsource_srv_latch(&gtmsourcelocal_ptr->gtmsource_srv_latch, UINT32_MAX,
						ASSERT_NO_ONLINE_ROLLBACK);
			grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		}
	}
	/* Assert that when lock is needed, jnlpool is attached and either the ftok or jnlpool access semaphore is obtained */
	assert(!need_inst_access || !need_lock
			|| ((NULL != jnlpool) && (NULL != udi)
				&& ((jnlpool_available && udi->grabbed_access_sem)
					|| (!jnlpool_available && udi->grabbed_ftok_sem))));
	if (change_present)
	{
		/* Indicate to "repl_inst_read" that we are in a MUPIP REPLIC -EDITINSTANCE -CHANGE command by modifying
		 * the global variable "in_repl_inst_edit" to a more specific value for the duration of the CHANGE command.
		 */
		assert(IN_REPL_INST_EDIT_CHANGE_OFFSET != IN_REPL_INST_EDIT_TRUE);
		assert(IN_REPL_INST_EDIT_CHANGE_OFFSET != IN_REPL_INST_EDIT_FALSE);
		in_repl_inst_edit = IN_REPL_INST_EDIT_CHANGE_OFFSET;	/* needed by "repl_inst_read" to avoid errors */
		mupcli_get_offset_size_value(&offset, &size, &value, &value_present);
		assert(size <= REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
		repl_inst_read(inst_fn, (off_t)offset, (sm_uc_ptr_t)buff, size);
		mupcli_edit_offset_size_value((sm_uc_ptr_t)buff, offset, size, value, value_present);
		repl_inst_write(inst_fn, (off_t)offset, (sm_uc_ptr_t)buff, size);
		in_repl_inst_edit = IN_REPL_INST_EDIT_TRUE;
	}
	if (name_present || qdbrundown_status || cleanslots_status)
	{
		if (name_present)
		{	/* Edit the instance name */
			instname_len = MAX_INSTNAME_LEN;
			assert(MAX_INSTNAME_LEN == SIZEOF(instname));
			if (!cli_get_str("NAME", instname, &instname_len))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
			assert(MAX_INSTNAME_LEN >= instname_len);
			if (MAX_INSTNAME_LEN == instname_len)
			{
				util_out_print("Error: Instance name length can be at most 15", TRUE);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUPCLIERR);
			}
		}
		repl_inst_read(inst_fn, (off_t)0, (sm_uc_ptr_t)buff, REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
		repl_instance = (repl_inst_hdr_ptr_t)&buff[0];
		if (name_present)
		{
			util_out_print("HDR Instance Name changing from !AZ to !AZ",
					TRUE, repl_instance->inst_info.this_instname, instname);
			assert('\0' == instname[instname_len]);
			memcpy(repl_instance->inst_info.this_instname, instname, instname_len + 1);
		}
		if (qdbrundown_status)
		{
			util_out_print("HDR Quick database rundown is active changing from !AZ to !AZ",
					TRUE, repl_instance->qdbrundown ? "TRUE" : "FALSE",
					(CLI_PRESENT == qdbrundown_status) ? "TRUE" : "FALSE");
			repl_instance->qdbrundown = (CLI_PRESENT == qdbrundown_status);
		}
		if (cleanslots_status && jnlpool_available)
		{
			assert(udi);
			assert(udi->grabbed_access_sem);
			/* If shared memory is available and clean up desired,
			 * then clean up the inactive gtmsource_local structures (slots)
			 * from shared memory and make the corresponding changes to the
			 * gtmsrc_lcl strunctures in the instance file header
			 */
			assert((NULL != jnlpool->jnlpool_dummy_reg) && jnlpool->jnlpool_dummy_reg->open);
			gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
			gtmsrclcl_ptr = (gtmsrc_lcl_ptr_t)&buff[REPL_INST_HDR_SIZE];
			num_slots_cleaned = 0;
			for (idx = 0; idx < NUM_GTMSRC_LCL; idx++, gtmsourcelocal_ptr++, gtmsrclcl_ptr++)
			{
				assert((NULL != gtmsourcelocal_ptr) && (NULL != gtmsrclcl_ptr));
				/* Check if slot is used and inactive */
				if ((('\0' != gtmsourcelocal_ptr->secondary_instname[0])
						|| (0 != gtmsourcelocal_ptr->read_jnl_seqno)
						|| (0 != gtmsourcelocal_ptr->connect_jnl_seqno))
					&& ((GTMSOURCE_DUMMY_STATE == gtmsourcelocal_ptr->gtmsource_state)
						|| (NO_SHUTDOWN != gtmsourcelocal_ptr->shutdown)
						|| !is_proc_alive(gtmsourcelocal_ptr->gtmsource_pid, 0)))
				{
					/* Slot is inactive so clean (i.e. set certain
					 * fields to default value so slot can be reused)
					 */
					gtmsourcelocal_ptr->secondary_instname[0] = '\0';
					gtmsourcelocal_ptr->gtmsource_state = GTMSOURCE_DUMMY_STATE;
					gtmsourcelocal_ptr->read_jnl_seqno = 0;
					gtmsourcelocal_ptr->connect_jnl_seqno = 0;
					UNSET_SRC_NEEDS_JPLWRITES(jnlpool,gtmsourcelocal_ptr);
					gtmsourcelocal_ptr->gtmsource_pid = 0;
					gtmsourcelocal_ptr->gtmsource_pstarttime = 0;
					/* Clean the corresponding gtmsrc_lcl structure */
					COPY_GTMSOURCELOCAL_TO_GTMSRCLCL(gtmsourcelocal_ptr, gtmsrclcl_ptr);
					gtmsourcelocal_ptr->last_flush_resync_seqno = gtmsourcelocal_ptr->read_jnl_seqno;
					num_slots_cleaned++;
				}
			}
		} else if (cleanslots_status)
		{
			assert(udi);
			assert(udi->grabbed_ftok_sem);
			/* If shared memory is not available and cleaning desired,
			 * only clean the inactive gtmsrc_lcl structures because the
			 * gtmsource_local structures are not available.
			 */
			gtmsrclcl_ptr = (gtmsrc_lcl_ptr_t)&buff[REPL_INST_HDR_SIZE];
			num_slots_cleaned = 0;
			for (idx = 0; idx < NUM_GTMSRC_LCL; idx++, gtmsrclcl_ptr++)
			{
				assert(NULL != gtmsrclcl_ptr);
				/* Skip slots that are not used */
				if (('\0' == gtmsrclcl_ptr->secondary_instname[0])
						&& (0 == gtmsrclcl_ptr->resync_seqno)
						&& (0 == gtmsrclcl_ptr->connect_jnl_seqno))
					continue;
				/* Clean the slot. The gtmsrc_lcl structure only has three
				 * fields as opposed to the gtmsource_local structure which
				 * has numerous fields.
				 */
				gtmsrclcl_ptr->secondary_instname[0] = '\0';
				gtmsrclcl_ptr->resync_seqno = 0;
				gtmsrclcl_ptr->connect_jnl_seqno = 0;
				num_slots_cleaned++;
			}
		}
		if (cleanslots_status)
		{
			util_out_print("Cleaned !2UL inactive source server slot(s) from instance file header",
					TRUE, num_slots_cleaned);
		}
		/* Write/Flush any changes to the instance file header */
		repl_inst_write(inst_fn, (off_t)0, (sm_uc_ptr_t)buff, REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
	}
	if (need_lock)
	{
		assert(udi);
		if (jnlpool_available)
		{
			assert(udi->grabbed_access_sem);
			/* Release the lock and journal pool access semaphore */
			rel_lock(jnlpool->jnlpool_dummy_reg);
			gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
			for (idx = 0; idx < NUM_GTMSRC_LCL; idx++, gtmsourcelocal_ptr++)
				rel_gtmsource_srv_latch(&gtmsourcelocal_ptr->gtmsource_srv_latch);
			rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
		} else
		{
			assert(udi->grabbed_ftok_sem);
			/* Release the ftok */
			ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented, FALSE);
		}
	}
	in_repl_inst_edit = IN_REPL_INST_EDIT_FALSE;
}
