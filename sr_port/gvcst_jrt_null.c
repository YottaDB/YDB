/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for jnl.h */
#include "jnl.h"

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_jrt_null.h"	/* for gvcst_jrt_null prototype */
#include "jnl_get_checksum.h"

GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	jnl_gbls_t		jgbl;
#ifdef DEBUG
GBLREF	uint4			dollar_tlevel;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	unsigned int		t_tries;
#endif

error_def(ERR_JRTNULLFAIL);

void	gvcst_jrt_null()
{
	enum cdb_sc		status;
	struct_jrec_null	*rec;

	assert(NULL != gv_cur_region);
	assert(NULL != cs_addrs);
	assert(cs_addrs == &FILE_INFO(gv_cur_region)->s_addrs);
	t_begin(ERR_JRTNULLFAIL, UPDTRNS_DB_UPDATED_MASK);
	assert(!dollar_tlevel);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry */
	non_tp_jfb_ptr->rectype = JRT_NULL;
	non_tp_jfb_ptr->record_size = NULL_RECLEN;
	non_tp_jfb_ptr->checksum = INIT_CHECKSUM_SEED;
	rec = (struct_jrec_null *)non_tp_jfb_ptr->buff;
	rec->prefix.jrec_type = JRT_NULL;
	rec->prefix.forwptr = NULL_RECLEN;
	rec->prefix.checksum = INIT_CHECKSUM_SEED;
	rec->suffix.backptr = NULL_RECLEN;
	rec->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	rec->filler = 0;
	jgbl.cumul_jnl_rec_len = NULL_RECLEN;
	/* The rest of the initialization is taken care of by jnl_write_logical (invoked in t_end below) */
	DEBUG_ONLY(jgbl.cumul_index = 1;)
	for (;;)
	{
		DEBUG_ONLY(jgbl.cu_jnl_index = 0;)
		if ((trans_num)0 == t_end(NULL, NULL, TN_NOT_SPECIFIED))
			continue;
		return;
	}
}
