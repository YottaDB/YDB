/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

GBLREF  jnl_process_vector	*prc_vec;
GBLREF 	jnl_gbls_t		jgbl;

void jfh_from_jnl_info (jnl_create_info *info, jnl_file_header *header)
{
	/**** We will write journal file header, epoch and eof in order ****/
	/* Write the file header */
	memset((char *)header, 0, JNL_HDR_LEN);
	memcpy(header->label, JNL_LABEL_TEXT, sizeof(JNL_LABEL_TEXT) - 1);
	assert(NULL != prc_vec);
	JNL_WHOLE_FROM_SHORT_TIME(prc_vec->jpv_time, jgbl.gbl_jrec_time);
	memcpy(&header->who_created, (unsigned char*)prc_vec, sizeof(jnl_process_vector));
	memcpy(&header->who_opened,  (unsigned char*)prc_vec, sizeof(jnl_process_vector));
	if (info->before_images)
		header->end_of_data = JNL_HDR_LEN + PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN;
	else
		header->end_of_data = JNL_HDR_LEN + PINI_RECLEN + PFIN_RECLEN;
	header->max_phys_reclen = info->max_phys_reclen;
	header->max_logi_reclen = info->max_logi_reclen;
	header->bov_timestamp = jgbl.gbl_jrec_time;
	header->eov_timestamp = jgbl.gbl_jrec_time;
	header->bov_tn = info->tn;
	header->eov_tn = info->tn;
	header->before_images = info->before_images;
	/* Note that in case of MUPIP JOURNAL -ROLLBACK, we need to set header->repl_state to repl_open although replication
	 * is currently not ON in the database. This is so future ROLLBACKs know this journal is replication enabled.
	 */
	header->repl_state = jgbl.mur_rollback ? repl_open : info->repl_state;
	header->data_file_name_length = info->fn_len;
	memcpy(header->data_file_name, info->fn, info->fn_len);
	header->data_file_name[info->fn_len] = '\0';
	header->alignsize = info->alignsize;
	header->autoswitchlimit = info->autoswitchlimit;
	header->epoch_interval = info->epoch_interval;
	header->start_seqno = info->reg_seqno;
	header->end_seqno = info->reg_seqno;
	header->prev_jnl_file_name_length = info->prev_jnl_len; ;
	memcpy(header->prev_jnl_file_name, info->prev_jnl, info->prev_jnl_len);
	header->prev_jnl_file_name[info->prev_jnl_len] = '\0';
	header->jnl_alq = info->alloc;
	header->virtual_size = info->alloc;
	header->jnl_deq = info->extend;
}
