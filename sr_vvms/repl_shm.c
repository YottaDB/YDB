/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"

#include <stddef.h>
#include <lkidef.h>
#include <ssdef.h>
#include <clidef.h>
#include <iodef.h>
#include <prtdef.h>
#include <prvdef.h>
#include <secdef.h>
#include <psldef.h>
#include <syidef.h>
#include <descrip.h>
#include <lckdef.h>
#include <efndef.h>

#include "gtm_string.h"

#include "vmsdtype.h"
#include "gdsroot.h"
#include "repl_sem.h"
#include "repl_shm.h"
#include "repl_sp.h"

#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "locks.h"
#include "mem_list.h"
#include "init_sec.h"

#define MAILBOX_SIZE 512

error_def(ERR_RECVPOOLSETUP);
error_def(ERR_JNLPOOLSETUP);
error_def(ERR_TEXT);
error_def(ERR_REPLWARN);

static uint4 header_size[2]	= {JNLDATA_BASE_OFF, RECVDATA_BASE_OFF};
static uint4 size_offset[2]	= {offsetof(jnlpool_ctl_struct, jnlpool_size), offsetof(recvpool_ctl_struct, recvpool_size)};
static uint4 ERR_POOLSETUP[2]	= {ERR_JNLPOOLSETUP, ERR_RECVPOOLSETUP};

static uint4 get_pagelet_count(boolean_t src_or_rcv, char *gsec_name, int4 *pgcnt)
{
	sm_uc_ptr_t	shm_range[2];
	uint4		hdr_pglets;
	uint4		status, *psize, size;
	$DESCR(d_gsec, gsec_name);

	/* Map initial pages required to get the header*/
	hdr_pglets = DIVIDE_ROUND_UP(header_size[src_or_rcv], OS_PAGELET_SIZE);
        if (SS$_NORMAL != (status = map_shm_aux(src_or_rcv, &d_gsec, hdr_pglets, shm_range)))
		/* Global section doesn't exist. Just return. Caller has to handle */
		return status;

	/* Get the size from the header */
	psize		= (uint4 *) (shm_range[0]+size_offset[src_or_rcv]);
	size		= header_size[src_or_rcv] + (*psize);
	*pgcnt		= DIVIDE_ROUND_UP(size, OS_PAGELET_SIZE);

	/* Unmap */
	detach_shm(shm_range);

	return SS$_NORMAL;
}

static int4 map_shm_aux(boolean_t src_or_rcv, struct dsc$descriptor_s *name_dsc, int4 buff_pagelets, sm_uc_ptr_t *shm_range)
{
	int4		status;
	uint4		flags;
	sm_uc_ptr_t	inadr[2];

	/* Expand virtual address space */
        status = gtm_expreg(buff_pagelets, inadr, PSL$C_USER, 0);
        if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_POOLSETUP[src_or_rcv], 0,
				       ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to expand virtual address space"), status);

	/* Optional gaurding of the shared space - to be done here, if needed.(gvcst_init_sysops.c is a sample)*/

	/* map to the global section */
        flags = SEC$M_SYSGBL | SEC$M_WRT;
        if (SS$_NORMAL != (status = sys$mgblsc(inadr, shm_range, PSL$C_USER, flags, name_dsc, NULL, 0)))
		detach_shm(inadr);
	return status;
}

int4 create_and_map_shm(boolean_t src_or_rcv, struct dsc$descriptor_s *name_dsc, int4 buffsize, sm_uc_ptr_t *shm_range)
{
	int4	buff_pagelets, status;
	uint4	flags;
	/* uint4	inadr[2]; temp for init_sec */

	buff_pagelets = DIVIDE_ROUND_UP(buffsize, OS_PAGELET_SIZE);
	/* If jnl-pool-size has to be in multiple of pages, uncomment the next line */
	/* buff_pagelets = ROUND_UP(buff_pagelets, OS_PAGE_SIZE / OS_PAGELET_SIZE); */

	/* Expand virtual address space */
        status = gtm_expreg(buff_pagelets, shm_range, PSL$C_USER, 0);
        if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_POOLSETUP[src_or_rcv], 0,
				       ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to expand virtual address space"), status);

	/* Optional gaurding of the shared space - to be done here, if needed.(gvcst_init_sysops.c is a sample)*/

	/* Create if not already existing, and map the global section */
        flags = SEC$M_GBL | SEC$M_SYSGBL | SEC$M_WRT | SEC$M_PAGFIL | SEC$M_PERM;
        status = init_sec(shm_range, name_dsc, 0, buff_pagelets, flags);

        if ((SS$_NORMAL != status) && (SS$_CREATED != status))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_POOLSETUP[src_or_rcv], 0,
				       ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to Create/Map global section"), status);
	return status;
}

