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

#include "mdef.h"

#ifdef GTM_FD_TRACE

#include "gtm_stat.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "eintr_wrappers.h"
#include "dpgbldir.h"
#include "gtm_dbjnl_dupfd_check.h"
#include "error.h"
#include "send_msg.h"
#include "is_file_identical.h"

#define	MAX_FD_FOR_FASTCHECK	256

GBLDEF	gd_region	*dupfd_check_reg;		/* for debugging purposes */
GBLDEF	int		dupfd_check_fd;			/* for debugging purposes */
GBLDEF	fdinfo_t	*dupfd_check_openfdarray;	/* for debugging purposes */

/* Before fixing corrupt jnl fd take a core dump and send syslog message to ensure it gets analyzed */
#define	FIX_CORRUPT_JNLFD(REG)				\
{							\
	jnl_private_control	*jpc;			\
							\
	error_def(ERR_GVFAILCORE);			\
							\
	assert(FALSE);					\
	gtm_fork_n_core();				\
	send_msg(VARLSTCNT(1) ERR_GVFAILCORE);		\
	jpc = FILE_INFO(REG)->s_addrs.jnl;		\
	jpc->channel = NOJNL;				\
	jpc->cycle--;					\
	jpc->pini_addr = 0;				\
}

boolean_t gtm_check_fd_is_valid(gd_region *reg, boolean_t is_db, int fd)
{
	struct stat	stat_buf;
	sgmnt_addrs	*csa;
	int		fstat_res;

	FSTAT_FILE(fd, &stat_buf, fstat_res);
	if (-1 == fstat_res)
		GTMASSERT;
	assert(reg->open);
	if (is_db)
	{
		if (!is_gdid_stat_identical(&FILE_ID(reg), &stat_buf))
			GTMASSERT;	/* db fd does not corespond back to itself */
	} else
	{
		csa = &FILE_INFO(reg)->s_addrs;
		/* If fd does not point back to journal file, it could be because of a concurrent journal switch.
		 * Check that. If that fails as well, go ahead and fix the journal file descriptor.
		 */
		if (!is_gdid_stat_identical(JNL_GDID_PTR(csa), &stat_buf) && !JNL_FILE_SWITCHED(csa->jnl))
		{
			FIX_CORRUPT_JNLFD(reg);	/* Journal file fd is corrupt. Fix it. */
			return FALSE;
		}
	}
	return TRUE;
}

void	gtm_dupfd_check_specific(gd_region *reg, fdinfo_t *open_fdarray, int fd, boolean_t is_db)
{
	gd_region		*db_reg, *jnl_reg;
	int			fstat_res;
	struct stat		stat_buf;

	/* Record key local variables in globals in case we take a GTMASSERT and need to analyze the pro core */
	dupfd_check_fd = fd;
	dupfd_check_reg = reg;
	dupfd_check_openfdarray = open_fdarray;
	if (0 > fd)
		GTMASSERT;
	if (MAX_FD_FOR_FASTCHECK > fd)
	{	/* fd is within fastcheck range. We assume the first fd that fills the array is valid and skip the
		 * heavyweight fstat check. For dbg builds though, we do this check just so that code is exercised as well.
		 */
		assert((NULL != open_fdarray[fd].reg) || gtm_check_fd_is_valid(reg, is_db, fd));
		if (NULL != open_fdarray[fd].reg)
		{
			if (is_db && open_fdarray[fd].is_db)
				GTMASSERT;	/* Two databases have SAME fd. Cannot do much to recover from this situation */
			/* The fds of one region's database and another region's journal collide.
			 * Check if db fd is indeed valid and if so close the journal's fd.
			 * If db fd is not valid, then cannot do much to recover from this situation.
			 */
			FSTAT_FILE(fd, &stat_buf, fstat_res);
			if (-1 == fstat_res)
				GTMASSERT;
			if (is_db)
			{
				db_reg = reg;
				jnl_reg = open_fdarray[fd].reg;
			} else
			{
				db_reg = open_fdarray[fd].reg;
				jnl_reg = reg;
			}
			if (!is_gdid_stat_identical(&FILE_ID(db_reg), &stat_buf))
				GTMASSERT;	/* fd does not correspond back to the db file so the db fd got corrupt somehow */
			/* fd corresponds back to the database which means the jnl file structure is corrupt which can be fixed */
			FIX_CORRUPT_JNLFD(jnl_reg);
			if (!is_db) /* Entry in open_fdarray[fd] is correct. So return without updating it (to the wrong value) */
				return;
		}
		open_fdarray[fd].reg = reg;
		open_fdarray[fd].is_db = is_db;
	} else
	{	/* fd is outside the fast check range. no other go but check that fd is indeed valid (using heavyweight fstat) */
		gtm_check_fd_is_valid(reg, is_db, fd);
	}
}

/* This routine is a debugging tool written to detect the symptom of D9I11-002714 before any damage to the database occurs.
 * It checks all open db and jnl file descriptors and identifies any duplicates and if so creates a core file for analysis.
 */
void	gtm_dbjnl_dupfd_check(void)
{
	fdinfo_t	open_fdarray[MAX_FD_FOR_FASTCHECK];
	gd_addr		*addr_ptr;
	gd_region	*r_top, *reg;
	gd_segment	*seg;
	int		fd;
	sgmnt_addrs	*csa;
	unix_db_info	*udi;

	memset(open_fdarray, 0, SIZEOF(open_fdarray));
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions; reg < r_top; reg++)
		{
			seg = reg->dyn.addr;
			if ((dba_bg != seg->acc_meth) && (dba_mm != seg->acc_meth))
				continue;
			if (!reg->open || reg->was_open)
				continue;
			udi = FILE_INFO(reg);
			/* Check DB first */
			fd = udi->fd;
			gtm_dupfd_check_specific(reg, open_fdarray, fd, TRUE);
			/* Check JNL next */
			csa = &udi->s_addrs;
			if (JNL_ALLOWED(csa))
			{
				fd = csa->jnl->channel;
				if (NOJNL != fd)
					gtm_dupfd_check_specific(reg, open_fdarray, fd, FALSE);
			}
		}
	}
}

#endif
