/****************************************************************
 *
 * Copyright (c) 2005-2017 Fidelity National Information	*
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

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "min_max.h"
#include "mur_validate_checksum.h"
#include "jnl_get_checksum.h"

/* #GTM_THREAD_SAFE : The below function (mur_validate_checksum) is thread-safe */
boolean_t mur_validate_checksum(jnl_ctl_list *jctl)
{
	enum jnl_record_type 	rectype;
	uint4			orig_csum, rec_csum, tmp_csum;
	unsigned char		*start_ptr, *end_ptr;
	jnl_record		*jnlrec;
	reg_ctl_list		*rctl;
	mur_read_desc_t		*mur_desc;
	struct_jrec_align	*align_rec;

	rec_csum = INIT_CHECKSUM_SEED;
	rctl = jctl->reg_ctl;
	mur_desc = rctl->mur_desc;
	jnlrec = mur_desc->jnlrec;
	rectype = (enum jnl_record_type)jnlrec->prefix.jrec_type;
	orig_csum = GET_JREC_CHECKSUM(jnlrec, rectype);
	if (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype))	/* TUPD/UUPD/FUPD/GUPD */
	{
		COMPUTE_COMMON_CHECKSUM(tmp_csum, jnlrec->prefix);
		assert(&jnlrec->jrec_set_kill.mumps_node == &jnlrec->jrec_ztworm.ztworm_str);
		assert(&jnlrec->jrec_set_kill.mumps_node == &jnlrec->jrec_lgtrig.lgtrig_str);
		start_ptr = (unsigned char *)&jnlrec->jrec_set_kill.mumps_node;
		end_ptr =  (unsigned char *)(jnlrec) + mur_desc->jreclen - JREC_SUFFIX_SIZE;
		rec_csum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)start_ptr, (int)(end_ptr - start_ptr));
		COMPUTE_LOGICAL_REC_CHECKSUM(rec_csum, &jnlrec->jrec_set_kill, tmp_csum, rec_csum);

	} else if (JRT_PBLK == rectype || JRT_AIMG == rectype)
	{
		COMPUTE_COMMON_CHECKSUM(tmp_csum, jnlrec->prefix);
		start_ptr = (unsigned char *)jnlrec->jrec_pblk.blk_contents;
		rec_csum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)start_ptr,
								MIN(jnlrec->jrec_pblk.prefix.forwptr, jnlrec->jrec_pblk.bsiz));
		COMPUTE_PBLK_CHECKSUM(rec_csum, &jnlrec->jrec_pblk, tmp_csum, rec_csum);
	} else if (IS_FIXED_SIZE(rectype))
	{
		jnlrec->prefix.checksum = INIT_CHECKSUM_SEED;
		assert(JRT_TRIPLE != rectype);
		assert(JRT_HISTREC != rectype);
		rec_csum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)jnlrec, jnlrec->prefix.forwptr);
		jnlrec->prefix.checksum = orig_csum;
	} else if (JRT_ALIGN == rectype)
	{	/* Note: "struct_jrec_align" has a different layout (e.g. "checksum" at different offset etc.) than all
		 * other jnl records. So handle this specially.
		 */
		align_rec = (struct_jrec_align *)jnlrec;
		align_rec->checksum = INIT_CHECKSUM_SEED;
		rec_csum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)align_rec, FIXED_ALIGN_RECLEN);
		align_rec->checksum = orig_csum;
	} else
		assert(FALSE);
	ADJUST_CHECKSUM(rec_csum, jctl->rec_offset, rec_csum);
	ADJUST_CHECKSUM(rec_csum, jctl->jfh->checksum, rec_csum);
	return (orig_csum == rec_csum);
}
