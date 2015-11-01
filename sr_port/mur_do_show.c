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
#include "iosp.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "copy.h"

GBLREF	mur_opt_struct	mur_options;


void 	mur_do_show(ctl_list *ctl)	/* Called from mur_back_process() */

{
	jnl_record		*rec;
	jnl_process_vector	*pv;
	show_list_type		*slp;
	enum jnl_record_type	rectype;


	rec = (jnl_record *)ctl->rab->recbuff;
	rectype = REF_CHAR(&rec->jrec_type);

	if (mur_options.show & SHOW_STATISTICS)
		++ctl->jnlrec_cnt[rectype > JRT_BAD  &&  rectype < JRT_RECTYPES ? rectype : JRT_BAD];

	if (!(mur_options.show & (SHOW_BROKEN | SHOW_ALL_PROCESSES | SHOW_ACTIVE_PROCESSES)))
		return;

	switch(rectype)
	{
	default:	/* i.e. JRT_EPOCH, JRT_PBLK, JRT_ALIGN, JRT_NULL */

		return;


	case JRT_PINI:
	case JRT_PFIN:
	case JRT_EOF:

		pv = &rec->val.jrec_pini.process_vector[CURR_JPV];

		assert(pv == &rec->val.jrec_pfin.process_vector);
		assert(pv == &rec->val.jrec_eof.process_vector);

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
	case JRT_TCOM:
	case JRT_ZTCOM:
	case JRT_ZKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:

		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_fset.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_gset.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_tset.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_uset.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_kill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_fkill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_gkill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_tkill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_ukill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_tcom.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_ztcom.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_zkill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_fzkill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_gzkill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_tzkill.pini_addr);
		assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_uzkill.pini_addr);

		if ((pv = mur_get_pini_jpv(ctl, rec->val.jrec_set.pini_addr)) == NULL)
			return;

	}


	/* See if a show_list entry exists for this process;  if so, return,
	   after recording any process initialization or termination time */

	for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
		if (slp->jpv.jpv_pid == pv->jpv_pid  &&
		    memcmp(slp->jpv.jpv_prcnam, pv->jpv_prcnam, JPV_LEN_PRCNAM) == 0)
		{
			if (rectype == JRT_PINI)
				slp->jpv.jpv_login_time = pv->jpv_time;
			else
				if (rectype == JRT_PFIN)
					slp->jpv.jpv_time = pv->jpv_time;

			return;
		}


	/* Create a show_list entry for this process */

	slp = (show_list_type *)malloc(sizeof(show_list_type));
	slp->next = ctl->show_list;
	ctl->show_list = slp;

	memcpy(&slp->jpv, pv, sizeof(jnl_process_vector));

	slp->broken = slp->recovered
		    = FALSE;

	switch(rectype)
	{
	case JRT_PINI:
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
	case JRT_TCOM:
	case JRT_ZTCOM:
	case JRT_ZKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:

		memset(&slp->jpv.jpv_time, 0, sizeof(slp->jpv.jpv_time));
	}
}
