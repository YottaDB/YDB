/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <unistd.h>
#include "stringpool.h"
#include "gtm_times.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "mvalconv.h"

#define MAX_KEY 16
#define MAX_STR 16

GBLREF int	errno ;
GBLREF spdesc 	stringpool ;

typedef char	keyword[MAX_KEY] ;
static keyword	key[6]= {"CPUTIM","CSTIME","CUTIME","STIME","UTIME",""} ;
enum 	kwind {kw_cputim, kw_cstime, kw_cutime, kw_stime, kw_utime, kw_end};

void op_fngetjpi(mint *jpid,mval *kwd,mval *ret)
{
	error_def	(ERR_BADJPIPARAM);
	struct tms	t ;
	int4		info ;
	int		k ;

	assert (stringpool.free >= stringpool.base);
	assert (stringpool.top >= stringpool.free);
	if (stringpool.top - stringpool.free < MAX_STR)
	{	stp_gcol( MAX_STR );
	}
	MV_FORCE_STR(kwd);
	if (kwd->str.len == 0)
	{	rts_error(VARLSTCNT(4) ERR_BADJPIPARAM,2,4,"Null");
	}
	if (kwd->str.len > MAX_KEY)
	{	rts_error(VARLSTCNT(4) ERR_BADJPIPARAM,2,kwd->str.len,kwd->str.addr );
	}
	lower_to_upper((uchar_ptr_t)key[kw_end],(uchar_ptr_t)kwd->str.addr,kwd->str.len);

	k=kw_cputim ;
	while (memcmp(key[kw_end],key[k],kwd->str.len)!=0)  k++ ;

	if (times(&t)==-1)
	{
		rts_error(VARLSTCNT(1) errno);
		return ;
	}
	switch(k)
	{
	case kw_cputim:	info = t.tms_utime+t.tms_stime+t.tms_cutime+t.tms_cstime ; break ;
	case kw_cstime:	info = t.tms_cstime ; break ;
	case kw_cutime:	info = t.tms_cutime ; break ;
	case kw_stime :	info = t.tms_stime ; break ;
	case kw_utime :	info = t.tms_utime ; break ;
	case kw_end   :
	default	   :
			rts_error(VARLSTCNT(4) ERR_BADJPIPARAM,2,kwd->str.len,kwd->str.addr );
			return ;
	}
	info = (info * 100) / sysconf(_SC_CLK_TCK);	/* Convert to standard 100 ticks per second */
	i2mval(ret,info) ;
}
