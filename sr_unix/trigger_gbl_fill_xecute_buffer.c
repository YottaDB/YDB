/****************************************************************
 *								*
 * Copyright (c) 2010-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"
#include "gvcst_protos.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro (within BUILD_HASHT_...) */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "op.h"
#include "trigger.h"
#include "trigger_gbl_fill_xecute_buffer.h"
#include "gtm_trigger_trc.h"
#include "mvalconv.h"
#include "memcoherency.h"
#include "t_retry.h"
#include "gtmimagename.h"
#include "filestruct.h"			/* for FILE_INFO, needed by REG2CSA */
#include "have_crit.h"
#include "repl_msg.h"
#include "gtmsource.h"			/* for jnlpool_addrs */

LITREF	mval			literal_ten;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_TRIGDEFBAD);

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	uint4			dollar_tlevel;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
#ifdef DEBUG
GBLREF	boolean_t		is_updproc;
#endif
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];

LITREF	char			*trigger_subs[];

STATICDEF char			*xecute_buff;

STATICFNDCL CONDITION_HANDLER(trigger_gbl_fill_xecute_buffer_ch);

/* The trigger_gbl_fill_xecute_buffer() routine below malloc()s storage for our trigger buffer buffer below but any one
 * of the various database calls can cause a restart that "loses" the buffer. So we wrap the call will this condition
 * handler to release the buffer if one was allocated before moving on to the next handler.
 */
STATICFNDEF CONDITION_HANDLER(trigger_gbl_fill_xecute_buffer_ch)
{
	START_CH(TRUE);
	if (!DUMPABLE && (NULL != xecute_buff))
		free(xecute_buff);
	NEXTCH;
}

