/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "vmsdtype.h"
#include "locks.h"
#include <descrip.h>
#include <lckdef.h>
#include <psldef.h>
#include <ssdef.h>
#include <syidef.h>
#include <efndef.h>



#define DEF_NODE 0xFFFF

typedef struct
	{
		item_list_3	ilist;
		int4		terminator;
	} syistruct;


/* Entered after any failure to grant database locks in ccp_tr_opendb1;  retry */

void	ccp_tr_opendb1b( ccp_action_record *rec)
{
	ccp_action_record	request;
	ccp_db_header		*db;
	uint4		node, status, retlen;
	unsigned short		iosb[4];
	unsigned char		resnam_buffer[GLO_NAME_MAXLEN];
	struct dsc$descriptor_s	resnam;
	syistruct		syi_list;


	if ((db = rec->v.h) == NULL)
		return;

	global_name("GT$F", &FILE_INFO(db->greg)->file_id, resnam_buffer);
	resnam.dsc$a_pointer = &resnam_buffer[1];
	resnam.dsc$w_length = resnam_buffer[0];
	resnam.dsc$b_dtype = DSC$K_DTYPE_T;
	resnam.dsc$b_class = DSC$K_CLASS_S;

	if (db->flush_iosb.cond != SS$_NORMAL)
	{
		ccp_signal_cont(db->flush_iosb.cond);	/***** Is this reasonable? *****/

		if (db->flush_iosb.cond == SS$_DEADLOCK)
		{
			/* Just try again */
			status = ccp_enqw(EFN$C_ENF, LCK$K_NLMODE, &db->flush_iosb, LCK$M_SYSTEM, &resnam, 0,
					  NULL, 0, NULL, PSL$C_USER, 0);
			/***** Check error status here? *****/
		}
	}

	if (db->lock_iosb.cond != SS$_NORMAL)
	{
		ccp_signal_cont(db->lock_iosb.cond);	/***** Is this reasonable? *****/

		if (db->lock_iosb.cond == SS$_DEADLOCK)
		{
			resnam_buffer[4] = 'L';
			/* Just try again */
			status = ccp_enqw(EFN$C_ENF, LCK$K_NLMODE, &db->lock_iosb, LCK$M_SYSTEM, &resnam, 0,
					  NULL, 0, NULL, PSL$C_USER, 0);
			/***** Check error status here? *****/
		}
	}

	if (db->wm_iosb.cond != SS$_NORMAL)
	{
		ccp_signal_cont(db->wm_iosb.cond);	/***** Is this reasonable? *****/

		if (db->wm_iosb.cond == SS$_DEADLOCK)
		{
			memset(db->wm_iosb.valblk, 0, SIZEOF(db->wm_iosb.valblk));
			resnam_buffer[4] = 'S';
			/* Just try again */
			status = ccp_enqw(EFN$C_ENF, LCK$K_NLMODE, &db->wm_iosb, LCK$M_SYSTEM, &resnam, 0,
					  NULL, 0, NULL, PSL$C_USER, 0);
			/***** Check error status here? *****/
		}
	}

	if (db->wm_iosb.cond == SS$_NORMAL  &&  db->refcnt_iosb.cond != SS$_NORMAL)
		if ((db->refcnt_iosb.cond & 0xFFFE) == SS$_DEADLOCK)	/* Ignore low bit possibly set in ccp_tr_opendb1 */
		{
			if ((db->refcnt_iosb.cond & 1) == 0)		/* Only report a true deadlock */
				ccp_signal_cont(SS$_DEADLOCK);

			node = 0;
			syi_list.ilist.item_code = SYI$_NODE_CSID;
			syi_list.ilist.buffer_address = &node;
			syi_list.ilist.buffer_length = SIZEOF(node);
			syi_list.ilist.return_length_address = &retlen;
			syi_list.terminator = 0;
			status = sys$getsyiw(EFN$C_ENF, NULL, NULL, &syi_list, iosb, NULL, 0);
			if (status != SS$_NORMAL  ||  (status = iosb[0]) != SS$_NORMAL)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
			if (node == 0)
				node = DEF_NODE;

			memcpy(resnam_buffer, "GT$N_", 5);
			i2hex(node, &resnam_buffer[5], 8);

			resnam.dsc$a_pointer = resnam_buffer;
			resnam.dsc$w_length = 13;
			/* Just try again */
			status = ccp_enqw(EFN$C_ENF, LCK$K_CRMODE, &db->refcnt_iosb, LCK$M_SYSTEM, &resnam, db->wm_iosb.lockid,
					  NULL, 0, NULL, PSL$C_USER, 0);
			/***** Check error status here? *****/
		}
		else
			ccp_signal_cont(db->refcnt_iosb.cond);	/***** Is this reasonable? *****/

	if (db->flush_iosb.cond == SS$_NORMAL  &&  db->lock_iosb.cond   == SS$_NORMAL  &&
	    db->wm_iosb.cond    == SS$_NORMAL  &&  db->refcnt_iosb.cond == SS$_NORMAL)
	{
		/* Convert Write-mode lock from Null to Protected Write, reading the lock value block */
		status = ccp_enq(0, LCK$K_PWMODE, &db->wm_iosb, LCK$M_CONVERT | LCK$M_VALBLK, NULL, 0,
				 ccp_opendb2, db, NULL, PSL$C_USER, 0);
		/***** Check error status here? *****/
	}
	else
	{
		/* Try again... */
		request.action = CCTR_OPENDB1B;
		request.pid = 0;
		request.v.h = db;
		ccp_act_request(&request);
	}

	return;
}
