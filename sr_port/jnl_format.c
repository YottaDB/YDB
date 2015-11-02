/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "mdefsp.h"
#include "ccp.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "copy.h"
#include "jnl_get_checksum.h"

#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF	gd_region		*gv_cur_region;
GBLREF 	short  			dollar_tlevel;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	sgm_info		*sgm_info_ptr;
LITREF	int			jrt_update[];
static	const	enum jnl_record_type	jnl_opcode[3][5] =
					{
						{ JRT_KILL, JRT_FKILL, JRT_TKILL, JRT_GKILL, JRT_UKILL },
						{ JRT_SET,  JRT_FSET,  JRT_TSET,  JRT_GSET,  JRT_USET  },
						{ JRT_ZKILL, JRT_FZKILL, JRT_TZKILL, JRT_GZKILL, JRT_UZKILL}
					};

void	jnl_format(jnl_format_buffer *jfb)
{
	enum jnl_record_type	rectype;
	sgmnt_addrs		*csa;
	uint4			align_fill_size, jrec_size, tmp_jrec_size;
	int			subcode;
	jnl_action		*ja;
	char			*local_buffer, *mumps_node_ptr;
	jnl_str_len_t		keystrlen;
	mstr_len_t		valstrlen;
	GTMCRYPT_ONLY(int	fixed_length;)
	GTMCRYPT_ONLY(int	crypt_status;)

	csa = &FILE_INFO(gv_cur_region)->s_addrs;
	if (jnl_fence_ctl.level == 0 && dollar_tlevel == 0)
	{
		/* Non-TP */
		subcode = 0;
		tmp_jrec_size = FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE;
	} else
	{
		if (NULL == csa->next_fenced)
		{
			/* F (or T) */
			subcode = 1;
			csa->next_fenced = jnl_fence_ctl.fence_list;
			jnl_fence_ctl.fence_list = csa;
		} else
			/* G (or U) */
			subcode = 3;
		if (0 != dollar_tlevel)
		{
			/* TP */
			++subcode;
			tmp_jrec_size = FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE;
		} else
			tmp_jrec_size = FIXED_ZTP_UPD_RECLEN + JREC_SUFFIX_SIZE;
	}
	ja = &(jfb->ja);
	rectype = jnl_opcode[ja->operation][subcode];
	assert(rectype > JRT_BAD && rectype < JRT_RECTYPES);
	assert(IS_SET_KILL_ZKILL(rectype));
	/* Compute actual record length */
	assert(NULL != ja->key);
	keystrlen = ja->key->end;
	tmp_jrec_size += keystrlen + SIZEOF(jnl_str_len_t);
	if (JNL_SET == ja->operation)
	{
		assert(NULL != ja->val);
		valstrlen = ja->val->str.len;
		tmp_jrec_size += valstrlen + SIZEOF(mstr_len_t);
	}
	jrec_size = ROUND_UP2(tmp_jrec_size, JNL_REC_START_BNDRY);
	align_fill_size = jrec_size - tmp_jrec_size; /* For JNL_REC_START_BNDRY alignment */
	if (dollar_tlevel)
	{
		assert((1 << JFB_ELE_SIZE_IN_BITS) == JNL_REC_START_BNDRY);
		assert(JFB_ELE_SIZE == JNL_REC_START_BNDRY);
		jfb->buff = (char *)get_new_element(sgm_info_ptr->format_buff_list, jrec_size >> JFB_ELE_SIZE_IN_BITS);
		GTMCRYPT_ONLY(
			if (REPL_ALLOWED(csa))
				jfb->alt_buff = (char *)get_new_element(sgm_info_ptr->format_buff_list,
									jrec_size >> JFB_ELE_SIZE_IN_BITS);
		)
		/* assume an align record will be written while computing maximum jnl-rec size requirements */
		sgm_info_ptr->total_jnl_rec_size += (int)(jrec_size + MIN_ALIGN_RECLEN);
	}
	/* else if (0 == dollar_tlevel) jfb->buff/jfb->alt_buff already malloced in gvcst_init. */
	jfb->record_size = jrec_size;
	jfb->rectype = rectype;
	/* PREFIX */
	((jrec_prefix *)jfb->buff)->jrec_type = rectype;
	((jrec_prefix *)jfb->buff)->forwptr = jrec_size;
	if (IS_ZTP(rectype))
		local_buffer = jfb->buff + FIXED_ZTP_UPD_RECLEN;
	else
		local_buffer = jfb->buff + FIXED_UPD_RECLEN;
	mumps_node_ptr = local_buffer;
	*(jnl_str_len_t *)local_buffer = keystrlen; /* direct assignment for already aligned address */
	local_buffer += sizeof(jnl_str_len_t);
	memcpy(local_buffer, (uchar_ptr_t)ja->key->base, keystrlen);
	local_buffer += keystrlen;
	if (JNL_SET == ja->operation)
	{
		PUT_MSTR_LEN(local_buffer, valstrlen); /* SET command's data may not be aligned */
		local_buffer +=  sizeof(jnl_str_len_t);
		memcpy(local_buffer, (uchar_ptr_t)ja->val->str.addr, valstrlen);
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
#	ifdef	GTM_CRYPT
	if (csa->hdr->is_encrypted)
	{
		/* At this place we have all the components of the *SET or *KILL records filled. Before the
		 * variable part of the journal record gets encrypted, we make sure we copy the buff into
		 * alt_buff to be used in it's original form in jnl_write.*/
		if (REPL_ALLOWED(csa))
			memcpy(jfb->alt_buff, jfb->buff, jrec_size);
		/* With the fixed length computed above as FIXED_{ZTP_}UPD_RECLEN + JREC_SUFFIX_SIZE, we encode
		 * the remaining buffer which consists of the *SET or *KILL components. */
		ASSERT_ENCRYPTION_INITIALIZED;
		fixed_length = JREC_SUFFIX_SIZE + (IS_ZTP(rectype) ? FIXED_ZTP_UPD_RECLEN : FIXED_UPD_RECLEN);
		GTMCRYPT_ENCODE_FAST(csa->encr_key_handle, mumps_node_ptr, (jrec_size - fixed_length), NULL, crypt_status);
		if (0 != crypt_status)
			GC_RTS_ERROR(crypt_status, gv_cur_region->dyn.addr->fname);
	}
#	endif
	/* The below call to jnl_get_checksum makes sure that checksum computation happens after the encryption if
	 * encryption is turned on. */
	jfb->checksum = jnl_get_checksum((uint4 *)mumps_node_ptr, NULL,(int)(local_buffer - mumps_node_ptr));
	assert(0 == ((UINTPTR_T)local_buffer % sizeof(jrec_suffix)));
}
