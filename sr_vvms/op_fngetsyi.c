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

#include <syidef.h>
#include <descrip.h>
#include "stringpool.h"
#include <ssdef.h>
#include "op_fn.h"
#include "min_max.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "mvalconv.h"

#define MAX_SYI_STRLEN 32
#define MAX_KW_LEN     20
#define MAX_ND_LEN     64
#define ZINDX 25				/* index for char 'Z' */
#define ALFA(k)		('A'<=k && k<='Z')
#define PROPER(status) 	if (status != SS$_NORMAL) { rts_error(VARLSTCNT(1) status) ;}

typedef struct
{
	char		kwlen ;			/* item keyword length 	*/
	char		kw[MAX_KW_LEN] ;	/* item keyword		*/
	unsigned short	code ;			/* syi code		*/
	unsigned char	valsiz ;		/* size of item value   */
	unsigned char	valtyp ;		/* item type		*/
}syi_item ;

static readonly syi_item kw_syi[] =
{
	 {13,"ACTIVECPU_CNT",	SYI$_ACTIVECPU_CNT,	4,	MV_NM}
	,{12,"AVAILCPU_CNT",	SYI$_AVAILCPU_CNT,	4,	MV_NM}
	,{ 8,"BOOTTIME",	SYI$_BOOTTIME	,	8,	MV_STR}
	,{18,"CHARACTER_EMULATED",SYI$_CHARACTER_EMULATED,1,	MV_NM}
	,{14,"CLUSTER_EVOTES",	SYI$_CLUSTER_EVOTES,	2,	MV_NM}
	,{14,"CLUSTER_FSYSID",	SYI$_CLUSTER_FSYSID,	6,	MV_NM}
	,{13,"CLUSTER_FTIME",	SYI$_CLUSTER_FTIME,	8,	MV_STR}
	,{14,"CLUSTER_MEMBER",	SYI$_CLUSTER_MEMBER,	1,	MV_NM}
	,{13,"CLUSTER_NODES",	SYI$_CLUSTER_NODES,	2,	MV_NM}
	,{14,"CLUSTER_QUORUM",	SYI$_CLUSTER_QUORUM,	2,	MV_STR}
	,{13,"CLUSTER_VOTES",	SYI$_CLUSTER_VOTES,	2,	MV_STR}
	,{15,"CONTIG_GBLPAGES",	SYI$_CONTIG_GBLPAGES,	4,	MV_NM}
	,{ 3,"CPU",		SYI$_CPU,		4,	MV_STR}
	,{16,"DECIMAL_EMULATED",SYI$_DECIMAL_EMULATED,	1,	MV_NM}
	,{16,"D_FLOAT_EMULATED",SYI$_D_FLOAT_EMULATED,	1,	MV_NM}
	,{15,"ERRORLOGBUFFERS",	SYI$_ERRORLOGBUFFERS,	2,	MV_NM}
	,{16,"F_FLOAT_EMULATED",SYI$_F_FLOAT_EMULATED,	1,	MV_NM}
	,{13,"FREE_GBLPAGES",	SYI$_FREE_GBLPAGES,	4,	MV_NM}
	,{13,"FREE_GBLSECTS",	SYI$_FREE_GBLSECTS,	4,	MV_NM}
	,{16,"G_FLOAT_EMULATED",SYI$_G_FLOAT_EMULATED,	1,	MV_NM}
	,{ 8,"HW_MODEL",	SYI$_HW_MODEL,		2,	MV_NM}
	,{ 7,"HW_NAME", 	SYI$_HW_NAME,		31,	MV_STR}
	,{14,"H_FLOAT_EMULATED",SYI$_H_FLOAT_EMULATED,	1,	MV_NM}
	,{ 9,"NODE_AREA",	SYI$_NODE_AREA,		4,	MV_STR}
	,{ 9,"NODE_CSID",	SYI$_NODE_CSID,		4,	MV_NM}
	,{11,"NODE_EVOTES",	SYI$_NODE_EVOTES,	2,	MV_NM}
	,{11,"NODE_HWVERS",	SYI$_NODE_HWVERS,	12,	MV_STR}
	,{11,"NODE_NUMBER",	SYI$_NODE_NUMBER,	4,	MV_STR}
	,{11,"NODE_QUORUM",	SYI$_NODE_QUORUM,	2,	MV_NM}
	,{13,"NODE_SWINCARN",	SYI$_NODE_SWINCARN,	8,	MV_STR}
	,{11,"NODE_SWTYPE",	SYI$_NODE_SWTYPE,	4,	MV_STR}
	,{11,"NODE_SWVERS",	SYI$_NODE_SWVERS,	4,	MV_STR}
	,{13,"NODE_SYSTEMID",	SYI$_NODE_SYSTEMID,	6,	MV_STR}
	,{10,"NODE_VOTES",	SYI$_NODE_VOTES,	2,	MV_NM}
	,{ 8,"NODENAME",	SYI$_NODENAME,		15,	MV_STR}
	,{13,"PAGEFILE_FREE",	SYI$_PAGEFILE_FREE,	4,	MV_NM}
	,{13,"PAGEFILE_PAGE",	SYI$_PAGEFILE_PAGE,	4,	MV_NM}
	,{10,"SCS_EXISTS",	SYI$_SCS_EXISTS,	4,	MV_NM}
	,{ 3,"SID",		SYI$_SID,		4,	MV_STR}
	,{13,"SWAPFILE_FREE",	SYI$_SWAPFILE_FREE,	4,	MV_NM}
	,{13,"SWAPFILE_PAGE",	SYI$_SWAPFILE_PAGE,	4,	MV_NM}
	,{ 7,"VERSION",		SYI$_VERSION,		8,	MV_STR}
	,{ 4,"XCPU",		SYI$_XCPU,		4,	MV_STR}
	,{ 4,"XSID",		SYI$_XSID,		4,	MV_STR}
} ;

