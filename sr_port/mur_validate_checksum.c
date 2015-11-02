/****************************************************************
 *
 *	Copyright 2005, 2011 Fidelity Information Services, Inc	*
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
#include "mur_validate_checksum.h"
#include "jnl_get_checksum.h"

boolean_t mur_validate_checksum(jnl_ctl_list *jctl)
{
	enum jnl_record_type 	rectype;
	uint4			rec_checksum;
	unsigned char		*start_ptr, *end_ptr;
	jnl_record		*jnlrec;
	reg_ctl_list		*rctl;
	mur_read_desc_t		*mur_desc;

	rec_checksum = INIT_CHECKSUM_SEED;
	rctl = jctl->reg_ctl;
	mur_desc = rctl->mur_desc;
	jnlrec = mur_desc->jnlrec;
	rectype = (enum jnl_record_type)jnlrec->prefix.jrec_type;
	if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))	/* TUPD/UUPD/FUPD/GUPD */
	{
		assert(&jnlrec->jrec_set_kill.mumps_node == &jnlrec->jrec_ztworm.ztworm_str);
		start_ptr = (unsigned char *)&jnlrec->jrec_set_kill.mumps_node;
		end_ptr =  (unsigned char *)(jnlrec) + mur_desc->jreclen - JREC_SUFFIX_SIZE;
		rec_checksum = jnl_get_checksum((uint4 *)start_ptr, NULL, (int)(end_ptr - start_ptr));
	} else if (JRT_PBLK == rectype)
	{
		start_ptr = (unsigned char *)jnlrec->jrec_pblk.blk_contents;
		rec_checksum = jnl_get_checksum((uint4 *)start_ptr, NULL, jnlrec->jrec_pblk.bsiz);
	}
	rec_checksum = ADJUST_CHECKSUM(rec_checksum, jctl->rec_offset);
	rec_checksum = ADJUST_CHECKSUM(rec_checksum, jctl->jfh->checksum);
	/* Note: rec_checksum updated inside the below macro */
	ADJUST_CHECKSUM_WITH_SEQNO(jrt_is_replicated[rectype], rec_checksum, GET_JNL_SEQNO(jnlrec));
	/* assert(jnlrec->prefix.checksum == rec_checksum); Can fail only for journal after crash or with holes */
	return (jnlrec->prefix.checksum == rec_checksum);
}
