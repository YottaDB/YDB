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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "send_msg.h"
#include "sleep_cnt.h"
#include "dpgbldir.h"
#include "wcs_sleep.h"

void	finish_active_jnl_qio(void)
{
	gd_addr			*addr_ptr;
	gd_region		*reg, *r_top;
	int4			lcnt;
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;

	error_def(ERR_JNLFLUSH);

	for (addr_ptr = get_next_gdr(NULL);  addr_ptr;  addr_ptr = get_next_gdr(addr_ptr))
	{
		for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions;  reg < r_top;  reg++)
		{
			if (reg->open && !reg->was_open && (NULL != (csa = &FILE_INFO(reg)->s_addrs)) && (NULL != csa->hdr)
		        	&& (JNL_ENABLED(csa->hdr) && (NULL != (jpc = csa->jnl)) && (NULL != jpc->jnl_buff)))
			{
				for (lcnt = 1;  (FALSE != jpc->qio_active) && (0 == jpc->jnl_buff->iosb.cond);  lcnt++)
				{
					if (lcnt <= JNL_MAX_FLUSH_TRIES)
		                        	wcs_sleep(lcnt);
					else
			                {
						jnl_send_oper(jpc, ERR_JNLFLUSH);
					        assert(FALSE);
						break;
					}
				}
			}
		}
	}
}
