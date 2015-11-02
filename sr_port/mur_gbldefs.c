/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
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
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#ifdef VMS
#include "iosb_disk.h"
#endif
#include "mur_read_file.h"

GBLDEF 	mur_gbls_t		murgbl;
GBLDEF	reg_ctl_list		*mur_ctl, *rctl_start;
GBLDEF	mur_opt_struct 		mur_options;
GBLDEF	boolean_t		mur_close_files_done;
