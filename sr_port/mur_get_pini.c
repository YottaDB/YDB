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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "hashtab.h"
#if defined(VMS)
#include "iosb_disk.h"
#endif
#include "mur_read_file.h"
#include "iosp.h"
#include "copy.h"
#include "gtmmsg.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF 	mur_rab_t	mur_rab;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_read_desc_t	mur_desc;
GBLREF	mur_opt_struct  mur_options;

#define	PROCEED_IF_EXTRACT_SHOW_VERIFY(pini_addr, plst, pplst)								\
{	/* allow EXTRACT/SHOW/VERIFY to proceed after printing BAD PINI error if error_limit permits.			\
	 * the way we proceed is by returning as if pini_addr was the first journal record in the file.			\
	 * this is guaranteed to be a PINI record because of the way GT.M create journal files.				\
	 */														\
	boolean_t		proceed;										\
	GBLREF 	mur_gbls_t	murgbl;											\
	GBLREF	mur_opt_struct  mur_options;										\
															\
	proceed = MUR_WITHIN_ERROR_LIMIT(murgbl.err_cnt, mur_options.error_limit);	/* increments murgbl.err_cnt */	\
	if (!proceed || mur_options.update)	/* RECOVER/ROLLBACK should not proceed even if error_limit permits */	\
		return ERR_JNLBADRECFMT;										\
	pini_addr = JNL_FILE_FIRST_RECORD;										\
	if (NULL != (plst = (pini_list_struct *)lookup_hashtab_ent(mur_jctl->pini_list, (void *)pini_addr, &dummy)))	\
	{														\
		*pplst = plst;												\
		return SS_NORMAL;											\
	}														\
	/* at this point we have a bad PINI record in the beginning of the journal file, we probably should GTMASSERT */\
	return ERR_JNLBADRECFMT;											\
}

/* This routine reads a PINI record from a Journal file.
 * If it is not already in the hash table, it saves it in the hash table,
 * For success pplst = (pointer to the pini_list_struct structure) is updated.
 * For success it returns SS_NORMAL. Else error code is returned.
 */
uint4	mur_get_pini(off_jnl_t pini_addr, pini_list_struct **pplst)
{
	pini_list_struct	*plst;
	struct_jrec_pini	*pini_rec;
	int4			dummy;
	uint4			status;

	error_def(ERR_JNLREAD);
	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_NOPINI);

	if (NULL != (plst = (pini_list_struct *)lookup_hashtab_ent(mur_jctl->pini_list, (void *)pini_addr, &dummy)))
	{
		*pplst = plst;
		return SS_NORMAL;
	}
	mur_desc.random_buff.dskaddr = ROUND_DOWN2(pini_addr, DISK_BLOCK_SIZE);
	mur_desc.random_buff.blen = pini_addr - mur_desc.random_buff.dskaddr + PINI_RECLEN;
	if (mur_desc.random_buff.dskaddr > mur_jctl->eof_addr - mur_desc.random_buff.blen ||
		(SS_NORMAL != (status = mur_read(mur_jctl))))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset);
		gtm_putmsg(VARLSTCNT(5) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, pini_addr);
		assert(FALSE);
		murgbl.wrn_count++;
		PROCEED_IF_EXTRACT_SHOW_VERIFY(pini_addr, plst, pplst);
	}
	mur_rab.pinirec = (struct_jrec_pini *)(mur_desc.random_buff.base + (pini_addr - mur_desc.random_buff.dskaddr));
	/* Verify that it's actually a PINI record */
	if (JRT_PINI != mur_rab.pinirec->prefix.jrec_type || PINI_RECLEN != mur_rab.pinirec->prefix.forwptr ||
       		!IS_VALID_JNLREC((jnl_record *)mur_rab.pinirec, mur_jctl->jfh))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset);
		if (JRT_PINI != mur_rab.pinirec->prefix.jrec_type)
			gtm_putmsg(VARLSTCNT(5) ERR_NOPINI, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, pini_addr);
		assert(FALSE);
		murgbl.wrn_count++;
		PROCEED_IF_EXTRACT_SHOW_VERIFY(pini_addr, plst, pplst);
	}
	/* Insert it into the list */
	plst = (pini_list_struct *)get_new_element(murgbl.pini_buddy_list, 1);
	plst->pini_addr = pini_addr;
	plst->new_pini_addr = 0;
	plst->state = IGNORE_PROC;
	memcpy(&plst->jpv,     &mur_rab.pinirec->process_vector[CURR_JPV], sizeof(jnl_process_vector));
	memcpy(&plst->origjpv, &mur_rab.pinirec->process_vector[ORIG_JPV], sizeof(jnl_process_vector));
	assert(sizeof(void *) == sizeof(pini_addr));
	add_hashtab_ent(&mur_jctl->pini_list, (void *)pini_addr, (void *)plst);
	*pplst = plst;
	return SS_NORMAL;
}
