/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h> /* for offsetof() macro */
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cdb_sc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "gdscc.h"
#include "iosp.h"
#include <mdefsp.h>
#include "ccp.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "copy.h"
#include "jnl_get_checksum.h"
#include "gdsblk.h"		/* for blk_hdr usage in JNL_MAX_SET_KILL_RECLEN macro */
#include "gtmcrypt.h"

#define UPDATE_JGBL_FIELDS_IF_ZTWORM(IS_ZTWORM, IS_ZTWORM_POST_TRIG, SRC_PTR)						\
{															\
	if (!jgbl.forw_phase_recovery)											\
	{														\
		if (IS_ZTWORM)												\
		{													\
			/* In case of a ZTWORMHOLE, it should be immediately followed by a SET or KILL record.		\
			 * We do not maintain different update_num values for the ZTWORMHOLE and its			\
			 * corresponding SET or KILL record. So we should decrement the update_num before		\
			 * returning from this function in the hope that the next time jnl_format is called for		\
			 * the SET or KILL, update_num will be incremented thereby using the exact same value		\
			 * that was used for the ZTWORMHOLE record. An exception is journal recovery forward		\
			 * phase in which case, we don't do any increments of jgbl.tp_ztp_jnl_upd_num so we		\
			 * should do no decrements either. But this check is already done in a previous "if"		\
			 * above and so no such check is needed here again.						\
			 */												\
			jgbl.tp_ztp_jnl_upd_num--;									\
			/* Update jgbl.pre_trig_ztworm_ptr for the JNL_ZTWORM case.					\
			 * This is used by a later call to REMOVE_ZTWORM_JFB_IF_NEEDED.					\
			 */												\
			jgbl.pre_trig_ztworm_ptr = (unsigned char *)SRC_PTR;						\
		} else if (IS_ZTWORM_POST_TRIG)										\
		{	/* Update jgbl.prev_ztworm_ptr for the JNL_ZTWORM_POST_TRIG case.  */				\
			jgbl.prev_ztworm_ptr = (unsigned char *)SRC_PTR;						\
		}													\
	}														\
}

GBLREF	gd_region		*gv_cur_region;
GBLREF 	uint4			dollar_tlevel;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	unsigned int		t_tries;
#ifdef GTM_TRIGGER
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
#endif
#ifdef DEBUG
GBLREF	boolean_t		is_replicator;
#endif

/* Do NOT define first dimension of jnl_opcode array to be JA_MAX_TYPES. Instead let compiler fill in the value according
 * to the number of rows actually specified in the array. This way, if ever a new entry is added in jnl.h to jnl_action_code
 * (thereby increasing JA_MAX_TYPES) but is forgotten to add a corresponding row here, an assert (in this module) will fail
 * indicating the inconsistency. Defining jnl_opcode[JA_MAX_TYPES][5] causes any changes to JA_MAX_TYPES to automatically
 * allocate a bigger array filled with 0s which might cause one to overlook the inconsistency.
 */
static	const	enum jnl_record_type	jnl_opcode[][5] =
{
	{ JRT_KILL,  JRT_FKILL,  JRT_TKILL,   JRT_GKILL,  JRT_UKILL   },	/* JNL_KILL             */
	{ JRT_SET,   JRT_FSET,   JRT_TSET,    JRT_GSET,   JRT_USET    },	/* JNL_SET              */
	{ JRT_ZKILL, JRT_FZKILL, JRT_TZKILL,  JRT_GZKILL, JRT_UZKILL  },	/* JNL_ZKILL            */
#	ifdef GTM_TRIGGER
	{ JRT_BAD,   JRT_BAD,    JRT_TZTWORM, JRT_BAD,    JRT_UZTWORM },	/* JNL_ZTWORM           */
	{ JRT_BAD,   JRT_BAD,    JRT_TZTWORM, JRT_BAD,    JRT_UZTWORM },	/* JNL_ZTWORM_POST_TRIG */
	{ JRT_BAD,   JRT_BAD,    JRT_TLGTRIG, JRT_BAD,    JRT_ULGTRIG },	/* JNL_LGTRIG           */
	{ JRT_BAD,   JRT_BAD,    JRT_TZTRIG,  JRT_BAD,    JRT_UZTRIG  },	/* JNL_ZTRIG            */
#	endif
};

