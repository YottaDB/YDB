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

#ifndef MUPIP_RESTORE_INCLUDED
#define MUPIP_RESTORE_INCLUDED

void mupip_restore(void);

#ifdef UNIX
STATICFNDCL	void	exec_read(BFILE *bf, char *buf, int nbytes);
STATICFNDCL	void	tcp_read(BFILE *bf, char *buf, int nbytes);
#endif

#endif /* MUPIP_RESTORE_INCLUDED */
