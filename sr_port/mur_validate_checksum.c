/****************************************************************
 *
 *	Copyright 2005, 2007 Fidelity Information Services, Inc	*
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

GBLREF 	mur_rab_t	mur_rab;
GBLREF	jnl_ctl_list	*mur_jctl;
LITREF	int		jrt_update[JRT_RECTYPES];

boolean_t mur_validate_checksum(void)
{
	enum jnl_record_type 	rectype;
	uint4			rec_checksum;
	unsigned char		*start_ptr, *end_ptr;

	rec_checksum = INIT_CHECKSUM_SEED;
	rectype = (enum jnl_record_type)mur_rab.jnlrec->prefix.jrec_type;
	if (IS_SET_KILL_ZKILL(rectype))	/* TUPD/UUPD/FUPD/GUPD */
	{
		start_ptr = (IS_ZTP(rectype)) ? (unsigned char *)&mur_rab.jnlrec->jrec_fkill.mumps_node :
				 		 (unsigned char *)&mur_rab.jnlrec->jrec_kill.mumps_node;
		end_ptr =  (unsigned char *)(mur_rab.jnlrec) + mur_rab.jreclen - JREC_SUFFIX_SIZE;
		rec_checksum = jnl_get_checksum((uint4 *)start_ptr, (int)(end_ptr - start_ptr));
	} else if (JRT_PBLK == rectype)
	{
		start_ptr = (unsigned char *)mur_rab.jnlrec->jrec_pblk.blk_contents;
		rec_checksum = jnl_get_checksum((uint4 *)start_ptr, mur_rab.jnlrec->jrec_pblk.bsiz);
	}
	rec_checksum = ADJUST_CHECKSUM(rec_checksum, mur_jctl->rec_offset);
	rec_checksum = ADJUST_CHECKSUM(rec_checksum, mur_jctl->jfh->checksum);
	/*assert(mur_rab.jnlrec->prefix.checksum == rec_checksum); Can fail only for journal after crash or with holes */
	return (mur_rab.jnlrec->prefix.checksum == rec_checksum);
}
