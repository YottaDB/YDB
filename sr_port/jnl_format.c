/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "copy.h"
#include "jnl_get_checksum.h"
#include "gdsblk.h"		/* for blk_hdr usage in JNL_MAX_SET_KILL_RECLEN macro */

#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

#ifdef GTM_TRIGGER
/* In case of a ZTWORMHOLE, it should be immediately followed by a SET or KILL record. We do not maintain different
 * update_num values for the ZTWORMHOLE and its corresponding SET or KILL record. So we should decrement the
 * update_num before returning from this function in the hope that the next time jnl_format is called for the SET
 * or KILL, update_num will be incremented thereby using the exact same value that was used for the ZTWORMHOLE record.
 * An exception is journal recovery forward phase in which case, we dont do any increments of jgbl.tp_ztp_jnl_upd_num
 * so we should do no decrements either.
 */
#define	ZTWORM_DECR_UPD_NUM	{ if (!jgbl.forw_phase_recovery) jgbl.tp_ztp_jnl_upd_num--; }

#define SET_PREV_ZTWORM_JFB_IF_NEEDED(is_ztworm_rec, src_ptr)		\
{									\
	if (is_ztworm_rec && !jgbl.forw_phase_recovery)			\
	{								\
		jgbl.save_ztworm_ptr = jgbl.prev_ztworm_ptr;		\
		jgbl.prev_ztworm_ptr = (unsigned char *)src_ptr;	\
		ZTWORM_DECR_UPD_NUM;					\
	}								\
}
#else
#define SET_PREV_ZTWORM_JFB_IF_NEEDED(is_ztworm_rec, src_ptr)
#endif

GBLREF	gd_region		*gv_cur_region;
GBLREF 	uint4			dollar_tlevel;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	jnl_gbls_t		jgbl;
#ifdef GTM_TRIGGER
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
#endif

/* Do NOT define first dimension of jnl_opcode array to be JA_MAX_TYPES. Instead let compiler fill in the value according
 * to the number of rows actually specified in the array. This way, if ever a new entry is added in jnl.h to jnl_action_code
 * (thereby increasing JA_MAX_TYPES) but is forgotten to add a corresponding row here, an assert (in this module) will fail
 * indicating the inconsistency. Defining jnl_opcode[JA_MAX_TYPES][5] causes any changes to JA_MAX_TYPES to automatically
 * allocate a bigger array filled with 0s which might cause one to overlook the inconsistency.
 */
static	const	enum jnl_record_type	jnl_opcode[][5] =
{
	{ JRT_KILL,  JRT_FKILL,  JRT_TKILL,   JRT_GKILL,  JRT_UKILL   },	/* KILL        record types */
	{ JRT_SET,   JRT_FSET,   JRT_TSET,    JRT_GSET,   JRT_USET    },	/* SET         record types */
	{ JRT_ZKILL, JRT_FZKILL, JRT_TZKILL,  JRT_GZKILL, JRT_UZKILL  },	/* ZKILL       record types */
#	ifdef GTM_TRIGGER
	{ JRT_BAD,   JRT_BAD,    JRT_TZTWORM, JRT_BAD,    JRT_UZTWORM },	/* ZTWORM      record types */
	{ JRT_BAD,   JRT_BAD,    JRT_TZTRIG,  JRT_BAD,    JRT_UZTRIG  },	/* ZTRIG       record types */
#	endif
};

