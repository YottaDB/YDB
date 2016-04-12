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
#include "copy.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcmtr_protos.h"
#include "gtcm_find_region.h"
#include "gtcm_bind_name.h"
#include "gvcst_protos.h"	/* for gvcst_kill prototype */

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key 		*gv_currkey;
GBLREF jnl_process_vector *originator_prc_vec;

bool gtcmtr_kill(void)
{
	cm_region_list	*reg_ref;
	unsigned char	*ptr, regnum;
	unsigned short	len;
	static readonly	gds_file_id file;

	error_def(ERR_DBPRIVERR);

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_KILL);
	ptr++;
	GET_SHORT(len, ptr);
	ptr += SIZEOF(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry,regnum);
	len--; /* subtract size of regnum */
	CM_GET_GVCURRKEY(ptr, len);
	gtcm_bind_name(reg_ref->reghead, TRUE);
	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	if (JNL_ALLOWED(cs_addrs))
	{	/* we need to copy client's specific prc_vec into the global variable in order that the gvcst* routines
		 *	do the right job. actually we need to do this only if JNL_ENABLED(cs_addrs), but since it is not
		 *	easy to re-execute the following two assignments in case gvcst_kill's call to t_end encounters a
		 *	cdb_sc_jnlstatemod retry code, we choose the easier approach of executing the following segment
		 *	if JNL_ALLOWED(cs_addrs) is TRUE instead of checking for JNL_ENABLED(cs_addrs) to be TRUE.
		 * this approach has the overhead that we will be doing the following assignments even though JNL_ENABLED
		 * 	might not be TRUE but since the following two are just pointer copies, it is not considered a big overhead.
		 * this approach ensures that the jnl_put_jrt_pini gets the appropriate prc_vec for writing into the
		 * 	journal record in case JNL_ENABLED turns out to be TRUE in t_end time.
		 * note that the value of JNL_ALLOWED(cs_addrs) cannot be changed on the fly without obtaining standalone access
		 * 	and hence the correctness of prc_vec (whenever it turns out necessary) is guaranteed.
		 */
		originator_prc_vec = curr_entry->pvec;
		cs_addrs->jnl->pini_addr = reg_ref->pini_addr;
	}
	if (gv_target->root)
		gvcst_kill(TRUE);
	if (JNL_ALLOWED(cs_addrs))
		reg_ref->pini_addr = cs_addrs->jnl->pini_addr;  /* In case  journal switch occurred */
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_KILL;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
