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

#ifndef GTM_DBJNL_DUPFD_CHECK_H_INCLUDED
#define	GTM_DBJNL_DUPFD_CHECK_H_INCLUDED

typedef	struct
{
	gd_region	*reg;
	boolean_t	is_db;	/* TRUE implies db file, FALSE implies journal file */
} fdinfo_t;

boolean_t	gtm_check_fd_is_valid(gd_region *reg, boolean_t is_db, int fd);
void		gtm_dupfd_check_specific(gd_region *reg, fdinfo_t *open_fd, int fd, boolean_t is_db);
void		gtm_dbjnl_dupfd_check(void);

#endif