jnl_format_buffer *jnl_format(jnl_action_code opcode, gv_key *key, mval *val, uint4 nodeflags)
{
	char			*local_buffer, *mumps_node_ptr;
	enum jnl_record_type	rectype;
	int			subcode;
	jnl_record		*rec;
	jnl_action		*ja;
	jnl_format_buffer	*prev_jfb, *jfb, *prev_prev_jfb;
	jnl_str_len_t		keystrlen;
	mstr_len_t		valstrlen;
	sgm_info		*si;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	uint4			align_fill_size, jrec_size, tmp_jrec_size, update_length;
	boolean_t		is_ztworm_rec = FALSE;
	uint4			cursum;
	DEBUG_ONLY(
		static boolean_t	dbg_in_jnl_format = FALSE;
	)
#	ifdef GTM_CRYPT
	int			gtmcrypt_errno;
	gd_segment		*seg;
#	endif
#	ifdef GTM_TRIGGER
	boolean_t		ztworm_matched, match_possible;
	mstr			prev_str, *cur_str;
#	endif

	/* The below assert ensures that if ever jnl_format is interrupted by a signal, the interrupt handler never calls
	 * jnl_format again. This is because jnl_format plays with global pointers and we would possibly end up in a bad
	 * state if the interrupt handler calls jnl_format again.
	 */
	assert(!dbg_in_jnl_format);
	DEBUG_ONLY(dbg_in_jnl_format = TRUE;)
	if (jgbl.forw_phase_recovery)	/* In case of recovery, copy "nodeflags" from journal record being played */
		nodeflags = jgbl.mur_jrec_nodeflags;
	csa = &FILE_INFO(gv_cur_region)->s_addrs;
	csd = csa->hdr;
#	ifdef GTM_TRIGGER
	/* If opcode is JNL_ZTWORM then check if ztwormhole operation can be avoided altogether.
	 * This is the case if the value of $ZTWORMHOLE passed in is identical to the value of
	 * $ZTWORMHOLE written for the immediately previous update stored in (global variable) jgbl.prev_ztworm_ptr
	 * across regions in the current TP transaction. In that case, return right away.
	 * For journal recovery, we skip this part since we want the ztwormhole record to be unconditionally written
	 * (because GT.M wrote it in the first place).
	 */
	is_ztworm_rec = (JNL_ZTWORM == opcode);
	if (is_ztworm_rec && !jgbl.forw_phase_recovery)
	{
		assert(REPL_ALLOWED(csa) || jgbl.forw_phase_recovery);
		assert(dollar_tlevel);
		assert(tstart_trigger_depth == gtm_trigger_depth);
		assert((NULL != val) && (NULL == key));
		assert(MV_IS_STRING(val));
		assert(FIXED_UPD_RECLEN == FIXED_ZTWORM_RECLEN);
		if (NULL != jgbl.prev_ztworm_ptr)
		{
			cur_str = &val->str;
			prev_str.len = (*(jnl_str_len_t *)jgbl.prev_ztworm_ptr);
			prev_str.addr = (char *)(jgbl.prev_ztworm_ptr + SIZEOF(jnl_str_len_t));
			if ((prev_str.len == cur_str->len) && !memcmp(prev_str.addr, cur_str->addr, prev_str.len))
			{
				DEBUG_ONLY(dbg_in_jnl_format = FALSE;)
				return NULL;
			}
		}
	}
#	endif
	/* Allocate a jfb structure */
	if (!dollar_tlevel)
	{
		jfb = non_tp_jfb_ptr; /* already malloced in gvcst_init() */
		jgbl.cumul_jnl_rec_len = 0;
		DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
	} else
	{
		si = sgm_info_ptr;	/* reset "si" since previous set was #ifdef GTM_TRIGGER only code while this is not */
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
		si->update_trans |= UPDTRNS_JNL_LOGICAL_MASK;	/* record that we are writing a logical jnl record in this region */
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
		tmp_jrec_size = FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE;
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
		{	/* At least one call to "jnl_format" has occurred in this TP transaction already. We therefore
			 * expect jgbl.tp_ztp_jnl_upd_num to be non-zero at this point. The only exception is if "jnl_format"
			 * had been called just once before and that was for a ZTWORM type of record in which case it would be
			 * zero (both ZTWORM and following SET/KILL record will have the same update_num value of 1).
			 */
			assert(jgbl.tp_ztp_jnl_upd_num
				GTMTRIG_ONLY(|| ((jfb->prev == si->jnl_head) && (JRT_TZTWORM == jfb->prev->rectype))));
			subcode = 3;
		}
		if (dollar_tlevel)
			++subcode; /* TP */
		tmp_jrec_size = FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE;
		assert(FIXED_UPD_RECLEN == FIXED_ZTWORM_RECLEN);
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
	assert(IS_VALID_JRECTYPE(rectype));
	assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype));
	GTMTRIG_ONLY(assert((JNL_ZTWORM != opcode) || (NULL == key));)
	GTMTRIG_ONLY(assert((JNL_ZTWORM == opcode) || (NULL != key));)
	/* Compute actual record length */
	if (NULL != key)
	{
		keystrlen = key->end;
		tmp_jrec_size += keystrlen + SIZEOF(jnl_str_len_t);
	}
	GTMTRIG_ONLY(assert((JNL_ZTWORM != opcode) || (NULL != val));)
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
		jfb->buff = (char *)get_new_element(si->format_buff_list, jrec_size >> JFB_ELE_SIZE_IN_BITS);
		GTMCRYPT_ONLY(
			if (REPL_ALLOWED(csa))
				jfb->alt_buff = (char *)get_new_element(si->format_buff_list, jrec_size >> JFB_ELE_SIZE_IN_BITS);
		)
		/* assume an align record will be written while computing maximum jnl-rec size requirements */
		si->total_jnl_rec_size += (int)(jrec_size + MIN_ALIGN_RECLEN);
	}
	/* else if (!dollar_tlevel) jfb->buff/jfb->alt_buff already malloced in gvcst_init. */
	jfb->record_size = jrec_size;
	jgbl.cumul_jnl_rec_len += jfb->record_size;
	assert(0 == jgbl.cumul_jnl_rec_len % JNL_REC_START_BNDRY);
	DEBUG_ONLY(jgbl.cumul_index++;)
	jfb->rectype = rectype;
	/* PREFIX */
	rec = (jnl_record *)jfb->buff;
	rec->prefix.jrec_type = rectype;
	assert(!IS_SET_KILL_ZKILL_ZTRIG(rectype) || (JNL_MAX_SET_KILL_RECLEN(csd) >= jrec_size));
	GTMTRIG_ONLY(assert(!IS_ZTWORM(rectype) || (MAX_ZTWORM_JREC_LEN >= jrec_size));)
	rec->prefix.forwptr = jrec_size;
	assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
	rec->jrec_set_kill.update_num = jgbl.tp_ztp_jnl_upd_num;
	rec->jrec_set_kill.num_participants = 0;
	local_buffer = (char *)rec + FIXED_UPD_RECLEN;
	mumps_node_ptr = local_buffer;
	if (NULL != key)
	{
		((jnl_string *)local_buffer)->length = keystrlen;
		((jnl_string *)local_buffer)->nodeflags = nodeflags;
		local_buffer += SIZEOF(jnl_str_len_t);
		memcpy(local_buffer, (uchar_ptr_t)key->base, keystrlen);
		local_buffer += keystrlen;
	}
	if (NULL != val)
	{
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
	update_length = (jrec_size - (JREC_SUFFIX_SIZE + FIXED_UPD_RECLEN));
#	ifdef GTM_CRYPT
	assert(REPL_ALLOWED(csa) || !is_ztworm_rec || jgbl.forw_phase_recovery);
	if (csd->is_encrypted)
	{
		/* At this point we have all the components of *SET, *KILL, *ZTWORM and *ZTRIG records filled. */
		if (REPL_ALLOWED(csa))
		{
			/* Before encrypting the journal record, copy the unencrypted buffer to an alternate buffer
			 * that eventually gets copied to the journal pool (in jnl_write). This way, the replication
			 * stream sends unencrypted data.
			 */
			memcpy(jfb->alt_buff, rec, jrec_size);
			SET_PREV_ZTWORM_JFB_IF_NEEDED(is_ztworm_rec, (jfb->alt_buff + FIXED_UPD_RECLEN));
		}
		ASSERT_ENCRYPTION_INITIALIZED;
		/* Encrypt the logical portion of the record which eventually gets written to the journal buffer/file */
		GTMCRYPT_ENCRYPT(csa, csa->encr_key_handle, mumps_node_ptr, update_length, NULL, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			seg = gv_cur_region->dyn.addr;
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
		}
	} else
#	endif
	{
		SET_PREV_ZTWORM_JFB_IF_NEEDED(is_ztworm_rec, mumps_node_ptr);
	}
	/* The below call to jnl_get_checksum makes sure that checksum computation happens AFTER the encryption (if turned on) */
	jfb->checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)mumps_node_ptr, (int)(local_buffer - mumps_node_ptr));
	assert(0 == ((UINTPTR_T)local_buffer % SIZEOF(jrec_suffix)));
	DEBUG_ONLY(dbg_in_jnl_format = FALSE;)
	return jfb;
}