char *trigger_gbl_fill_xecute_buffer(char *trigvn, int trigvn_len, mval *trig_index, mval *first_rec, int4 *xecute_len)
{
	mval			data_val;
	mval			index, key_val, *val_ptr;
	int4			len, xecute_buff_len;
	int4			num;
	int4			trgindx;
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];
	int4			util_len;
	char			*xecute_buff_ptr;
	mval			xecute_index;
	DEBUG_ONLY(int		gvt_cycle;)
	DEBUG_ONLY(int	   	csd_cycle;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 < dollar_tlevel); /* Too be added later when it stops breaking MUPIP SELECT & $ZTRIGGER("SELECT"..) */
	xecute_buff = NULL;
	ESTABLISH_RET(trigger_gbl_fill_xecute_buffer_ch, NULL);
	index = *trig_index;
	DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: entry $tlevel:%d\tindex:%d\t:crit:%d\tkv:%d\n",
				dollar_tlevel, mval2i(&index), have_crit(CRIT_HAVE_ANY_REG), key_val.str.len));
	if (NULL != first_rec)
	{
		DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: NULL != first_rec\n"));
		xecute_buff_len = first_rec->str.len;
		assert(MAX_XECUTE_LEN >= xecute_buff_len);
		xecute_buff = malloc(xecute_buff_len);
		memcpy(xecute_buff, first_rec->str.addr, xecute_buff_len);
	} else
	{	/* First check for a single record xecute string */
		DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: First check for a single record xecute string\n"));
		BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, index,
					trigger_subs[XECUTE_SUB], STRLEN(trigger_subs[XECUTE_SUB]));
		if (gvcst_get(&key_val))
		{
			xecute_buff_len = key_val.str.len;
			assert(MAX_XECUTE_LEN >= xecute_buff_len);
			xecute_buff = malloc(xecute_buff_len);
			memcpy(xecute_buff, key_val.str.addr, xecute_buff_len);
			*xecute_len = xecute_buff_len;
			DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: Found a single record xecute string of %d bytes\n",
						xecute_buff_len));
			REVERT;
			return xecute_buff;
		} else
		{	/* No single line trigger exists. See if multi-line trigger exists. The form is ^#t(gbl,indx,XECUTE,n)
			 * so can be easily tested for with $DATA().
			 */
			DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: No single record xecute string(%d), try for multiline\n",
						key_val.str.len));
			op_gvdata(&data_val);
			if ((literal_ten.m[0] != data_val.m[0]) || (literal_ten.m[1] != data_val.m[1]))
			{	/* The process' view of the triggers is likely stale. Restart to be safe.
				 * Triggers can be invoked only by GT.M and Update process. We expect to see GT.M processes to
				 * restart due to concurrent trigger changes. The Update Process should only restart if it is a
				 * supplementary instance. Assert accordingly. Note similar asserts occur in t_end.c and
				 * tp_tend.c.
				 */
				assert(!is_updproc || (jnlpool && jnlpool->repl_inst_filehdr->is_supplementary
							&& !jnlpool->jnlpool_ctl->upd_disabled));
				DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: multiline not found, retry\n"));
				/* Assert that the cycle has changed but in order to properly do the assert, we need a memory
				 * barrier since cs_data->db_trigger_cycle could be stale in our cache.
				 */
				DEBUG_ONLY(SHM_READ_MEMORY_BARRIER);
				/* Vars in locals so can look at them in the core instead of at constantly changing numbers */
				DEBUG_ONLY(gvt_cycle = gv_target->db_trigger_cycle);
				DEBUG_ONLY(csd_cycle = cs_data->db_trigger_cycle);
				assert(csd_cycle > gvt_cycle);
				if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
					t_retry(cdb_sc_triggermod);
				assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
				trgindx = mval2i(&index);
				SET_PARAM_STRING(util_buff, util_len, trgindx, ",\"XECUTE\"");
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
						trigvn_len, trigvn, util_len, util_buff);
			}
		}
		/* Multi-line triggers exist */
		num = 0;
		i2mval(&xecute_index, num);
		BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(trigvn, trigvn_len, index,
					trigger_subs[XECUTE_SUB], STRLEN(trigger_subs[XECUTE_SUB]), xecute_index);
		if (!gvcst_get(&key_val))
		{	/* There has to be an XECUTE string or else it is a retry situation (due to concurrent updates) */
			DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: problem getting multiline record count, retry\n"));
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			trgindx = mval2i(&index);
			SET_PARAM_STRING(util_buff, util_len, trgindx, ",\"XECUTE\"");
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					trigvn_len, trigvn, util_len, util_buff);
		}
		val_ptr = &key_val;
		xecute_buff_len = mval2i(val_ptr);
		assert(MAX_XECUTE_LEN >= xecute_buff_len);
		xecute_buff_ptr = xecute_buff = malloc(xecute_buff_len);
		len = 0;
		while (len < xecute_buff_len)
		{
			i2mval(&xecute_index, ++num);
			BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(trigvn, trigvn_len, index,
					trigger_subs[XECUTE_SUB], STRLEN(trigger_subs[XECUTE_SUB]), xecute_index);
			if (!gvcst_get(&key_val))
				break;
			if (xecute_buff_len < (len + key_val.str.len))
			{	/* The DB string total is longer than the length stored at index 0 -- something is wrong.
				 * Most likely a retry necessary due to concurrent changes.
				 */
				DBGTRIGR((stderr, "trigger_gbl_fill_xecute_buffer: problem getting multiline component, retry\n"));
				free(xecute_buff);
				if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
					t_retry(cdb_sc_triggermod);
				assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
				SET_PARAM_STRING(util_buff, util_len, num, ",\"XECUTE\"");
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
						trigvn_len, trigvn, util_len, util_buff);
			}
			memcpy(xecute_buff_ptr, key_val.str.addr, key_val.str.len);
			xecute_buff_ptr += key_val.str.len;
			len += key_val.str.len;
		}
	}
	*xecute_len = xecute_buff_len;
	REVERT;
	return xecute_buff;
}
#endif