static readonly unsigned char kw_syi_index[] =
{
	0,2,3,13,14,15,18,19,22,22,22,22,22,22,35,35,37,37,37,41,41,41,42,42,44,44,44
} ;

GBLREF spdesc 	stringpool;

void op_fngetsyi(mval *keyword,mval *node,mval *ret)
{
	error_def	(ERR_BADSYIPARAM) ;
	error_def	(ERR_AMBISYIPARAM) ;
	char		bufw[MAX_KW_LEN] ;
	char		bufn[MAX_ND_LEN] ;
	short		bufwlen ;
	short		bufnlen ;
	$DESCRIPTOR	(res_str,stringpool.free) ;
	$DESCRIPTOR	(dnode,bufn) ;
	uint4	res_val[2] ;
	short		res_len ;
	unsigned char	*pnode ;
	unsigned char	k,j ;
	bool		match ;
	int4		item  ;
	uint4 	status ;
	mval		*tmp ;

	assert(stringpool.top >= stringpool.free);
	assert(stringpool.free >= stringpool.base);
	MV_FORCE_STR(node);
	MV_FORCE_STR(keyword);
	ENSURE_STP_FREE_SPACE(MAX_SYI_STRLEN);
	bufwlen = MIN(keyword->str.len,SIZEOF(bufw)) ;
	bufnlen = MIN(node->str.len,SIZEOF(bufn)) ;
	lower_to_upper(bufw,keyword->str.addr,bufwlen) ;
	lower_to_upper(bufn,node->str.addr,bufnlen) ;
	dnode.dsc$w_length = bufnlen ;
	pnode = ((node->str.len != 0) ? &dnode : NULL) ;
	k = bufw[0] ;
	k = ( ALFA(k) ? k - 'A' : ZINDX ) ;
	match = FALSE ;
	for ( j = kw_syi_index[k] ; !match && j!=kw_syi_index[k+1] ; j++ )
	{
		match = (keyword->str.len<=kw_syi[j].kwlen && !memcmp(bufw,kw_syi[j].kw,keyword->str.len)) ;
	}
	if (!match)
	{
		rts_error(VARLSTCNT(4) ERR_BADSYIPARAM,2,keyword->str.len,keyword->str.addr) ;
		return ;
	}
	else
	{
		if (j!=kw_syi_index[k+1] && keyword->str.len<=kw_syi[j].kwlen && !memcmp(bufw,kw_syi[j].kw,keyword->str.len))
		{
			rts_error(VARLSTCNT(4) ERR_AMBISYIPARAM,2,keyword->str.len,keyword->str.addr) ;
			return ;
		}
		else
		{
			item = kw_syi[j-1].code ;
			res_str.dsc$w_length = MAX_SYI_STRLEN ;
			res_str.dsc$a_pointer = stringpool.free ;
			status = lib$getsyi(&item,res_val,&res_str,&res_len,0,pnode) ;
			PROPER(status) ;
			ret->mvtype  = kw_syi[j-1].valtyp ;
		}
	}
	if (item != SYI$_BOOTTIME && item != SYI$_CLUSTER_FTIME)
	{
		ret->str.addr = stringpool.free ;
		ret->str.len  = res_len ;
		stringpool.free += res_len ;
	}
	else
	{
		int4 day ;
		int4 sec ;
		status = lib$day(&day,res_val,&sec) ;
		PROPER(status) ;
		day += DAYS;
		sec /= CENTISECONDS;
		ret->str.addr = stringpool.free;
		stringpool.free = i2s(&day) ;
		*stringpool.free++ = ',' ;
		stringpool.free = i2s(&sec) ;
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
	}
	if (MV_IS_NUMERIC(ret))
	{
		i2mval(ret,res_val[0]) ;
	}
	assert (stringpool.top >= stringpool.free);
}
