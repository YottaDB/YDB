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

#ifndef __WCS_RECOVER_H__
#define __WCS_RECOVER_H__

void		wcs_recover(gd_region *reg);
#ifdef UNIX
void	wcs_mm_recover(gd_region *reg);
#elif defined(VMS)
void	wcs_mm_recover(gd_region *reg);
#else
#error UNSUPPORTED PLATFORM
#endif

#endif
