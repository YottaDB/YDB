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

#include <rms.h>
#include <iodef.h>
#include <efndef.h>


#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cce_sec_size.h"

int cce_sec_size(gds_file_id *file)
{
	uint4			status;
	int			size;
	short			iosb[4];
	sgmnt_data		sd;
	sgmnt_data_ptr_t	csd = &sd;
	char			expanded_file_name[MAX_FN_LEN];
	struct FAB		fab;
	struct NAM		nam;
	int			buckets;

	fab = cc$rms_fab;
	fab.fab$l_fop = FAB$M_NAM | FAB$M_UFO;
	fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO;
	fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
	fab.fab$l_nam = &nam;
	nam = cc$rms_nam;
	nam.nam$b_ess = MAX_FN_LEN;
	nam.nam$l_esa = expanded_file_name;
	memcpy(nam.nam$t_dvi, file->dvi,file->dvi[0] + 1);
	memcpy(nam.nam$w_fid, file->fid, SIZEOF(file->fid));
	status = sys$open(&fab, 0, 0);
	if (!(status & 1))
		return 0;
	status = sys$qiow(EFN$C_ENF,fab.fab$l_stv, IO$_READVBLK, &iosb[0], 0,0, csd, SIZEOF(sgmnt_data), 1,0,0,0);
	if (!(status & 1))
		return 0;
	buckets = getprime(sd.n_bts);
	size = (LOCK_BLOCK(csd) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(csd) + CACHE_CONTROL_SIZE(csd) + NODE_LOCAL_SPACE(csd)
			+ JNL_SHARE_SIZE(csd);
	sys$dassgn(fab.fab$l_stv);
	return size / OS_PAGELET_SIZE;
}
