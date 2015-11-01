/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* 	insert_region.c
 *
 *	requirement
 *		reg		initialized (gvcst_init)
 *	parameters
 *		reg		the region to be inserted
 *		reg_list	pointer to the pointer to the list head
 *		reg_free_list	pointer to the pointer to the free list
 *		size		size of the structure of each item in the list
 *	return
 *		pointer to the item in the list that is corresponding to the region.
 *		*reg_list and *reg_free_list are also updated if needed.
 */

#include "mdef.h"

#ifdef	VMS
#include <rms>
#endif

#ifdef UNIX
#include "gtm_ipc.h"		/* needed for FTOK */
#endif

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "iosp.h"
#include "dbfilop.h"
#include "gtmmsg.h"
#include "is_file_identical.h"

GBLREF	tp_region	*rlist;
GBLREF	gd_region	*gv_cur_region;

tp_region	*insert_region(	gd_region	*reg,
		   		tp_region	**reg_list,
		   		tp_region	**reg_free_list,
		   		int4		size)
{
	tp_region	*tr, *tr_last, *tr_new;
	unique_file_id	local_id;
#ifdef 	VMS
	char		*local_id_fiptr;
	file_control	*fc;
	uint4		status;
	gd_region	*temp_reg;
#endif

	assert(size >= sizeof(tp_region));
#if defined(VMS)
	local_id_fiptr = &local_id.file_id[0];
	temp_reg = gv_cur_region;
	gv_cur_region = reg;
	if (TRUE != reg->open)
	{
		if (NULL == mupfndfil(reg))
		{
			gv_cur_region = temp_reg;
			return NULL;
		}
	  	if (NULL == reg->dyn.addr->file_cntl)
	    	{
	      		reg->dyn.addr->file_cntl = malloc(sizeof(file_control));
              		memset(reg->dyn.addr->file_cntl, 0, sizeof(file_control));
	    	}
	  	if (NULL == reg->dyn.addr->file_cntl->file_info)
	    	{
	      		reg->dyn.addr->file_cntl->file_info = malloc(sizeof(vms_gds_info));
	      		memset(reg->dyn.addr->file_cntl->file_info, 0, sizeof(vms_gds_info));
	    	}
	  	fc = reg->dyn.addr->file_cntl;
		fc->file_type = reg->dyn.addr->acc_meth;
	  	fc->op = FC_OPEN;
	  	status = dbfilop(fc);
	  	if (status & 1)
	    	{
			local_id_fiptr = &(FILE_INFO(reg)->file_id);
	      		sys$dassgn(FILE_INFO(reg)->fab->fab$l_stv);
	    	}
	  	else
	    	{
	      		gtm_putmsg(VARLSTCNT(1) status);
			gv_cur_region = temp_reg;
	      		return NULL;
	    	}
	}
	else
		local_id_fiptr = &((&FILE_INFO(reg)->s_addrs)->nl->unique_id.file_id);
	gv_cur_region = temp_reg;
#elif defined(UNIX)
	if (!reg->open)
	{
		if (NULL == mupfndfil(reg))
			return NULL;
		if (!filename_to_id(&local_id.uid, (char *)reg->dyn.addr->fname))
			return NULL;
	} else
		local_id.uid = (&FILE_INFO(reg)->s_addrs)->nl->unique_id.uid;
#endif
	/* See if the region is already on the list or if we have to add it */
	for (tr = *reg_list, tr_last = NULL; NULL != tr; tr = tr->fPtr)
	{
		if (reg == tr->reg)			/* Region is found */
			return tr;
		VMS_ONLY(if (0 < memcmp(&(tr->file_id), local_id_fiptr, sizeof(gd_id))))
		UNIX_ONLY(if (0 < gdid_cmp(&(tr->file_id), &(local_id.uid))))
			break;				/* .. we have found our insertion point */
		tr_last = tr;
	}
	if ((NULL != reg_free_list) && (NULL != *reg_free_list))	/* Get a used block off our unused queue */
	{
		tr_new = *reg_free_list;		/* Get element */
		*reg_free_list = tr_new->fPtr;		/* Remove from queue */
	}
	else						/* get a new one */
	{
		tr_new = (tp_region *)malloc(size);
		if (size > sizeof(tp_region))
			memset(tr_new, 0, size);
	}
	tr_new->reg = reg;				/* Add this region to end of list */
	VMS_ONLY(memcpy(&(tr_new->file_id), local_id_fiptr, sizeof(gd_id));)
	UNIX_ONLY(tr_new->file_id = local_id.uid;)
	if (NULL == tr_last)
	{	/* First element on the list */
		tr_new->fPtr = *reg_list;
		*reg_list = tr_new;
	} else
	{	/* Insert into list */
		tr_new->fPtr = tr_last->fPtr;
		tr_last->fPtr = tr_new;
	}
	return tr_new;
}
