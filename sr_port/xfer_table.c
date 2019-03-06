/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 74ea4a3c... GT.M V6.3-006
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "xfer_enum.h"

/* Below is the list of header files needed for the prototypes of C functions used in the transfer table. */
#include "lv_val.h"
#include "op.h"
#include "flt_mod.h"
#include "mprof.h"
#include "stack_frame.h"
#include "glvn_pool.h"
#include "rtnhdr.h"
#include "gdsroot.h"		/* needed by "gvname_info.h" */
#include "gdskill.h"		/* needed by "gvname_info.h" */
#include "gtm_facility.h"	/* needed by "gvname_info.h" */
#include "gdsbt.h"		/* needed by "gvname_info.h" */
#include "gdsfhead.h"		/* needed by "gvname_info.h" */
#include "buddy_list.h"		/* needed by "tp.h" */
#include "hashtab_int4.h"	/* needed by "tp.h" */
#include "filestruct.h"		/* needed by "jnl.h" */
#include "gdscc.h"		/* needed by "jnl.h" */
#include "jnl.h"		/* needed by "tp.h" */
#include "tp.h"			/* needed by "gvname_info.h" */
#include "gvname_info.h"	/* needed by "op_merge.h" */
#include "op_merge.h"

/* Below is the list of function prototypes for Assembly functions which do not have a header file with the prototype
 * and which are used in the transfer table.
 */
int mint2mval(), mval2bool(), mval2mint(), mval2num(), op_contain(), op_currtn(), op_equ(), op_equnul(),
    op_extjmp(), op_fnget(), op_follow(), op_forcenum(), op_forinit(), op_gettruth(), op_iretmvad(), op_neg(),
    op_numcmp(), op_pattern(), op_restartpc(), op_sorts_after(), op_sto(), op_zhelp(), opp_break(), opp_commarg(),
    opp_hardret(), opp_inddevparms(), opp_indfnname(), opp_indfun(), opp_indglvn(), opp_indincr(), opp_indlvadr(),
    opp_indlvarg(), opp_indlvnamadr(), opp_indmerge(), opp_indpat(), opp_indrzshow(), opp_indsavglvn(), opp_indsavlvn(),
    opp_indset(), opp_indtext(), opp_iretmval(), opp_newintrinsic(), opp_newvar(), opp_rterror(), opp_setzbrk(), opp_svput(),
    opp_tcommit(), opp_trestart(), opp_trollback(), opp_tstart(), opp_xnew(), opp_zcont(), opp_zg1(), opp_zgoto();

#ifndef UTF8_SUPPORTED
/* Call "z" counterparts for non-utf8 flavor of these functions on unsupported platforms */

#define op_fnascii op_fnzascii
#define op_fnchar op_fnzchar
#define op_fnzechar op_fnzchar
#define op_fnextract op_fnzextract
#define op_setextract op_setzextract
#define op_fnfind op_fnzfind
#define op_fnj2 op_fnzj2
#define op_fnlength op_fnzlength
#define op_fnpopulation op_fnzpopulation
#define op_fnpiece op_fnzpiece
#define op_fnp1 op_fnzp1
#define op_setpiece op_setzpiece
#define op_setp1 op_setzp1
#define op_fntranslate op_fnztranslate

#endif

/* Initialize the table with the runtime routine functions */

#define XFER(a,b) (xfer_entry_t)b

GBLDEF xfer_entry_t xfer_table[] =
{
#include "xfer.h"
};
#undef XFER

#if defined(__ia64)

#ifdef XFER
#       undef XFER
#endif /* XFER */

#define XFER(a,b) #b


/* On IA64, we want to use CODE_ADDRESS() macro, to dereference all the function pointers, before storing them in
   global array. Now doing a dereference operation, as part of initialization, is not allowed by linux/gcc (HP'a aCC
   was more tolerant towards this). So to make sure that the xfer_table is initialized correctly, before anyone
   uses it, this function is called right at the beginning of gtm_startup
*/

int init_xfer_table()
{
        int i;

        for (i = 0; i < (SIZEOF(xfer_table) / SIZEOF(xfer_entry_t)); i++)
        {
                xfer_table[i] = (int (*)())CODE_ADDRESS(xfer_table[i]);
        }

        return 0;
}

#endif /* __ia64 */


