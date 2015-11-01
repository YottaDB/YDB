/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "iosp.h"
#include "copy.h"
#include "buddy_list.h"
#include "mur_ext_set.h"


static	void	(* const extraction_routine[])() =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, size)	extract_rtn,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

typedef struct jnl_rec_list_s
{
	struct jnl_rec_list_s	*next;
	mur_rab			pini_rab;
	trans_num		tn;
} jnl_rec_list;

static	jnl_rec_list	*jnl_list_head, *jnl_list_tail;
static	buddy_list	*jnl_rec_buddy_list, *rab_buddy_list;

GBLREF	mur_opt_struct	mur_options;


void	mur_forward_buddy_list_init(void);

void	mur_forward_buddy_list_init(void)
{
	jnl_rec_buddy_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(jnl_rec_buddy_list, sizeof(jnl_rec_list), MUR_JNL_REC_BUDDY_LIST_INIT_ALLOC);
	rab_buddy_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(rab_buddy_list, 8, DIVIDE_ROUND_UP(MUR_RAB_BUDDY_LIST_INIT_ALLOC, 8));
}

/* Add a new item onto the list */
static	void	append_rec_to_list(ctl_list *ctl, trans_num tn)
{
	jnl_rec_list	*new;


	new = (jnl_rec_list *)get_new_element(jnl_rec_buddy_list, 1);
	new->pini_rab = *ctl->rab;
	assert(ctl->rab->reclen <= MAX_JNL_REC_SIZE);
	new->pini_rab.recbuff = (unsigned char *)get_new_element(rab_buddy_list, DIVIDE_ROUND_UP(ctl->rab->reclen, 8));
	memcpy(new->pini_rab.recbuff, ctl->rab->recbuff, ctl->rab->reclen);
	new->tn = tn;
	new->next = NULL;

	if (jnl_list_tail == NULL)
		jnl_list_head = jnl_list_tail = new;
	else
		jnl_list_tail = jnl_list_tail->next = new;
}


/* Purge the entire list */
static	void	purge_list(void)
{
	jnl_rec_list	*this, *next;

	reinitialize_list(rab_buddy_list);
	reinitialize_list(jnl_rec_buddy_list);
	jnl_list_head = jnl_list_tail = NULL;
}


/* Process and delete all items on the journal record list */
static	bool	process_list(ctl_list *ctl)
{
	jnl_rec_list	*this, *next;
	mur_rab		save;


	save = *ctl->rab;

	for (this = jnl_list_head;  this != NULL;  this = this->next)
	{
		/* okay....this is a total hack.
		   ctl->rab contains the current status of the journal buffer read.
		   In order to process a prior record, we just need to replace the
		   pointer to the current buffer with a previous one.  We leave all
		   else intact so that a future mur_read/mur_next will read the
		   next physical record. */

		*ctl->rab = this->pini_rab;

		if ((!mur_options.selection  ||  mur_do_record(ctl))  &&  !mur_forward_process(ctl))
		{
			*ctl->rab = save;
			purge_list();
			return FALSE;
		}
	}
	*ctl->rab = save;
	purge_list();
	return TRUE;
}

/*
 * Do forward journaling, eliminating operations with duplicate transaction
 * numbers.
 *
 * While doing journaling on a database, a process may be killed immediately
 * after updating (or partially updating) the journal file, but before the
 * database gets updated.  Since the transaction was never fully committed,
 * the database transaction number has not been updated, and the last journal
 * record does not reflect the actual state of the database.  The next process
 * to update the database writes a journal record with the same transaction
 * number as the previous record.  While processing the journal file, we must
 * recognize this and delete the uncommitted transaction.
 *
 * This process is fairly straightforward (queue up the journal records,
 * writing out the ones in the queue when prev_tn != curr_tn), except for
 * the following special conditions:
 *
 *	-------------------------------------------
 *	|  tn  | PBLK | PBLK | PBLK | PBLK | tn+1 |    case 1 (normal)
 *	-------------------------------------------
 *	       ^
 *
 *	-------------------------------------------
 *	|  tn  | PBLK | PBLK | PBLK | PBLK |  tn  |    case 2
 *	-------------------------------------------
 *	       ^
 *
 * PBLK records (before-image database blocks) don't have a transaction
 * number associated with them.  We may have any number of these PBLKs
 * before the next record with a transaction number, so we must queue up
 * the record prior to the first PBLK, and not update prev_tn until we
 * have seen the record following the sequence of PBLKs.  When we encounter
 * it, we do the comparison.  If prev_tn == curr_tn, we delete all the records
 * in the queue.  If prev_tn != curr_tn, we commit all the records in the
 * queue, then start the process over again, queueing up the current record.
 *
 * Similarly, although ZTCOM records do have a transaction number associated
 * with them, they do not represent a separate database update;  thus, the
 * next record following a ZTCOM that corresponds to an update may have the
 * same transaction number as the ZTCOM.  Therefore, We do not update prev_tn
 * after encountering a ZTCOM record.
 *
 * Transaction processing:
 * Each journal record has the same transaction number, so we queue them
 * up and when we reach the record following the tcommit, we check its
 * transaction number.  If it matches, we throw away the queue, otherwise
 * commit.
 *
 */


