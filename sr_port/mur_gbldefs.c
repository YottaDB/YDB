/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

 /* General repository for mupip journal command related global variable definitions.
  * This keeps us from pulling in modules and all their references
  * when all we wanted was the global data def.. */

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#ifdef VMS
#include "iosb_disk.h"
#endif
#include "mur_read_file.h"

GBLDEF 	mur_gbls_t		murgbl;
GBLDEF 	mur_rab_t		mur_rab;	/* To access a record */
GBLDEF	jnl_ctl_list		*mur_jctl;
GBLDEF	reg_ctl_list		*mur_ctl;
GBLDEF 	int			mur_regno;
GBLDEF	mur_opt_struct 		mur_options;
GBLDEF	mur_read_desc_t		mur_desc;	/* Only for mur_read_files.c and mur_get_pini.c */