int4 map_shm(boolean_t src_or_rcv, struct dsc$descriptor_s *name_dsc, sm_uc_ptr_t *shm_range)
{
	int4		buff_pagelets, status;
	uint4		flags;
	sm_uc_ptr_t	inadr[2];

	status = get_pagelet_count(src_or_rcv, name_dsc->dsc$a_pointer, &buff_pagelets);
	if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_POOLSETUP[src_or_rcv], 0,
				       ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get the number of gblsection pagelets"), status);

	/* If jnl-pool-size has to be in multiple of pages, uncomment the next line */
	/* buff_pagelets = ROUND_UP(buff_pagelets, OS_PAGE_SIZE / OS_PAGELET_SIZE); */

	return (map_shm_aux(src_or_rcv, name_dsc, buff_pagelets, shm_range));
}

boolean_t shm_exists(boolean_t src_or_rcv, struct dsc$descriptor_s *name_dsc)
{
	int4		buff_pagelets, status;
	uint4		flags;
	sm_uc_ptr_t	inadr[2], shm_range[2];
	boolean_t	res;

	buff_pagelets = 1;
	status = gtm_expreg(buff_pagelets, inadr, PSL$C_USER, 0);
	if (SS$_NORMAL != status)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to expand virtual address space"), status);
		return FALSE;
	}

	/* map to the global section */
	flags = SEC$M_SYSGBL | SEC$M_WRT;
	res      = (SS$_NORMAL == sys$mgblsc(inadr, shm_range, PSL$C_USER, flags, name_dsc, NULL, 0));
	status   = gtm_deltva(inadr, NULL, PSL$C_USER);
	if (SS$_NORMAL != status)
		if (SOURCE == src_or_rcv)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLWARN, 2,
					RTS_ERROR_LITERAL("Could not detach from journal pool"), status);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLWARN, 2,
					 RTS_ERROR_LITERAL("Could not detach from receiver pool"), status);
	return res;
}

int4	register_with_gsec(struct dsc$descriptor_s *name_dsc, int4 *lockid)
{
	int4		status;
	vms_lock_sb	lksb;

	status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, &lksb, LCK$M_SYSTEM | LCK$M_EXPEDITE, name_dsc, 0, NULL, 0, NULL, PSL$C_USER, 0);
	assert(SS$_NORMAL == status);
	if (SS$_NORMAL == status)
	{
		status = gtm_enqw(EFN$C_ENF, LCK$K_CRMODE, &lksb, LCK$M_CONVERT, name_dsc, 0, NULL, 0, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		if (SS$_NORMAL == status)
			status = lksb.cond;
		assert(SS$_NORMAL == status);
	}
	if (SS$_NORMAL == status)
		*lockid = lksb.lockid;
	return status;
}

int4	lastuser_of_gsec(int4 gsec_lockid)
{
	int4		status;
	vms_lock_sb	lksb;

	lksb.lockid = gsec_lockid;
	status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, &lksb, LCK$M_CONVERT | LCK$M_NOQUEUE, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
	assert(SS$_NORMAL == status || SS$_NOTQUEUED == status);
	if (SS$_NORMAL == status)
		status = lksb.cond;
	assert(SS$_NORMAL == status || SS$_NOTQUEUED == status);
	return status;
}

uint4	signoff_from_gsec_dbg(unsigned int gsec_lockid)
{
	uint4	status;

	status = gtm_deq(gsec_lockid, NULL, PSL$C_USER, 0);
	assert(SS$_NORMAL == status);
	return status;
}
