/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
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
#include "format_targ_key.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_key		*gv_altkey;
GBLREF	spdesc		stringpool;
GBLREF	bool		undef_inhibit;

error_def(ERR_BADSRVRNETMSG);
error_def(ERR_UNIMPLOP);
error_def(ERR_TEXT);
error_def(ERR_GVIS);

void gvcmz_doop(unsigned char query_code, unsigned char reply_code, mval *v)
{
	unsigned char	*ptr;
	short		len, temp_short;
	int4		status, max_reply_len;
	struct CLB	*lnk;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	unsigned short	srv_buff_size;

	lnk = gv_cur_region->dyn.addr->cm_blk;
	if (!((link_info *)lnk->usr)->server_supports_long_names && (PRE_V5_MAX_MIDENT_LEN < strlen((char *)gv_currkey->base)))
	{
		end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
		rts_error(VARLSTCNT(14) ERR_UNIMPLOP, 0,
					ERR_TEXT, 2,
					LEN_AND_LIT("GT.CM server does not support global names longer than 8 characters"),
					ERR_GVIS, 2, end - buff, buff,
					ERR_TEXT, 2, DB_LEN_STR(gv_cur_region));
	}
	lnk->ast = 0;	/* all database queries are sync */
	lnk->cbl = 1 + /* HDR */
		   gv_currkey->end + /* key */
		   SIZEOF(unsigned short) + /* gv_key.top */
		   SIZEOF(unsigned short) + /* gv_key.end */
		   SIZEOF(unsigned short) + /* gv_key.prev */
		   SIZEOF(unsigned char) + /* gv_key.base */
		   SIZEOF(unsigned char) + /* regnum */
		   SIZEOF(unsigned short); /* size for variable len SUBSC */
	/* the current GT.CM maximum message buffer length is bounded by the size of a short which is 64K. but the
	 * calculation below of lnk->cbl and max_reply_len takes into account the fact that the value that is sent in as
	 * input or read in from the server side can be at most MAX_DBSTRLEN in size. therefore, there is a dependency
	 * that MAX_DBSTRLEN always be less than the GT.CM maximum message buffer length. to ensure this is always the
	 * case, the following assert is added so that whenever MAX_DBSTRLEN is increased, we will fail this assert
	 * and reexamine the code below.
	 */
	assert(SIZEOF(lnk->cbl) == 2);	/* assert it is a short. when it becomes a uint4 the assert can be removed
					 * if the macro CM_MAX_BUF_LEN (used below) is changed appropriately */
	assert(MAX_DBSTRLEN == (32 * 1024 - 1));
	if (CMMS_Q_PUT == query_code || CMMS_Q_INCREMENT == query_code)
	{
		if (CMMS_Q_INCREMENT == query_code)
		{ /* 1-byte boolean value of "undef_inhibit" passed to the server for $INCREMENT
		   although, effective V5.0-000, undef_inhibit is no longer relevant as $INCREMENT()
		   implicitly does a $GET() on the global. We keep this byte to ensure compatibility
		   with V5.0-FT01 */
			lnk->cbl++;
		}
		assert((uint4)lnk->cbl + SIZEOF(unsigned short) + (uint4)MAX_DBSTRLEN <= (uint4)CM_MAX_BUF_LEN);
		lnk->cbl += (SIZEOF(unsigned short) + v->str.len); /* VALUE + length */
	}
	if ((CMMS_Q_GET == query_code)
			|| (CMMS_Q_INCREMENT == query_code)
			|| (CMMS_Q_QUERY == query_code) && ((link_info *)lnk->usr)->query_is_queryget)
		max_reply_len = lnk->mbl + SIZEOF(unsigned short) + MAX_DBSTRLEN; /* can't predict the length of data value */
	else
		max_reply_len = lnk->mbl;
	assert(max_reply_len <= (int4)CM_MAX_BUF_LEN);
	ENSURE_STP_FREE_SPACE(max_reply_len);
	lnk->mbf = stringpool.free;
	ptr = lnk->mbf;
	*ptr++ = query_code;
	/* temp_short = gv_currkey->end + SIZEOF(gv_key) + 1; */
	temp_short = gv_currkey->end + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(char) + 1;
	CM_PUT_SHORT(ptr, temp_short, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += SIZEOF(short);
	*ptr++ = gv_cur_region->cmx_regnum;
	CM_PUT_SHORT(ptr, gv_currkey->top, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += SIZEOF(short);
	CM_PUT_SHORT(ptr, gv_currkey->end, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += SIZEOF(short);
	CM_PUT_SHORT(ptr, gv_currkey->prev, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += SIZEOF(short);
	memcpy(ptr, gv_currkey->base, gv_currkey->end + 1);
	if (CMMS_Q_PUT == query_code || CMMS_Q_INCREMENT == query_code)
	{
		ptr += gv_currkey->end + 1;
		temp_short = (short)v->str.len;
		assert((int4)temp_short == v->str.len); /* short <- int4 assignment lossy? */
		CM_PUT_SHORT(ptr, temp_short, ((link_info *)(lnk->usr))->convert_byteorder);
		ptr += SIZEOF(short);
		memcpy(ptr,v->str.addr, v->str.len);
		if (CMMS_Q_INCREMENT == query_code)
		{ /* UNDEF flag is no longer relevant, but set the flag to ensure compatibility with V5.0-FT01 */
			assert(SIZEOF(undef_inhibit) == 1);
			ptr[v->str.len] = (unsigned char)undef_inhibit;
		}
	}
	status = cmi_write(lnk);
	if (CMI_ERROR(status))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(query_code, status);
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
	if (CMMS_Q_PUT == query_code || CMMS_Q_ZWITHDRAW == query_code || CMMS_Q_KILL == query_code)
	{
		if (reply_code != *ptr)
		{
			if (*ptr != CMMS_E_ERROR)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			gvcmz_errmsg(lnk,FALSE);
		}
		return;
	}
	if (reply_code != *ptr)
	{
		if ((CMMS_R_UNDEF != *ptr) || ((CMMS_Q_GET != query_code) && (CMMS_Q_INCREMENT != query_code)))
		{
			if (CMMS_E_ERROR != *ptr)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			gvcmz_errmsg(lnk, FALSE);
		}
		if (CMMS_Q_INCREMENT == query_code)
			v->mvtype = 0;	/* set the result to be undefined */
		return;
	}
	ptr++;
	if (CMMS_R_DATA == reply_code)
	{
		CM_GET_SHORT(temp_short, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
		if (1 != temp_short)
			rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
		ptr += SIZEOF(short);
		status = *ptr;	/* Temp assignment to status gets rid of compiler warning in MV_FORCE_MVAL macro */
		MV_FORCE_MVAL(v, status);
		return;
	}
	if (reply_code == CMMS_R_PREV || reply_code == CMMS_R_QUERY || reply_code == CMMS_R_ORDER)
	{
		CM_GET_SHORT(len, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
		ptr += SIZEOF(short);
		if (1 == len)
		{
			MV_FORCE_MVAL(v, 0);
		} else
		{
			if (*ptr++ != gv_cur_region->cmx_regnum)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
#ifdef DEBUG
			CM_GET_USHORT(srv_buff_size, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
			assert(srv_buff_size == gv_altkey->top);
			/* Check gv_altkey has enough size allocated for the data to be copied*/
			/*gv_init_reg would have got the correct key length from server*/
			assert(srv_buff_size >= (len - 1 - SIZEOF(unsigned short) - SIZEOF(unsigned short) -
							SIZEOF(unsigned short)));
#endif
			ptr += SIZEOF(unsigned short);
			CM_GET_USHORT(gv_altkey->end, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
			DEBUG_ONLY(assert(gv_altkey->end <= gv_altkey->top));
  			ptr += SIZEOF(unsigned short);
			CM_GET_USHORT(gv_altkey->prev, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
			ptr += SIZEOF(unsigned short);
			memcpy(gv_altkey->base, ptr, len - 1 - SIZEOF(unsigned short) - SIZEOF(unsigned short) -
						     SIZEOF(unsigned short));
			ptr += (len - 1 - SIZEOF(unsigned short) - SIZEOF(unsigned short) - SIZEOF(unsigned short));
			MV_FORCE_MVAL(v, 1);
		}
		if (CMMS_R_QUERY != reply_code || 1 == len || !((link_info *)lnk->usr)->query_is_queryget)
		{
			if (CMMS_R_QUERY == reply_code && ((link_info *)lnk->usr)->query_is_queryget)
				v->mvtype = 0; /* force undefined to distinguish $Q returning "" from value of QUERYGET being 0 */
			return;
		}
	}
	assert(CMMS_R_GET == reply_code
		|| CMMS_R_INCREMENT == reply_code
		|| CMMS_R_QUERY == reply_code && ((link_info *)lnk->usr)->query_is_queryget && 1 < len);
	CM_GET_SHORT(len, ptr, ((link_info *)(lnk->usr))->convert_byteorder);
	ptr += SIZEOF(unsigned short);
	assert(ptr >= stringpool.base && ptr + len < stringpool.top); /* incoming message is in stringpool */
	v->mvtype = MV_STR;
	v->str.len = len;
	v->str.addr = (char *)stringpool.free;	/* we don't need the reply msg anymore, can overwrite reply */
	memmove(v->str.addr, ptr, len);		/* so that we don't leave a gaping hole in the stringpool */
	stringpool.free += len;
	return;
}