uint4	mur_forward(ctl_list *ctl)
{
	jnl_record		*rec;
	trans_num		tn, prev_tn = 0;
	uint4			status;
	jnl_process_vector	*pv;
	enum	jnl_record_type	rectype;
	uint4			pini_addr;
	void			(*extract)();

	error_def(ERR_JNLEXTR);

	do
	{
		rec = (jnl_record *)ctl->rab->recbuff;
		rectype = (REF_CHAR(&rec->jrec_type));


		if (mur_options.detail  && NULL != mur_options.extr_file_info)
		{
			jnlext1_write(ctl);

			pini_addr = 0;
			switch(rectype)
			{
			case JRT_SET:
			case JRT_FSET:
			case JRT_GSET:
			case JRT_TSET:
			case JRT_USET:
			case JRT_KILL:
			case JRT_FKILL:
			case JRT_GKILL:
			case JRT_TKILL:
			case JRT_UKILL:
			case JRT_ZKILL:
			case JRT_FZKILL:
			case JRT_GZKILL:
			case JRT_TZKILL:
			case JRT_UZKILL:
				pini_addr = rec->val.jrec_set.pini_addr;
				if (0 != pini_addr  &&  (pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
					detailed_extract_set(rec, pv->jpv_pid);
				else
					detailed_extract_set(rec, 0);
				break;

			case JRT_TCOM:
			case JRT_ZTCOM:
				pini_addr = rec->val.jrec_tcom.pini_addr;
				if (0 != pini_addr  &&  (pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
					detailed_extract_tcom(rec, pv->jpv_pid);
				else
					detailed_extract_tcom(rec, 0);
				break;

			case JRT_PBLK:
				pini_addr = rec->val.jrec_pblk.pini_addr;
				if (0 != pini_addr  &&  (pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
					mur_extract_pblk(rec, pv->jpv_pid);
				else
					mur_extract_pblk(rec, 0);
				break;

			case JRT_AIMG:
				pini_addr = rec->val.jrec_aimg.pini_addr;
				if (0 != pini_addr  &&  (pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
					mur_extract_aimg(rec, pv->jpv_pid);
				else
					mur_extract_aimg(rec, 0);
				break;

			case JRT_EPOCH:
				pini_addr = rec->val.jrec_epoch.pini_addr;
				if (0 == pini_addr  ||  (pv = mur_get_pini_jpv(ctl, pini_addr)) == NULL)
					pv = &ctl->rab->pvt->jfh->who_created;/* Make pv to be the one which created the journal
										 file as first creation will have pini_addr zero */
					mur_extract_epoch(rec, pv->jpv_pid);
				break;

			case JRT_INCTN:
				pini_addr = rec->val.jrec_inctn.pini_addr;
				if (0 != pini_addr  &&  (pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
					mur_extract_inctn(rec, pv->jpv_pid);
				else
					mur_extract_inctn(rec, 0);
				break;
			case JRT_NULL:
			case JRT_ALIGN:
			case JRT_EOF:
			case JRT_PINI:
			case JRT_PFIN:
				extract = extraction_routine[rec->jrec_type];
				(*extract)(rec);
				break;

			case JRT_BAD:
				assert(FALSE);
				break;
			}
		}

		/* Get the transaction # */

		switch(rectype)
		{
		case JRT_ALIGN:
		case JRT_PBLK:

			/* Ignore these;  don't update prev_tn */
			continue;


		case JRT_PINI:

			tn = 0;

			break;


		case JRT_PFIN:

			tn = rec->val.jrec_pfin.tn;

			break;


		case JRT_EOF:
			tn = rec->val.jrec_eof.tn;
			break;

		case JRT_NULL:
			tn = rec->val.jrec_null.tn;
			break;

		case JRT_SET:
		case JRT_FSET:
		case JRT_GSET:
		case JRT_TSET:
		case JRT_USET:
		case JRT_KILL:
		case JRT_FKILL:
		case JRT_GKILL:
		case JRT_TKILL:
		case JRT_UKILL:
		case JRT_EPOCH:
		case JRT_TCOM:
		case JRT_ZTCOM:
		case JRT_ZKILL:
		case JRT_FZKILL:
		case JRT_GZKILL:
		case JRT_TZKILL:
		case JRT_UZKILL:
		case JRT_INCTN:
		case JRT_AIMG:
			tn = rec->val.jrec_set.tn;

			assert(tn == rec->val.jrec_fset.tn);
			assert(tn == rec->val.jrec_gset.tn);
			assert(tn == rec->val.jrec_tset.tn);
			assert(tn == rec->val.jrec_uset.tn);
			assert(tn == rec->val.jrec_kill.tn);
			assert(tn == rec->val.jrec_fkill.tn);
			assert(tn == rec->val.jrec_gkill.tn);
			assert(tn == rec->val.jrec_tkill.tn);
			assert(tn == rec->val.jrec_ukill.tn);
			assert(tn == rec->val.jrec_epoch.tn);
			assert(tn == rec->val.jrec_tcom.tn);
			assert(tn == rec->val.jrec_ztcom.tn);
			assert(tn == rec->val.jrec_zkill.tn);
			assert(tn == rec->val.jrec_fzkill.tn);
			assert(tn == rec->val.jrec_gzkill.tn);
			assert(tn == rec->val.jrec_tzkill.tn);
			assert(tn == rec->val.jrec_uzkill.tn);
			assert(tn == rec->val.jrec_inctn.tn);
			assert(tn == rec->val.jrec_aimg.tn);
		}


		/* Check for duplicate tn's */

		switch(rectype)
		{
		case JRT_PINI:
		case JRT_SET:
		case JRT_FSET:
		case JRT_GSET:
		case JRT_TSET:		/*** What about nested transactions? ***/
		case JRT_KILL:
		case JRT_FKILL:
		case JRT_GKILL:
		case JRT_TKILL:		/*** What about nested transactions? ***/
		case JRT_NULL:
		case JRT_ZKILL:
		case JRT_FZKILL:
		case JRT_GZKILL:
		case JRT_TZKILL:		/*** What about nested transactions? ***/
		case JRT_INCTN:
		case JRT_AIMG:	/* Two after image cannot have same tn */

			/* Individual operations that cannot have duplicate tn's */

			if (tn == prev_tn)
				purge_list();
			else
				if (!process_list(ctl))
					return SS_NORMAL;

			append_rec_to_list(ctl, tn);

			break;


		case JRT_USET:
		case JRT_UKILL:
		case JRT_UZKILL:
		case JRT_TCOM:
		case JRT_ZTCOM:

			append_rec_to_list(ctl, tn);

			/* Don't update prev_tn */
			continue;


		case JRT_EPOCH:

			/* Determine the status of any previous operations and immediately
			   process an EPOCH request.  Duplicate EPOCHs are okay */

			if (tn == prev_tn)
				purge_list();
			else
				if (!process_list(ctl))
					return SS_NORMAL;
			break;


		case JRT_EOF:
		case JRT_PFIN:

			/* Process termination;  flush list */

			append_rec_to_list(ctl, tn);

			if (!process_list(ctl))
				return SS_NORMAL;

		}

		prev_tn = tn;

	} while ((status = mur_next(ctl->rab, 0)) == SS_NORMAL);


	/* Perform any transactions previously queued up */
	process_list(ctl);

	return status;
}


