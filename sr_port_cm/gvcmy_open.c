/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include <errno.h>
#include "parse_file.h"
#include "gtm_stdlib.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "stringpool.h"
#include "gtm_string.h"
#include "iosp.h"
#include "gvcmy_open.h"
#include "gvcmy_close.h"
#include "trans_log_name.h"
#include "gvcmz.h"
#include "copy.h"
#include "error.h"
#include "op.h"

GBLREF spdesc stringpool;

error_def(ERR_BADSRVRNETMSG);
error_def(ERR_INVNETFILNM);
error_def(ERR_LOGTOOLONG);
error_def(ERR_NETDBOPNERR);
error_def(ERR_REMOTEDBNOSPGBL);
error_def(ERR_SYSCALL);

#define GTCM_ENVVAR_PFX "GTCM_"
#define GTCM_ENVVAR_PFXLEN (SIZEOF(GTCM_ENVVAR_PFX) - 1)

void gvcmy_open(gd_region *reg, parse_blk *pb)
{
	struct CLB	*clb_ptr;
	link_info	*li;
	unsigned char	*ptr, *top, *fn, *libuff;
	char		*trans_name;
	bool		new = FALSE;
	int		len;
	int4		status;
	cmi_descriptor	node;
	mstr		task1, task2;
	unsigned char	buff[256], lbuff[MAX_HOST_NAME_LEN + GTCM_ENVVAR_PFXLEN];
	short		temp_short;
	MSTR_DEF(task, 0, NULL);
	DCL_THREADGBL_ACCESS;		/* needed by TREF usage inside SET_REGION_OPEN_TRUE macro */

	SETUP_THREADGBL_ACCESS;		/* needed by TREF usage inside SET_REGION_OPEN_TRUE macro */
	ESTABLISH(gvcmy_open_ch);
	if (reg->is_spanned)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REMOTEDBNOSPGBL, 2, REG_LEN_STR(reg));
	if (!pb->b_node)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVNETFILNM);
	fn = (unsigned char *)pb->l_dir;
	top = fn + pb->b_esl - pb->b_node; 	/* total length except node gives end of string */
	/*
	   The "task" value for unix comes from a logical of the form GTCM_<hostname>. This value
	   has the form of "<hostname or ip:>portnum". If the optional hostname or ip is specified
	   as part of the value, it will override the node value from the global directory. That
	   processing is handled down in the CMI layer.
	*/
	node.addr = pb->l_node;
	node.len = pb->b_node - 1;
	memcpy(lbuff, GTCM_ENVVAR_PFX, GTCM_ENVVAR_PFXLEN);
	memcpy(lbuff + GTCM_ENVVAR_PFXLEN, node.addr, node.len);
	task1.addr = (char *)lbuff;
	task1.len = node.len + (int)GTCM_ENVVAR_PFXLEN;
	lbuff[task1.len] = '\0';
	task2.addr = (char *)buff;
	task2.len = 0;
	if (NULL != (trans_name = GETENV((const char *)lbuff)))
	{
		status = SS_NORMAL;
		task2.len = STRLEN(trans_name);
		if (SIZEOF(buff) > task2.len)
			memcpy(task2.addr, trans_name, task2.len);
		else
			status = SS_LOG2LONG;
	} else
		status = SS_NOLOGNAM;
	if (SS_NOLOGNAM != status)
	{
		if (SS_NORMAL != status)
		{
			if (SS_LOG2LONG == status)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(5) ERR_LOGTOOLONG, 3, task1.len, task1.addr, SIZEOF(buff) - 1);
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
		}
		task.addr = (char *)task2.addr;
		task.len = task2.len;
	}
	clb_ptr = cmu_getclb(&node, &task);
	if (!clb_ptr)		/* If link not open */
	{
		new = TRUE;
		clb_ptr = gvcmz_netopen(clb_ptr, &node, &task);
	} else
	{
		assert(((link_info *)clb_ptr->usr)->lnk_active);
		len = (int)(top - fn + 2 + S_HDRSIZE);
		if (len <  CM_MINBUFSIZE)
			len = CM_MINBUFSIZE;
		ENSURE_STP_FREE_SPACE(len);
		clb_ptr->mbf = stringpool.free;
		clb_ptr->mbl = len;
	}
	li = (link_info *)clb_ptr->usr;
	clb_ptr->cbl = top - fn + 2 + S_HDRSIZE;
	*clb_ptr->mbf = CMMS_S_INITREG;
	ptr = clb_ptr->mbf + 1;
	temp_short = top - fn;
	CM_PUT_SHORT(ptr, temp_short, li->convert_byteorder);
	ptr += SIZEOF(short);
	memcpy(ptr, fn, top - fn);
	status = cmi_write(clb_ptr);
	if (CMI_ERROR(status))
	{
		if (new)
			gvcmy_close(clb_ptr);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_NETDBOPNERR, 0, status);
	}
	status = cmi_read(clb_ptr);
	if (CMI_ERROR(status))
	{
		if (new)
			gvcmy_close(clb_ptr);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_NETDBOPNERR, 0, status);
	}
	if (CMMS_T_REGNUM != *clb_ptr->mbf)
	{
		if (CMMS_E_ERROR != *clb_ptr->mbf)
		{
			if (new)
				gvcmy_close(clb_ptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_NETDBOPNERR, 0, ERR_BADSRVRNETMSG);
		}
		gvcmz_errmsg(clb_ptr, new);
	}
	ptr = clb_ptr->mbf + 1;
	reg->cmx_regnum = *ptr++;
	reg->null_subs = *ptr++;
	CM_GET_SHORT(reg->max_rec_size, ptr, li->convert_byteorder);
	ptr += SIZEOF(short);
	if (reg->max_rec_size + CM_BUFFER_OVERHEAD > li->buffer_size)
	{
		if (li->buffered_count)
		{
			assert(li->buffer);
			libuff = malloc(reg->max_rec_size);
			memcpy(libuff, li->buffer, li->buffer_used);
			free(li->buffer);
			li->buffer = libuff;
		} else if (li->buffer)
		{
			free(li->buffer);
			li->buffer = 0;
		}
		li->buffer_size = reg->max_rec_size + CM_BUFFER_OVERHEAD;
	}
	CM_GET_SHORT(reg->max_key_size, ptr, li->convert_byteorder);
	ptr += SIZEOF(short);
	reg->std_null_coll = (li->server_supports_std_null_coll) ? *ptr++ : 0;
		/* From level 210 (GT.M V5), server will send null subscript collation info into CMMS_S_INITREG message */
	reg->dyn.addr->cm_blk = clb_ptr;
	REG_ACC_METH(reg) = dba_cm;
	SET_REGION_OPEN_TRUE(reg, WAS_OPEN_FALSE);
	clb_ptr->mbl = li->buffer_size;
	if (clb_ptr->mbl < CM_MINBUFSIZE)
		clb_ptr->mbl = CM_MINBUFSIZE;
	REVERT;
	return;
}
