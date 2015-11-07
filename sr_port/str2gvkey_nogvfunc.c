/****************************************************************
 *								*
 *	Copyright 2002, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "subscript.h"
#include "str2gvargs.h"
#include "str2gvkey.h"
#include "collseq.h"

#ifdef UNIX
GBLREF  repl_conn_info_t        *this_side;
#endif

#ifdef VMS
GBLREF	gd_region	*gv_cur_region;
#endif

void str2gvkey_nogvfunc(char *cp, int len, gv_key *key)
{ /* We have two functions str2gvkey_gvfunc and str2gvkey_nogvfunc instead of passing a boolean argument to say a common  */
  /* function str2gvkey because GVUSR_QUERYGET when moved to DDPGVUSR would pull in op_gvname and op_gvargs objects - will */
  /* cause nasty problems in the link */

	boolean_t	naked;
	gvargs_t	op_gvargs;
	int		subsc;

	naked = str2gvargs(cp, len, &op_gvargs);
	assertpro(!naked);	/* because this function does not handle nakeds correctly */
	key->end = op_gvargs.args[0]->str.len;
	memcpy(key->base, op_gvargs.args[0]->str.addr, key->end);
	key->base[key->end] = 0;
	key->end++;
	key->prev = 0;
	/* Since this function is called from the source and/or receiver server for now and since both of them ensure
	 * all regions have the same std_null_coll value and since "this_side" holds this information (for Unix) we
	 * use this to determine the std_null_coll setting to use for mval2subsc. In VMS, we use a hardcoded value.
	 */
	for (subsc = 1; subsc < op_gvargs.count; subsc++)
		mval2subsc(op_gvargs.args[subsc], key, UNIX_ONLY(this_side->is_std_null_coll)
				VMS_ONLY((NULL != gv_cur_region ? gv_cur_region->std_null_coll : STD_NULL_COLL_FALSE)));
	return;
}
