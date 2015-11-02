/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

 /*
 * -------------------------------------------------------
 * This routine cleans up the child process prior to the exec
 * -------------------------------------------------------
 */
#include "mdef.h"

#include "gtm_fcntl.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "dpgbldir.h"
#include "jnl.h"
#include "jobsp.h"
#include "gtmio.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF	int			mutex_sock_fd;

void ojchildioclean(void)
{
	int			rc;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gd_region		*r_top, *r_local;
	gd_addr			*addr_ptr;

	/* Close any encryption related fds that the plug-in might have opened */
	GTMCRYPT_ONLY(GTMCRYPT_CLOSE;)

	/* Run through the list of databases to simply close them out (still open by parent) */
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top;
		     r_local++)
		{
			if (r_local->open && !r_local->was_open &&
			    (dba_bg == r_local->dyn.addr->acc_meth || dba_mm == r_local->dyn.addr->acc_meth))
			{
				udi = (unix_db_info *)(r_local->dyn.addr->file_cntl->file_info);
				csa = &udi->s_addrs;
				csd = csa->hdr;
				/* Close journal file if open. Check for JNL_ALLOWED instead of JNL_ENABLED to ensure
				 * we do not miss out on closing open journal file descriptors in the case where the
				 * current jnl_state is "jnl_closed" but we had opened the file when it was "jnl_open".
				 */
				if (JNL_ALLOWED(csd) && (NULL != csa->jnl) && (NOJNL != csa->jnl->channel))
					CLOSEFILE_RESET(csa->jnl->channel, rc);	/* resets "channel" to FD_INVALID */
				CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
			}
		}
	}
#ifndef MUTEX_MSEM_WAKE
	/* We don't need the parent's mutex socket anymore either (if we are using the socket) */
	if (FD_INVALID != mutex_sock_fd)
		CLOSEFILE_RESET(mutex_sock_fd, rc);	/* resets "mutex_sock_fd" to FD_INVALID */
#endif
}
