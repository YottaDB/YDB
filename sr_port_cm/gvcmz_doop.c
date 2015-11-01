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

#include "mdef.h"
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gvcmz.h"
#include "mvalconv.h"
#include "copy.h"
#include "gtm_string.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF spdesc		stringpool;

void gvcmz_doop(unsigned char query_code, unsigned char reply_code, mval *v)
{
	unsigned char	*ptr;
	short		len, temp_short;
	int4		status, max_reply_len;
	struct CLB	*lnk;
	error_def(ERR_BADSRVRNETMSG);

	lnk = gv_cur_region->dyn.addr->cm_blk;
	lnk->ast = 0;	/* all database queries are sync */
	lnk->cbl = 1 + /* HDR */
		   gv_currkey->end + /* key */
		   sizeof(unsigned short) + /* gv_key.top */
		   sizeof(unsigned short) + /* gv_key.end */
		   sizeof(unsigned short) + /* gv_key.prev */
		   sizeof(unsigned char) + /* gv_key.base */
		   sizeof(unsigned char) + /* regnum */
		   sizeof(unsigned short); /* size for variable len SUBSC */
	if (CMMS_Q_PUT == query_code)
		lnk->cbl += (sizeof(unsigned short) + v->str.len); /* VALUE + length */
	if (CMMS_Q_GET == query_code || CMMS_Q_QUERY == query_code && ((link_info *)lnk->usr)->query_is_queryget)
		max_reply_len = lnk->mbl + sizeof(unsigned short) + MAX_STRLEN; /* can't predict the length of data value */
	else
		max_reply_len = lnk->mbl;
	if (stringpool.top < stringpool.free + max_reply_len)
		stp_gcol(max_reply_len);
	lnk->mbf = stringpool.free;
	ptr = lnk->mbf;
	*ptr++ = query_code;
	/* temp_short = gv_currkey->end + sizeof(gv_key) + 1; */
	temp_short = gv_currkey->end + sizeof(unsigned short) + sizeof(unsigned short) + sizeof(unsigned short) + sizeof(char) + 1;
	CM_PUT_SHORT(ptr, temp_short, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += sizeof(short);
	*ptr++ = gv_cur_region->cmx_regnum;
	CM_PUT_SHORT(ptr, gv_currkey->top, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += sizeof(short);
	CM_PUT_SHORT(ptr, gv_currkey->end, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += sizeof(short);
	CM_PUT_SHORT(ptr, gv_currkey->prev, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += sizeof(short);
	memcpy(ptr, gv_currkey->base, gv_currkey->end + 1);
	if (query_code == CMMS_Q_PUT)
	{
		ptr += gv_currkey->end + 1;
		temp_short = (short)v->str.len;
		assert((int4)temp_short == v->str.len); /* short <- int4 assignment lossy? */
		CM_PUT_SHORT(ptr, temp_short, ((link_info *)(lnk->usr))->convert_byteorder);
		ptr += sizeof(short);
		memcpy(ptr,v->str.addr, v->str.len);
	}
	status = cmi_write(lnk);
	if (CMI_ERROR(status))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(query_code, status);
		return;
	}
	if (CMMS_Q_PUT == query_code || CMMS_Q_ZWITHDRAW == query_code || CMMS_Q_KILL == query_code)
	{
		status = cmi_read(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			gvcmz_error(query_code, status);
			return;
		}
		ptr = lnk->mbf;
		if (reply_code != *ptr)
		{
			if (*ptr != CMMS_E_ERROR)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			gvcmz_errmsg(lnk,FALSE);
		}
		return;
	}
	status = cmi_read(lnk);
	if (CMI_ERROR(status))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(query_code, status);
		return;
	}
	ptr = lnk->mbf;
	if (reply_code != *ptr)
	{
		if (CMMS_R_UNDEF != *ptr || CMMS_Q_GET != query_code)
		{
			if (CMMS_E_ERROR != *ptr)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			gvcmz_errmsg(lnk, FALSE);
		}
		return;
	}
	ptr++;
	if (CMMS_R_DATA == reply_code)
	{
		CM_GET_SHORT(temp_short, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
		if (1 != temp_short)
			rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
		ptr += sizeof(short);
		status = *ptr;	/* Temp assignment to status gets rid of compiler warning in MV_FORCE_MVAL macro */
		MV_FORCE_MVAL(v, status);
		return;
	}
	if (reply_code == CMMS_R_PREV || reply_code == CMMS_R_QUERY || reply_code == CMMS_R_ORDER)
	{
		CM_GET_SHORT(len, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
		ptr += sizeof(short);
		if (1 == len)
		{
			MV_FORCE_MVAL(v, 0);
		} else
		{
			if (*ptr++ != gv_cur_region->cmx_regnum)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			/* memcpy(gv_altkey, ptr, len - 1); */
			CM_GET_USHORT(gv_altkey->top, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
			ptr += sizeof(unsigned short);
			CM_GET_USHORT(gv_altkey->end, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
			ptr += sizeof(unsigned short);
			CM_GET_USHORT(gv_altkey->prev, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
			ptr += sizeof(unsigned short);
			memcpy(gv_altkey->base, ptr, len - 1 - sizeof(unsigned short) - sizeof(unsigned short) -
						     sizeof(unsigned short));
			ptr += (len - 1 - sizeof(unsigned short) - sizeof(unsigned short) - sizeof(unsigned short));
			MV_FORCE_MVAL(v, 1);
		}
		if (CMMS_R_QUERY != reply_code || 1 == len || !((link_info *)lnk->usr)->query_is_queryget)
		{
			if (CMMS_R_QUERY == reply_code && ((link_info *)lnk->usr)->query_is_queryget)
				v->mvtype = 0; /* force undefined to distinguish $Q returning "" from value of QUERYGET being 0 */
			return;
		}
	}
	assert(CMMS_R_GET == reply_code || CMMS_R_QUERY == reply_code && ((link_info *)lnk->usr)->query_is_queryget && 1 < len);
	CM_GET_SHORT(len, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += sizeof(unsigned short);
	assert(ptr >= stringpool.base && ptr + len < stringpool.top); /* incoming message is in stringpool */
	v->mvtype = MV_STR;
	v->str.len = len;
	v->str.addr = (char *)stringpool.free;	/* we don't need the reply msg anymore, can overwrite reply */
	memcpy(v->str.addr, ptr, len);		/* so that we don't leave a gaping hole in the stringpool */
	stringpool.free += len;
	return;
}
