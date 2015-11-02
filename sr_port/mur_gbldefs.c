/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
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

/* The below array is used to identify the first zero bit in a given byte. For instance, the first non-zero bit position of
 * 0xBF (10111111 in binary) is first_zerobit_position[0xBF] = 6.
 */
LITDEF	char	first_zerobit_position[256] =	{        /* 0x0 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0xA 0xB 0xC 0xD 0xE 0xF */

						  /* 0x0 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
						  /* 0x1 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  5,
						  /* 0x2 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
					 	  /* 0x3 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  6,
						  /* 0x4 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
						  /* 0x5 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  5,
						  /* 0x6 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
						  /* 0x7 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  7,
						  /* 0x8 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
						  /* 0x9 */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  5,
						  /* 0xA */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
						  /* 0xB */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  6,
						  /* 0xC */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
						  /* 0xD */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  5,
						  /* 0xE */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  4,
						  /* 0xF */  0,  1,  0,  2,  0,  1,  0,  3,  0,  1,  0,  2,  0,  1,  0,  -1
						};
