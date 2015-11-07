/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "lv_val.h"
#include "error.h"
#include "op.h"
#include "io.h"
#include "fgncal.h"

GBLREF lv_val           *active_lv;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF int		mumps_status;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(fgncal_ch)
{
        int4            status;

        START_CH;
        if (DUMP)
        {
                gtm_dump();
                NEXTCH;
        }
        MDB_START;
        if (active_lv)
        {
		if (!LV_IS_VAL_DEFINED(active_lv) && !LV_HAS_CHILD(active_lv))
                        op_kill(active_lv);
                active_lv = (lv_val *)0;
        }
        set_zstatus(NULL, sig, NULL, FALSE);
        if (SEVERITY == SEVERE)
                EXIT(SIGNAL);
        fgncal_unwind();
        mumps_status = SIGNAL;
        mch->CHF_MCH_SAVR0 = mumps_status;
        UNWIND(NULL, NULL);
}
