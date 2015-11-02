/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "xfer_enum.h"

/* Declare all prototypes with same signature as xfer_entry_t */
#define XFER(a,b) b()
int
#include "xfer.h"
;
#undef XFER

#ifndef UNICODE_SUPPORTED
/* Call "z" counterparts for non-unicode flavor of these functions on unsupported platforms */

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

#define XFER(a,b) b

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


