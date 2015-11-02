/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_termios.h"

#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "setterm.h"
#include "gtm_isanlp.h"

error_def(ERR_TCSETATTR);

void setterm(io_desc *ioptr)
{
	int		status;
	int		save_errno;
	struct termios	t;
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct *) ioptr->dev_sp;
	t = *tt_ptr->ttio_struct;
	if (tt_ptr->canonical)
	{	t.c_lflag &= ~(ECHO);
		t.c_lflag |= ICANON;
	}else
	{	t.c_lflag &= ~(ICANON | ECHO);
		t.c_cc[VTIME] = 8;
		t.c_cc[VMIN] = 1;
	}
	t.c_iflag &= ~(ICRNL);
	Tcsetattr(tt_ptr->fildes, TCSANOW, &t, status, save_errno);
	if (0 != status)
	{
		if (gtm_isanlp(tt_ptr->fildes) == 0)
			rts_error(VARLSTCNT(4) ERR_TCSETATTR, 1, tt_ptr->fildes, save_errno);
	}
	return;
}


/* These routines are here because it is frightfully important to keep them
   in synch with setterm.  When they get out of line, a r x:0 causes your
   terminal to be unreachable thereafter.
*/

/* iott_mterm sets the inter-character timer (t.c_cc[VTIME]) to 0.0 seconds
   so that a read with a zero timeout (ie.  Read x:0) will not wait.
*/

void iott_mterm(io_desc *ioptr)
{
	int		status;
	int		save_errno;
	struct termios	t;
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct *) ioptr->dev_sp;
	t = *tt_ptr->ttio_struct;
	if (tt_ptr->canonical)
	{	t.c_lflag &= ~(ECHO);
		t.c_lflag |= ICANON;
	}else
	{	t.c_lflag &= ~(ICANON | ECHO);
#ifdef __MVS__
		t.c_cc[VTIME] = 1;
#else
		t.c_cc[VTIME] = 0;
#endif
		t.c_cc[VMIN] = 0;
	}
	t.c_iflag &= ~(ICRNL);
	Tcsetattr(tt_ptr->fildes, TCSANOW, &t, status, save_errno);
	if (0 != status)
		rts_error(VARLSTCNT(4) ERR_TCSETATTR, 1, tt_ptr->fildes, save_errno);
	return;
}


/* iott_rterm restores the inter-character timer (t.c_cc[VTIME]) to 0.8 seconds
*/

void iott_rterm(io_desc *ioptr)
{
	int		status;
	int		save_errno;
	struct termios	t;
	d_tt_struct  	*tt_ptr;

	tt_ptr = (d_tt_struct *) ioptr->dev_sp;
	t = *tt_ptr->ttio_struct;
	if (tt_ptr->canonical)
	{	t.c_lflag &= ~(ECHO);
		t.c_lflag |= ICANON;
	}else
	{	t.c_lflag &= ~(ICANON | ECHO);
		t.c_cc[VTIME] = 8;
		t.c_cc[VMIN] = 1;
	}
	t.c_iflag &= ~(ICRNL);
	Tcsetattr(tt_ptr->fildes, TCSANOW, &t, status, save_errno);
	if (0 != status)
		rts_error(VARLSTCNT(4) ERR_TCSETATTR, 1, tt_ptr->fildes, save_errno);
	return;
}
