/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
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
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
GBLREF  jnl_process_vector	*prc_vec;
GBLREF 	jnl_gbls_t		jgbl;
#ifdef UNIX
GBLREF	int4			strm_index;
#endif

void jfh_from_jnl_info(jnl_create_info *info, jnl_file_header *header)
{
	int		idx;
	trans_num	db_tn;

	/**** We will write journal file header, epoch and eof in order ****/
	/* Write the file header */
	memset((char *)header, 0, JNL_HDR_LEN);	/* note: In Unix, this means we 0-fill 2K REAL_JNL_HDR_LEN + 62K padding */
	memcpy(header->label, JNL_LABEL_TEXT, STR_LIT_LEN(JNL_LABEL_TEXT));
	header->is_little_endian = GTM_IS_LITTLE_ENDIAN;
	assert(NULL != prc_vec);
	JNL_WHOLE_FROM_SHORT_TIME(prc_vec->jpv_time, jgbl.gbl_jrec_time);
	memcpy(&header->who_created, (unsigned char*)prc_vec, SIZEOF(jnl_process_vector));
	memcpy(&header->who_opened,  (unsigned char*)prc_vec, SIZEOF(jnl_process_vector));
	/* EPOCHs are written unconditionally in Unix while they are written only for BEFORE_IMAGE in VMS */
	if (JNL_HAS_EPOCH(info))
		header->end_of_data = JNL_HDR_LEN + PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN;
	else
		header->end_of_data = JNL_HDR_LEN + PINI_RECLEN + PFIN_RECLEN;
	header->max_jrec_len = info->max_jrec_len;
	header->bov_timestamp = jgbl.gbl_jrec_time;
	header->eov_timestamp = jgbl.gbl_jrec_time;
	db_tn = info->csd->trans_hist.curr_tn;
	header->bov_tn = db_tn;
	header->eov_tn = db_tn;
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
	assert(JNL_ALLOC_MIN <= info->alloc);
	header->jnl_alq = info->alloc;
	header->virtual_size = info->alloc;
	header->jnl_deq = info->extend;
	header->checksum = info->checksum;
	GTMCRYPT_ONLY(
		GTMCRYPT_COPY_HASH(info, header);
	)
#	ifdef UNIX
	if (INVALID_SUPPL_STRM != strm_index)
	{
		assert(MAX_SUPPL_STRMS == ARRAYSIZE(header->strm_start_seqno));
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			header->strm_start_seqno[idx] = info->csd->strm_reg_seqno[idx];
		if (jgbl.forw_phase_recovery)
		{	/* If MUPIP JOURNAL -ROLLBACK, might need to do additional processing. See macro definition for comments */
			MUR_ADJUST_STRM_REG_SEQNO_IF_NEEDED(info->csd, header->strm_start_seqno);
		}
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			header->strm_end_seqno[idx] = header->strm_start_seqno[idx];
	} else
	{
		memset(&header->strm_start_seqno[0], 0, SIZEOF(header->strm_start_seqno));
		memset(&header->strm_end_seqno[0], 0, SIZEOF(header->strm_end_seqno));
	}
#	endif
}
