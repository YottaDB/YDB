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

#include "mdef.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "iosp.h"
#include "copy.h"


/*
 *  This routine reads a Process Initialization record from a Journal file and saves
 *  it in a list, after first searching the list to avoid reading it more than once.
 *  It returns a pointer to the record's process vector field, or NULL if anything
 *  went wrong.
 */

jnl_process_vector	*mur_get_pini_jpv(ctl_list *ctl, uint4 pini_addr)
{
	jnl_record		*pini_rec;
	struct pini_list_struct	*p;
	mur_rab			pini_rab;
	uint4		status;


	/* First see if it's already in the list */
	for (p = ctl->pini_list;  p != NULL;  p = p->next)
		if (p->pini_addr == pini_addr)
			return &p->jpv;

	/* It's not;  try to read it in */
	pini_rab.pvt = ctl->rab->pvt;
	if ((status = mur_read(&pini_rab, pini_addr)) != SS_NORMAL)
	{
		mur_jnl_read_error(ctl, status, TRUE);
		return NULL;
	}

	/* Verify that it's actually a PINI record */
	pini_rec = (jnl_record *)pini_rab.recbuff;
	if (REF_CHAR(&pini_rec->jrec_type) != JRT_PINI)
		return NULL;

	/* Insert it into the list */
	p = (struct pini_list_struct *)malloc(sizeof(struct pini_list_struct));
	p->next = ctl->pini_list;
	ctl->pini_list = p;

	p->pini_addr = pini_addr;
	if (0 == pini_rec->val.jrec_pini.process_vector[SRVR_JPV].jpv_pid
			&& 0 ==  pini_rec->val.jrec_pini.process_vector[SRVR_JPV].jpv_image_count)
		memcpy(&p->jpv, &pini_rec->val.jrec_pini.process_vector[ORIG_JPV], sizeof(jnl_process_vector));
	else
		memcpy(&p->jpv, &pini_rec->val.jrec_pini.process_vector[SRVR_JPV], sizeof(jnl_process_vector));
	return &p->jpv;
}
