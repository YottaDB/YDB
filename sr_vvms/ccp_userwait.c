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

#include <iodef.h>
#include <fab.h>
#include <efndef.h>


#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"

static bool ccp_communication_timeout;
/***********************************************************************
This routine is called to wait for a clustered data base to transition
to a new state.  If the "state" parameter is non-zero, then it is
considered a bit mask, with state zero being the lsb, etc.  (All of
these masks are symbolically defined in ccp.h as CCST_MASK_nnnnn.)
In this case we will return when the state is obtained.  If the
state is zero, we will return when the ccp_cycle count is bumped,
indicating that we have transitioned into read mode.  In any event
we will return after the "next" cycle has passed, so that if we
"miss" our window, we can try again.
***********************************************************************/
bool ccp_userwait(gd_region *reg, uint4 state, int4 *timadr, unsigned short cycle)
	/* if timadr is non-zero, then the timeout quadword, else use the seg's timeout interval */
{
	int4		status;
	static void	ccp_nocomm();
	bool		timer_on;
	sgmnt_data	*seg;
	sgmnt_addrs	*csa;
	int4		*timptr;
	char		buff[(MM_BLOCK - 1) * DISK_BLOCK_SIZE];
	short		iosb[4];
	error_def(ERR_CCPNOTFND);

	csa = &((vms_gds_info *)(reg->dyn.addr->file_cntl->file_info))->s_addrs;
	seg = ((vms_gds_info *)(reg->dyn.addr->file_cntl->file_info))->s_addrs.hdr;
	timptr = timadr ? timadr : (int4 *) &seg->ccp_response_interval;
	timer_on = timptr[0] != 0 && timptr[1] != 0;
	for (; ;)
	{	ccp_communication_timeout = FALSE;
		if (timer_on)
		{
			status = sys$setimr(0, timptr, ccp_nocomm, ccp_nocomm, 0);
			if ((status & 1) == 0)
				rts_error(VARLSTCNT(1) status);
		}
		while (!CCP_SEGMENT_STATE(csa->nl, state) && csa->nl->ccp_cycle == cycle && !ccp_communication_timeout)
		{
			sys$hiber();
		}

		if (ccp_communication_timeout && !(CCP_SEGMENT_STATE(csa->nl, state) || csa->nl->ccp_cycle != cycle))
		{
			status = sys$qiow(EFN$C_ENF, ((vms_gds_info *)(reg->dyn.addr->file_cntl->file_info))->fab->fab$l_stv,
								IO$_READVBLK, &iosb[0], 0, 0, buff, SIZEOF(buff), 1, 0, 0, 0);
			if ((status & 1) && (iosb[0] & 1) && ((sgmnt_data *)buff)->freeze)
				continue;
			rts_error(VARLSTCNT(1) ERR_CCPNOTFND);
		}else
			break;
	}
	if (timer_on)
		sys$cantim(ccp_nocomm,0);
	return CCP_SEGMENT_STATE(csa->nl, state);
}

static void ccp_nocomm()
{
	ccp_communication_timeout = TRUE;
	sys$wake(0,0);
	return;
}
