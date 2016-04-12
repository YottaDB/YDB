/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "gtm_isanlp.h"

/* gtm_isanlp returns 1 if the indicated file is associated with
   a line printer and 0 if it isn't.                             */

/* This is a stub version that always returns 0.  */

/* gtm_isanlp is intended to be called whenever a tty-specific
   ioctl call fails; if gtm_isanlp returns true (1), then the
   device is assumed to be a line printer and the tty-specific
   ioctl failure will be ignored.  So far, systems that don't
   allow tty-specific ioctl calls for line printer devices have
   some other way to determine whether the device is a line
   printer; those systems have specific gtm_isanlp functions.
   This version is for those Unix systems that do allow tty-
   specific ioctl calls for line printers, so this function
   returns 0 to ensure the ioctl error will not be ignored.      */


int gtm_isanlp (int file_des)
{
	return (0);
}
