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
#include "util.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF	int4		mur_error_count;


bool	mur_report_error(ctl_list *ctl, enum mur_error code)
{
	jnl_record 		*rec;
	jnl_process_vector	*pv;
	show_list_type		*slp;
	uint4		pid, pini_addr;


	switch (code)
	{
	default:
		assert(FALSE);
		break;


#if 0	/* NOTE: currently unused */
	case MUR_JUSTPINI:

		rec = (jnl_record *)ctl->rab->recbuff;
		GET_LONG(pid, &rec->val.jrec_pini.process_vector.jpv_pid);

		util_out_print("Process !UL has only a process initialization record in journal file !AD", TRUE,
				pid, ctl->jnl_fn_len, ctl->jnl_fn);
		break;
#endif


	case MUR_NOPINI:

#if 0	/* NOTE: currently not implemented */
		if (REF_CHAR(&rec->jrec_type) == JRT_PFIN)
		{
			rec = (jnl_record *)ctl->rab->recbuff;
			GET_LONG(pid, &rec->val.jrec_pfin.process_vector.jpv_pid);

			util_out_print("Process !UL had no process initialization record in journal file !AD at file location !UL",
					TRUE, pid, ctl->jnl_fn_len, ctl->jnl_fn, ctl->rab->dskaddr + DISK_BLOCK_SIZE);
		}
		else
#endif
			util_out_print(
				"An unknown process had no process initialization record in journal file !AD at file location !UL",
					TRUE, ctl->jnl_fn_len, ctl->jnl_fn, ctl->rab->dskaddr + DISK_BLOCK_SIZE);

		++mur_error_count;

		break;


#if 0	/* NOTE: currently unused */
	case MUR_NOPFIN:

		rec = (jnl_record *)ctl->rab->recbuff;

		if (REF_CHAR(&rec->jrec_type) == JRT_ZTCOM)
		{
			GET_LONG(pini_addr, &rec->val.jrec_ztcom.pini_addr);

			if ((pv = mur_get_pini_jpv(ctl, pini_addr)) == NULL)
				util_out_print("ZTCOMMIT record found for unknown process in journal file !AD at file location !UL",
						TRUE, ctl->jnl_fn_len, ctl->jnl_fn, ctl->rab->dskaddr + DISK_BLOCK_SIZE);
			else
				util_out_print("Process !UL did not have a process termination record in journal file !AD", TRUE,
						pv->jpv_pid, ctl->jnl_fn_len, ctl->jnl_fn);
		}
		else
		{
			assert(REF_CHAR(&rec->jrec_type) == JRT_PINI);
			GET_LONG(pid, &rec->val.jrec_pini.process_vector.jpv_pid);

			util_out_print("Second process initialization record found for process !UL at file location !UL",
					TRUE, pid, ctl->rab->dskaddr + DISK_BLOCK_SIZE);
			util_out_print(" without having found a process termination record in journal file !AD", TRUE,
					ctl->jnl_fn_len, ctl->jnl_fn);

			++mur_error_count;
		}

		break;
#endif


	case MUR_BRKTRANS:

		rec = (jnl_record *)ctl->rab->recbuff;

		switch(REF_CHAR(&rec->jrec_type))
		{
		default:

			assert(FALSE);
			return TRUE;


#if 0	/* NOTE: currently not implemented */
		case JRT_PFIN:

			pv = &rec->val.jrec_pfin.process_vector;
			GET_LONG(pid, &pv->jpv_pid);

			util_out_print("Broken fence found for process !UL in journal file !AD", TRUE,
					pid, ctl->jnl_fn_len, ctl->jnl_fn);

			if (mur_options.show & SHOW_BROKEN)
			{
				for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
					if (slp->jpv.jpv_pid == pid  &&
					    memcmp(slp->jpv.jpv_user, pv->jpv_user, JPV_LEN_USER) == 0)
						break;

				if (slp == NULL)
				{
					slp = (show_list_type *)malloc(sizeof(show_list_type));
					slp->next = ctl->show_list;
					ctl->show_list = slp;

					memcpy(&slp->jpv, pv, sizeof(jnl_process_vector));
					slp->recovered = FALSE;
				}

				slp->broken = TRUE;
			}

			break;
#endif


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

			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_fset.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_gset.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_tset.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_uset.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_kill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_fkill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_gkill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_tkill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_ukill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_zkill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_fzkill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_gzkill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_tzkill.pini_addr);
			assert(&rec->val.jrec_set.pini_addr == &rec->val.jrec_uzkill.pini_addr);

			GET_LONG(pini_addr, &rec->val.jrec_set.pini_addr);

			if ((pv = mur_get_pini_jpv(ctl, pini_addr)) == NULL)
				util_out_print("Broken fence for unknown process found in journal file !AD", TRUE,
						ctl->jnl_fn_len, ctl->jnl_fn);
			else
			{
				util_out_print("Broken fence for user !AD (process !UL)", TRUE,
						real_len(JPV_LEN_USER, pv->jpv_user), pv->jpv_user, pv->jpv_pid);
				util_out_print("  found in journal file !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);

				if (mur_options.show & SHOW_BROKEN)
				{
					for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
						if (slp->jpv.jpv_pid == pv->jpv_pid  &&
						    memcmp(slp->jpv.jpv_user, pv->jpv_user, JPV_LEN_USER) == 0)
							break;

					if (slp == NULL)
					{
						slp = (show_list_type *)malloc(sizeof(show_list_type));
						slp->next = ctl->show_list;
						ctl->show_list = slp;

						memcpy(&slp->jpv, pv, sizeof(jnl_process_vector));
						memset(&slp->jpv.jpv_time, 0, sizeof slp->jpv.jpv_time);
						slp->recovered = FALSE;
					}

					slp->broken = TRUE;
				}
			}
		}

		break;


	case MUR_INSUFLOOK:

		util_out_print("Insufficient LOOKBACK_LIMIT in which to resolve broken fence(s)", TRUE);
		util_out_print("  in journal file !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);

		++mur_error_count;

		break;

	case MUR_MISSING_EXTRACT:

		util_out_print("!/*** WARNING:  Missing journal files prevent full extract ***!/", TRUE);
		break;

	case MUR_MISSING_FILES:

		util_out_print("!/*** ERROR:  Missing journal files no recovery done***!/", TRUE);
		++mur_error_count;
		break;

	case MUR_MISSING_PREVLINK:

		util_out_print("Rollback found previous journal link of journal file !AD to be NULL", TRUE,
											ctl->jnl_fn_len, ctl->jnl_fn);
		++mur_error_count;
		break;

	case MUR_MULTEOF:

		util_out_print("Misplaced EOF record found in journal file !AD at file location !UL", TRUE,
				ctl->jnl_fn_len, ctl->jnl_fn, ctl->rab->dskaddr + DISK_BLOCK_SIZE);

		++mur_error_count;
		ctl->bypass = TRUE;

		break;


	case MUR_TNCHECK:

		util_out_print("Transaction numbers !XL for journal file !AD", TRUE,
				ctl->jnl_tn, ctl->jnl_fn_len, ctl->jnl_fn);
		util_out_print("  and !XL for database file !AD do not line up;  recovery not being done", TRUE,
				ctl->db_tn, DB_LEN_STR(ctl->gd));

		++mur_error_count;

		break;


	case MUR_UNFENCE:

		rec = (jnl_record *)ctl->rab->recbuff;

		assert(&rec->val.jrec_kill.pini_addr == &rec->val.jrec_set.pini_addr);
		assert(&rec->val.jrec_kill.pini_addr == &rec->val.jrec_zkill.pini_addr);

		GET_LONG(pini_addr, &rec->val.jrec_kill.pini_addr);

		if ((pv = mur_get_pini_jpv(ctl, pini_addr)) == NULL)
			util_out_print("Unfenced transaction from unknown process found in journal file !AD at file location !UL",
					TRUE, ctl->jnl_fn_len, ctl->jnl_fn, ctl->rab->dskaddr + DISK_BLOCK_SIZE);
		else
			util_out_print(
				"Unfenced transaction from user !AD (process !UL) found in journal file !AD at file location !UL",
					TRUE, real_len(JPV_LEN_USER, pv->jpv_user), pv->jpv_user, pv->jpv_pid,
					ctl->jnl_fn_len, ctl->jnl_fn, ctl->rab->dskaddr + DISK_BLOCK_SIZE);

		++mur_error_count;

		break;


	case MUR_UNKNOWN:

		util_out_print("Unknown record type found in journal file !AD at file location !UL", TRUE,
				ctl->jnl_fn_len, ctl->jnl_fn, ctl->rab->dskaddr + DISK_BLOCK_SIZE);

		++mur_error_count;
		ctl->bypass = TRUE;

	}

	return mur_error_count <= mur_options.error_limit  ||  mur_options.interactive  &&  mur_interactive();

}
