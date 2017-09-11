/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"
#include "jnl_get_checksum.h"

GBLREF	boolean_t		in_jnl_file_autoswitch;

/* Success : returns 0
 * Failure : returns -1
 */
int jnl_write_extend_if_needed(int4 jrec_len, jnl_buffer_ptr_t jb, uint4 lcl_freeaddr, sgmnt_addrs *csa,
					enum jnl_record_type rectype, blk_hdr_ptr_t blk_ptr, jnl_format_buffer *jfb,
					gd_region *reg, jnl_private_control *jpc, jnl_record *jnl_rec)
{
	int4			jrec_len_padded;
	int4			blocks_needed;
	boolean_t		do_extend;

	/* Before writing a journal record, check if we have some padding space to close the journal file in case we
	 * are on the verge of an autoswitch. If we are about to autoswitch the journal file at this point, don't do
	 * the padding check since the padding space has already been checked in jnl_write calls before this autoswitch
	 * invocation. We can safely write the input record without worrying about autoswitch limit overflow.
	 */
	jrec_len_padded = jrec_len;
	if (!in_jnl_file_autoswitch)
		jrec_len_padded = jrec_len + JNL_FILE_TAIL_PRESERVE;
	blocks_needed = DISK_BLOCKS_SUM(lcl_freeaddr, jrec_len_padded);
	do_extend = jb->last_eof_written || (jb->filesize < blocks_needed);
	if (do_extend)
	{	/* not enough room in jnl file, extend it */
		/* We should never reach here if we are called from t_end/tp_tend. We check that by using the fact that
		 * early_tn is different from curr_tn in the t_end/tp_tend case. The only exception is wcs_recover which
		 * also sets these to be different in case of writing an INCTN record. For this case though it is okay to
		 * extend/autoswitch the file. So allow that.
		 */
		if (!jb->last_eof_written)
		{
			assertpro((csa->ti->early_tn == csa->ti->curr_tn) || (JRT_INCTN == rectype));
			assert(!IS_REPLICATED(rectype)); /* all replicated jnl records should have gone through t_end/tp_tend */
			assert(jrt_fixed_size[rectype]); /* this is used later in re-computing checksums */
		}
		assert(NULL == blk_ptr);	/* as otherwise it is a PBLK or AIMG record which is of variable record
						 * length that conflicts with the immediately above assert.
						 */
		assert(NULL == jfb);		/* as otherwise it is a logical record with formatted journal records which
						 * is of variable record length (conflicts with the jrt_fixed_size assert).
						 */
		assertpro(!in_jnl_file_autoswitch);	/* avoid recursion of jnl_file_extend */
		if (SS_NORMAL != jnl_flush(reg))
		{
			assert(NOJNL == jpc->channel); /* jnl file lost */
			return -1; /* let the caller handle the error */
		}
		assert(lcl_freeaddr == jb->dskaddr);
		if (EXIT_ERR == jnl_file_extend(jpc, jrec_len))
			return -1; /* let the caller handle the error */
		assert(!jb->last_eof_written);
		if (0 == jpc->pini_addr)
		{	/* This can happen only if jnl got switched in jnl_file_extend above.
			 * Write a PINI record in the new journal file and then continue writing the input record.
			 * Basically we need to redo the processing in jnl_write because a lot of the local variables
			 * have changed state (e.g. JB->freeaddr etc.). So we instead call "jnl_write"
			 * recursively and then return immediately.
			 */
			jnl_write_pini(csa);
			assertpro(jpc->pini_addr);	/* should have been set in "jnl_write_pini" */
			if (JRT_PINI != rectype)
			{
				assert(JRT_ALIGN != rectype); /* need this assert so it is safe to do "prefix.pini_addr" below */
				jnl_rec->prefix.pini_addr = jpc->pini_addr;
				/* Checksum needs to be recomputed since prefix.pini_addr is changed in above statement */
				jnl_rec->prefix.checksum = INIT_CHECKSUM_SEED;
				jnl_rec->prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)jnl_rec,
													jnl_rec->prefix.forwptr);
				jnl_write(jpc, rectype, jnl_rec, NULL);
			}
			return -1; /* let the caller handle the error */
		}
	}
	return 0;
}
