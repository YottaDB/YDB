/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CDB_SC
#define CDB_SC

/*********************************  WARNING:  ***********************************
*   Several of these codes are concurrently defined in GVCST_BLK_SEARCH.MAR,	*
*   GVCST_SEARCH.MAR, MUTEX.MAR, and MUTEX_STOPREL.MAR.  If their positions	*
*   are changed here, their definitions must be modified there as well!		*
********************************************************************************/

enum cdb_sc
{
#define CDB_SC_NUM_ENTRY(code, value)			code = value,
#define CDB_SC_UCHAR_ENTRY(code, is_wcs_code, value)	code = value,
#define CDB_SC_LCHAR_ENTRY(code, is_wcs_code, value)	code = value,
#include "cdb_sc_table.h"
#undef CDB_SC_NUM_ENTRY
#undef CDB_SC_UCHAR_ENTRY
#undef CDB_SC_LCHAR_ENTRY
};

#define FUTURE_READ 0		/* used by dsk_read and t_qread */

#endif
