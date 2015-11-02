/****************************************************************
 *
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
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

boolean_t mur_validate_checksum(jnl_ctl_list *jctl)
{
	enum jnl_record_type 	rectype;
	uint4			rec_csum, tmp_csum;
	unsigned char		*start_ptr, *end_ptr;
	jnl_record		*jnlrec;
	reg_ctl_list		*rctl;
	mur_read_desc_t		*mur_desc;

	rec_csum = INIT_CHECKSUM_SEED;
	rctl = jctl->reg_ctl;
	mur_desc = rctl->mur_desc;
	jnlrec = mur_desc->jnlrec;
	rectype = (enum jnl_record_type)jnlrec->prefix.jrec_type;
	if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))	/* TUPD/UUPD/FUPD/GUPD */
	{
		COMPUTE_COMMON_CHECKSUM(tmp_csum, jnlrec->prefix);
		assert(&jnlrec->jrec_set_kill.mumps_node == &jnlrec->jrec_ztworm.ztworm_str);
		start_ptr = (unsigned char *)&jnlrec->jrec_set_kill.mumps_node;
		end_ptr =  (unsigned char *)(jnlrec) + mur_desc->jreclen - JREC_SUFFIX_SIZE;
		rec_csum = jnl_get_checksum((uint4 *)start_ptr, NULL, (int)(end_ptr - start_ptr));
		COMPUTE_LOGICAL_REC_CHECKSUM(rec_csum, &jnlrec->jrec_set_kill, tmp_csum, rec_csum);

	} else if (JRT_PBLK == rectype || JRT_AIMG == rectype)
	{
		COMPUTE_COMMON_CHECKSUM(tmp_csum, jnlrec->prefix);
		start_ptr = (unsigned char *)jnlrec->jrec_pblk.blk_contents;
		rec_csum = jnl_get_checksum((uint4 *)start_ptr, NULL, MIN(jnlrec->jrec_pblk.prefix.forwptr,
										jnlrec->jrec_pblk.bsiz));
		COMPUTE_PBLK_CHECKSUM(rec_csum, &jnlrec->jrec_pblk, tmp_csum, rec_csum);
	} else if (IS_FIXED_SIZE(rectype) || rectype == JRT_ALIGN)
	{
		tmp_csum = jnlrec->prefix.checksum;
		jnlrec->prefix.checksum = INIT_CHECKSUM_SEED;
		switch (rectype)
		{
		case JRT_ALIGN:
			rec_csum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&jnlrec->jrec_align, SIZEOF(jrec_prefix));
			break;
		default:
			if (JRT_TRIPLE != rectype && JRT_HISTREC != rectype)
			rec_csum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&jnlrec->jrec_set_kill, jnlrec->prefix.forwptr);
			break;
		}
		jnlrec->prefix.checksum = tmp_csum;
	}
	ADJUST_CHECKSUM(rec_csum, jctl->rec_offset, rec_csum);
	ADJUST_CHECKSUM(rec_csum, jctl->jfh->checksum, rec_csum);
	/* assert(jnlrec->prefix.checksum == rec_csum); Can fail only for journal after crash or with holes */
	return (jnlrec->prefix.checksum == rec_csum);
}
