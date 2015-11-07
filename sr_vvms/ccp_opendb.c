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
#include <fab.h>
#include <nam.h>

void ccp_opendb( ccp_action_record *rec)
{
	ccp_db_header *db;
	struct NAM *ccp_nam;
	struct FAB *ccp_fab;
	uint4 status;
	int size;

	db = malloc(SIZEOF(*db));
	memset(db, 0, SIZEOF(*db));
	db->write_wait.last = &db->write_wait.first;
	db->flu_wait.last = &db->flu_wait.first;
	db->exitwm_wait.last = &db->exitwm_wait.first;
	db->reopen_wait.last = &db->reopen_wait.first;
	ccp_pndg_proc_add(&db->write_wait, rec->pid);
	db->greg = malloc(SIZEOF(*db->greg));
	memset(db->greg, 0, SIZEOF(*db->greg));
	db->greg->dyn.addr = malloc(SIZEOF(gd_segment));
	memset(db->greg->dyn.addr, 0, SIZEOF(gd_segment));
	FILE_CNTL_INIT(db->greg->dyn.addr);
	ccp_fab = ((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->fab = malloc(SIZEOF(struct FAB));
	*ccp_fab = cc$rms_fab;
	ccp_nam = malloc(SIZEOF(*ccp_nam));
	*ccp_nam = cc$rms_nam;
	ccp_fab->fab$l_nam = ccp_nam;
	db->greg->dyn.addr->acc_meth = dba_bg;
	((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id = rec->v.file_id;
	db->segment = &((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->s_addrs;
/* Malloc a temporary local node struct so can mark the state, necessary for processing of CCP_TR_WRITEDB from other processes */
	db->segment->nl = malloc(SIZEOF(node_local));
	db->segment->nl->ccp_state = CCST_OPNREQ;
	db->segment->now_crit = 0;
	ccp_add_reg(db);
	memcpy(ccp_nam->nam$t_dvi,rec->v.file_id.dvi,SIZEOF(rec->v.file_id.dvi));
	memcpy(ccp_nam->nam$w_did,rec->v.file_id.did, SIZEOF(rec->v.file_id.did));
	memcpy(ccp_nam->nam$w_fid,rec->v.file_id.fid, SIZEOF(rec->v.file_id.fid));
	ccp_fab->fab$l_fop = (FAB$M_UFO | FAB$M_NAM | FAB$M_CBT);
	ccp_fab->fab$b_fac = (FAB$M_PUT | FAB$M_GET | FAB$M_BIO);
	ccp_fab->fab$b_rtv = WINDOW_ALL;
	ccp_fab->fab$b_shr = (FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI);
	db->extra_tick_id = db->tick_timer_id = db->quantum_timer_id = db->exitwm_timer_id =
		db->wmcrit_timer_id = db->close_timer_id = db;
	sys$open(ccp_fab, ccp_opendb1e, ccp_opendb1a);
	ccp_quemin_adjust(CCP_OPEN_REGION);
	return;
}