jnl_format_buffer *jnl_format(jnl_action_code opcode, gv_key *key, mval *val, uint4 nodeflags)
{
	char			*local_buffer, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			subcode;
	jnl_record		*rec;
	jnl_format_buffer	*prev_jfb, *jfb, *prev_prev_jfb;
	jnl_str_len_t		keystrlen = 0;
	mstr_len_t		valstrlen = -1;
	sgm_info		*si;
	sgmnt_addrs		*csa;
	uint4			align_fill_size, jrec_size, tmp_jrec_size;
	uint4			cursum, ztworm_post_trig_update_num;
	int			gtmcrypt_errno;
	gd_segment		*seg;
	char			iv[GTM_MAX_IV_LEN];
	boolean_t		use_new_key, is_ztworm, is_ztworm_post_trig;
	enc_info_t		*encr_ptr;
#	ifdef GTM_TRIGGER
	boolean_t		ztworm_matched, match_possible;
#	endif
#	ifdef DEBUG
	static boolean_t	dbg_in_jnl_format = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	/* We are about to write a logical jnl record so caller better have set "is_replicator" for us to write to the
	 * journal pool (if replication is turned on in this database). The only exceptions are
	 *	a) forward phase of journal recovery which runs with replication turned off so "is_replicator" does not
	 *		matter to it.
	 *	b) MUPIP TRIGGER -UPGRADE which does an inline upgrade of the ^#t global with journaling but replication
	 *		turned off.
	 */
	assert(is_replicator || jgbl.forw_phase_recovery || TREF(in_trigger_upgrade));
	/* The below assert ensures that if ever jnl_format is interrupted by a signal, the interrupt handler never calls
	 * jnl_format again. This is because jnl_format plays with global pointers and we would possibly end up in a bad
	 * state if the interrupt handler calls jnl_format again.
	 */
	assert(!dbg_in_jnl_format);
	DEBUG_ONLY(dbg_in_jnl_format = TRUE;)
	if (jgbl.forw_phase_recovery)	/* In case of recovery, copy "nodeflags" from journal record being played */
		nodeflags = jgbl.mur_jrec_nodeflags;
	csa = &FILE_INFO(gv_cur_region)->s_addrs;
	is_ztworm = (JNL_ZTWORM == opcode);
	is_ztworm_post_trig = (JNL_ZTWORM_POST_TRIG == opcode);
	if (is_ztworm_post_trig)
	{	/* In the JNL_ZTWORM_POST_TRIG case, "key" parameter is overloaded to contain the "jfb" created during the
		 * previous call to "jnl_format()" for the JNL_ZTWORM case. Use that instead of creating a new "jfb".
		 * Skip most of the code that is done for the normal "jnl_format()" case. Do just reformatting related code.
		 */
		assert(!jgbl.forw_phase_recovery);	/* Forward phase of mupip journal recover/rollback do not apply triggers */
		jfb = (jnl_format_buffer *)key;
		key = NULL;	/* clear this so later code can work right */
		rectype = jfb->rectype;
		/* Adjust a few fields to reflect the fact that the prior value of $ZTWORMHOLE is going to be replaced with
		 * the new value of $ZTWORMHOLE. All that is needed is remove the updates done for the prior value. They
		 * will be updated for the new value in later code in this function.
		 */
		si = sgm_info_ptr;
		si->total_jnl_rec_size -= (int)(jfb->record_size + MIN_ALIGN_RECLEN);
		assert(0 < si->total_jnl_rec_size);
		jgbl.cumul_jnl_rec_len -= jfb->record_size;
		assert(0 < jgbl.cumul_jnl_rec_len);
		DEBUG_ONLY(jgbl.cumul_index--;)
		assert(0 < jgbl.cumul_index);
		ztworm_post_trig_update_num = ((jnl_record *)jfb->buff)->jrec_set_kill.update_num;
	} else
	{	/* Allocate a jfb structure */
		jnl_action	*ja;

		if (!dollar_tlevel)
		{
			jfb = non_tp_jfb_ptr; /* already malloced in "gvcst_init" */
			assert(!is_ztworm && !is_ztworm_post_trig);
			jgbl.cumul_jnl_rec_len = 0;
			DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
			DEBUG_ONLY(si = NULL;)
		} else
		{
			si = sgm_info_ptr;
			assert(si->tp_csa == csa);
			assert((NULL != si->jnl_head) || (NULL == csa->next_fenced));
			assert((NULL == si->jnl_head) || (NULL != csa->next_fenced));
			assert((NULL == csa->next_fenced) || (JNL_FENCE_LIST_END == csa->next_fenced)
							|| (NULL != csa->next_fenced->sgm_info_ptr->jnl_head));
			jfb = (jnl_format_buffer *)get_new_element(si->jnl_list, 1);
			jfb->next = NULL;
			assert(NULL != si->jnl_tail);
			GTMTRIG_ONLY(SET_PREV_JFB(si, jfb->prev);)
			assert(NULL == *si->jnl_tail);
			*si->jnl_tail = jfb;
			si->jnl_tail = &jfb->next;
			si->update_trans |= UPDTRNS_JNL_LOGICAL_MASK;	/* as we are writing a logical jnl record in this region */
			if (!(nodeflags & JS_NOT_REPLICATED_MASK))
				si->update_trans |= UPDTRNS_JNL_REPLICATED_MASK;
		}
		ja = &(jfb->ja);
		ja->operation = opcode;
		ja->nodeflags = nodeflags;
		/* Proceed with formatting the journal record in the allocated jfb */
		if (!jnl_fence_ctl.level && !dollar_tlevel)
		{	/* Non-TP */
			subcode = 0;
			assert(0 == jgbl.tp_ztp_jnl_upd_num);
		} else
		{
			if (NULL == csa->next_fenced)
			{	/* F (or T) */
				assert((NULL != jnl_fence_ctl.fence_list) || (0 == jgbl.tp_ztp_jnl_upd_num));
				subcode = 1;
				csa->next_fenced = jnl_fence_ctl.fence_list;
				jnl_fence_ctl.fence_list = csa;
			} else	/* G (or U) */
			{	/* If this is a U type of record (jnl_fence_ctl.level would be 0 in that case), at least one call
				 * to "jnl_format" has occurred in this TP transaction already. We therefore expect
				 * jgbl.tp_ztp_jnl_upd_num to be non-zero at this point. The only exception is if "jnl_format"
				 * had been called just once before and that was for a ZTWORM type of record in which case it would
				 * be zero (both ZTWORM and following SET/KILL record will have the same update_num value of 1).
				 */
				assert(jnl_fence_ctl.level || jgbl.tp_ztp_jnl_upd_num
				    GTMTRIG_ONLY(|| (si && (jfb->prev == si->jnl_head) && (JRT_TZTWORM == jfb->prev->rectype))));
				subcode = 3;
			}
			if (dollar_tlevel)
				++subcode; /* TP */
			else if (!jgbl.forw_phase_recovery && t_tries)
				jgbl.tp_ztp_jnl_upd_num--;	/* If ZTP, increment this only ONCE per update, not ONCE per retry.
								 * We do this by decrementing it if t_tries > 0 to balance the
								 * tp_ztp_jnl_upd_num++ done a few lines below.
								 */
			assert(FIXED_UPD_RECLEN == FIXED_ZTWORM_RECLEN);
			assert(FIXED_UPD_RECLEN == FIXED_LGTRIG_RECLEN);
			if (!jgbl.forw_phase_recovery)
				jgbl.tp_ztp_jnl_upd_num++;
			/* In case of forward phase of journal recovery, this would have already been set to appropriate value.
			 * It is necessary to honor the incoming jgbl value for ZTP (since recovery could be playing records
			 * from the middle of a ZTP transaction because the rest are before the EPOCH), but for TP it is not
			 * necessary since all records are guaranteed to be AFTER the EPOCH so we can generate the numbers in
			 * this function too. But since we expect recovery to play the TP records in the exact order in which
			 * GT.M wrote them no point regenerating the same set of numbers again here. So we use incoming jgbl always.
			 */
		}
		assert(ARRAYSIZE(jnl_opcode) == JA_MAX_TYPES);
		assert(JA_MAX_TYPES > opcode);
		rectype = jnl_opcode[opcode][subcode];
	}
	assert(IS_VALID_JRECTYPE(rectype));
	assert(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype));
	tmp_jrec_size = FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE;
	GTMTRIG_ONLY(assert((!is_ztworm && !is_ztworm_post_trig && (JNL_LGTRIG != opcode)) || (NULL == key));)
	GTMTRIG_ONLY(assert(is_ztworm || is_ztworm_post_trig || (JNL_LGTRIG == opcode) || (NULL != key));)
	/* Compute actual record length */
	if (NULL != key)
	{
		assert(!is_ztworm);
		keystrlen = key->end;
		tmp_jrec_size += keystrlen + SIZEOF(jnl_str_len_t);
	}
	GTMTRIG_ONLY(assert((!is_ztworm && !is_ztworm_post_trig && (JNL_LGTRIG != opcode)) || (NULL != val));)
	assert((JNL_SET != opcode) || (NULL != val));
	if (NULL != val)
	{
		valstrlen = val->str.len;
		tmp_jrec_size += valstrlen + SIZEOF(mstr_len_t);
	}
	jrec_size = ROUND_UP2(tmp_jrec_size, JNL_REC_START_BNDRY);
	align_fill_size = jrec_size - tmp_jrec_size; /* For JNL_REC_START_BNDRY alignment */
	if (dollar_tlevel)
	{
		assert((1 << JFB_ELE_SIZE_IN_BITS) == JNL_REC_START_BNDRY);
		assert(JFB_ELE_SIZE == JNL_REC_START_BNDRY);
		assert(si);
		jfb->buff = (char *)get_new_element(si->format_buff_list, jrec_size >> JFB_ELE_SIZE_IN_BITS);
		if (REPL_ALLOWED(csa))
			jfb->alt_buff = (char *)get_new_element(si->format_buff_list, jrec_size >> JFB_ELE_SIZE_IN_BITS);
		/* Assume an align record will be written while computing maximum jnl-rec size requirements */
		si->total_jnl_rec_size += (int)(jrec_size + MIN_ALIGN_RECLEN);
	}
	/* else if (!dollar_tlevel) jfb->buff/jfb->alt_buff already malloced in gvcst_init. */
	jfb->record_size = jrec_size;
	INCREMENT_JGBL_CUMUL_JNL_REC_LEN(jrec_size);	/* Bump jgbl.cumul_jrec_len by "jrec_size" */
	DEBUG_ONLY(jgbl.cumul_index++;)
	jfb->rectype = rectype;
	/* PREFIX */
	rec = (jnl_record *)jfb->buff;
	rec->prefix.jrec_type = rectype;
	assert(!IS_SET_KILL_ZKILL_ZTRIG(rectype) || (JNL_MAX_SET_KILL_RECLEN(csa->hdr) >= jrec_size));
	GTMTRIG_ONLY(assert(!IS_ZTWORM(rectype) || (MAX_ZTWORM_JREC_LEN >= jrec_size));)
	GTMTRIG_ONLY(assert(MAX_LGTRIG_JREC_LEN <= (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE));)
	GTMTRIG_ONLY(assert(!IS_LGTRIG(rectype) || (MAX_LGTRIG_JREC_LEN >= jrec_size));)
	rec->prefix.forwptr = jrec_size;
	assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
	assert(&rec->jrec_set_kill.update_num == &rec->jrec_lgtrig.update_num);
	rec->jrec_set_kill.update_num = !is_ztworm_post_trig ? jgbl.tp_ztp_jnl_upd_num : ztworm_post_trig_update_num;
	rec->jrec_set_kill.num_participants = 0;
	local_buffer = (char *)rec + FIXED_UPD_RECLEN;
	mumps_node_ptr = local_buffer;
	if (NULL != key)
	{
		assert(keystrlen);
		((jnl_string *)local_buffer)->length = keystrlen;
		((jnl_string *)local_buffer)->nodeflags = nodeflags;
		local_buffer += SIZEOF(jnl_str_len_t);
		memcpy(local_buffer, (uchar_ptr_t)key->base, keystrlen);
		local_buffer += keystrlen;
	}
	if (NULL != val)
	{
		assert(0 <= valstrlen);
		PUT_MSTR_LEN(local_buffer, valstrlen); /* SET command's data may not be aligned */
		/* The below assert ensures that it is okay for us to increment by jnl_str_len_t (uint4)
		 * even though valstrlen (above) is of type mstr_len_t (int). This is because PUT_MSTR_LEN
		 * casts the input to (uint4*) before storing it in the destination pointer (in this case
		 * local_buffer)
		 */
		assert(SIZEOF(uint4) == SIZEOF(jnl_str_len_t));
		local_buffer += SIZEOF(jnl_str_len_t);
		memcpy(local_buffer, (uchar_ptr_t)val->str.addr, valstrlen);
		local_buffer += valstrlen;
	}
	if (0 != align_fill_size)
	{
		memset(local_buffer, 0, align_fill_size);
		local_buffer += align_fill_size;
	}
	/* SUFFIX */
	((jrec_suffix *)local_buffer)->backptr = jrec_size;
	((jrec_suffix *)local_buffer)->suffix_code = JNL_REC_SUFFIX_CODE;
	assert(REPL_ALLOWED(csa) || (!is_ztworm && !is_ztworm_post_trig) || jgbl.forw_phase_recovery);
	/* If the fields in the database file header have been updated by a concurrent MUPIP REORG -ENCRYPT, we may end up not
	 * encrypting the journal records or encrypting them with wrong settings, which is OK because t_end / tp_tend will detect
	 * that the mupip_reorg_cycle flag in cnl has been updated, and restart the transaction.
	 */
	encr_ptr = csa->encr_ptr;
	if ((NULL != encr_ptr) && USES_ANY_KEY(encr_ptr))
	{	/* At this point we have all the components of *SET, *KILL, *ZTWORM and *ZTRIG records filled. */
		uint4	update_length;

#		ifdef DEBUG
		if (encr_ptr->reorg_encrypt_cycle != csa->nl->reorg_encrypt_cycle)
		{	/* This is a restartable situation for sure. But we cannot return a restart code from this function.
			 * So set a dbg-only variable to indicate we never expect this to commit and alert us if it does.
			 */
			/* Note: Cannot add assert(CDB_STAGNATE > t_tries) here (like we do for other donot_commit cases)
			 * because cdb_sc_reorg_encrypt restart code is possible in final retry too.
			 */
			TREF(donot_commit) |= DONOTCOMMIT_JNL_FORMAT;
		}
#		endif
		if (REPL_ALLOWED(csa))
		{	/* Before encrypting the journal record, copy the unencrypted buffer to an alternate buffer that eventually
			 * gets copied to the journal pool (in jnl_write). This way, the replication stream sends unencrypted data.
			 */
			memcpy(jfb->alt_buff, rec, jrec_size);
			UPDATE_JGBL_FIELDS_IF_ZTWORM(is_ztworm, is_ztworm_post_trig, (jfb->alt_buff + FIXED_UPD_RECLEN));
		}
		ASSERT_ENCRYPTION_INITIALIZED;
		if (encr_ptr->reorg_encrypt_cycle == csa->nl->reorg_encrypt_cycle)
			use_new_key = USES_NEW_KEY(encr_ptr);
		else	/* Cycle mismatch, use the current key unless there is none */
			use_new_key = (GTMCRYPT_INVALID_KEY_HANDLE != csa->encr_key_handle) ? FALSE : TRUE;
		assert((!use_new_key && (NULL != csa->encr_key_handle)) || (use_new_key && (NULL != csa->encr_key_handle2)));
		/* Encrypt the logical portion of the record, which eventually gets written to the journal buffer/file */
		if (use_new_key || encr_ptr->non_null_iv)
			PREPARE_LOGICAL_REC_IV(jrec_size, iv);
		update_length = (jrec_size - (JREC_SUFFIX_SIZE + FIXED_UPD_RECLEN));
		GTMCRYPT_ENCRYPT(csa, (use_new_key ? TRUE : encr_ptr->non_null_iv),
				(use_new_key ? csa->encr_key_handle2 : csa->encr_key_handle),
				mumps_node_ptr, update_length, NULL, iv, GTM_MAX_IV_LEN, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			seg = gv_cur_region->dyn.addr;
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
		}
	} else
		UPDATE_JGBL_FIELDS_IF_ZTWORM(is_ztworm, is_ztworm_post_trig, mumps_node_ptr);
	/* The below call to jnl_get_checksum makes sure that checksum computation happens AFTER the encryption (if turned on) */
	jfb->checksum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)mumps_node_ptr, (int)(local_buffer - mumps_node_ptr));
	assert(0 == ((UINTPTR_T)local_buffer % SIZEOF(jrec_suffix)));
	DEBUG_ONLY(dbg_in_jnl_format = FALSE;)
	return jfb;
}
