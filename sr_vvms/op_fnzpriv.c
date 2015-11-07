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

#include "prvdef.h"
#include <jpidef.h>
#include <ssdef.h>

#include "min_max.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "mvalconv.h"

typedef	struct
{	char len;
	char name[20];
	char bit_number;
}prv_struct;

typedef struct
{	char index;
	char last_index;
}ind_struct;

static readonly prv_struct priv_table[] =
{
	{ 8, "ALLSPOOL", PRV$V_ALLSPOOL },	{ 6, "BUGCHK", PRV$V_BUGCHK },
	{ 6, "BYPASS", PRV$V_BYPASS },		{ 6, "CMEXEC", PRV$V_CMEXEC },
	{ 6, "CMKRNL", PRV$V_CMKRNL },		{ 6, "DETACH", PRV$V_DETACH },
	{ 8, "DIAGNOSE", PRV$V_DIAGNOSE },	{ 9, "DOWNGRADE", PRV$V_DOWNGRADE },
	{ 7, "EXQUOTA", PRV$V_EXQUOTA },	{ 5, "GROUP", PRV$V_GROUP },
	{ 6, "GRPNAM", PRV$V_GRPNAM },		{ 6, "GRPPRV", PRV$V_GRPPRV },
	{ 6, "LOG_IO", PRV$V_LOG_IO },		{ 5, "MOUNT", PRV$V_MOUNT },
	{ 6, "NETMBX", PRV$V_NETMBX },		{ 6, "NOACNT", PRV$V_NOACNT },
	{ 4, "OPER", PRV$V_OPER },		{ 6, "PFNMAP", PRV$V_PFNMAP },
	{ 6, "PHY_IO", PRV$V_PHY_IO },		{ 6, "PRMCEB", PRV$V_PRMCEB },
	{ 6, "PRMGBL", PRV$V_PRMGBL },		{ 6, "PRMJNL", PRV$V_PRMJNL },
	{ 6, "PRMMBX", PRV$V_PRMMBX },		{ 6, "PSWAPM", PRV$V_PSWAPM },
	{ 7, "READALL", PRV$V_READALL },	{ 8, "SECURITY", PRV$V_SECURITY },
	{ 6, "SETPRI", PRV$V_SETPRI },		{ 6, "SETPRV", PRV$V_SETPRV },
	{ 5, "SHARE", PRV$V_SHARE },		{ 5, "SHMEM", PRV$V_SHMEM },
	{ 6, "SYSGBL", PRV$V_SYSGBL },		{ 6, "SYSLCK", PRV$V_SYSLCK },
	{ 6, "SYSNAM", PRV$V_SYSNAM },		{ 6, "SYSPRV", PRV$V_SYSPRV },
	{ 6, "TMPJNL", PRV$V_TMPJNL },		{ 6, "TMPMBX", PRV$V_TMPMBX },
	{ 7, "UPGRADE", PRV$V_UPGRADE },	{ 6, "VOLPRO", PRV$V_VOLPRO },
	{ 5, "WORLD", PRV$V_WORLD}
};

static readonly ind_struct prv_index[] =
{
	{ 0 , 1 }, { 1 , 3 }, { 3 , 5 }, { 5 , 8 }, { 8 , 9 }, { 0 , 0 }, { 9 , 12 }, { 0 , 0 },
	{ 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 12 , 13 }, { 13 , 14 }, { 14 , 16 }, { 16 , 17 },
	{ 17, 24 }, { 0 , 0 }, { 24 , 25 }, { 25 , 34 }, { 34 , 36 }, { 36 , 37 }, { 37 , 38 },
	{ 38 , 39 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }
};

static int4 jpi_code = JPI$_CURPRIV;

#define MAX_KW_LEN  128
#define MIN_INDEX 0
#define MAX_INDEX 25

void op_fnzpriv(mval *prv,mval *ret)
{
uint4 	status;
int4 		priv_bit;
short 		index, slot, last_slot, prv_len;
short 		buflen ;
unsigned char 	out_value[8];
char		buf[MAX_KW_LEN] ;
char 		*str_end,*str_cur,*prv_start;
error_def	(ERR_ZPRIVARGBAD);
error_def	(ERR_ZPRIVSYNTAXERR);

MV_FORCE_STR(prv);
ret->mvtype = MV_NM;
if (prv->str.len == 0)
{	rts_error(VARLSTCNT(4) ERR_ZPRIVARGBAD,2,4,"Null");
}
if ((status = lib$getjpi(	 &jpi_code
				,0 ,0
				,out_value
				,0 ,0	)) != SS$_NORMAL)
{	rts_error(VARLSTCNT(1)  status );
}

buflen = MIN(prv->str.len,MAX_KW_LEN) ;
lower_to_upper(buf,prv->str.addr,buflen) ;
str_cur = prv_start = str_end = buf ;
str_end += buflen ;

while (prv_start < str_end)
{
	while (*str_cur != ',' && str_cur != str_end)
	{	str_cur++;
	}
	prv_len = str_cur - prv_start;
	if ((index = *prv_start - 'A') < MIN_INDEX || index > MAX_INDEX
		|| !(last_slot = prv_index[ index ].last_index) )
	{	rts_error(VARLSTCNT(4) ERR_ZPRIVARGBAD,2,prv_len,prv_start);
	}
	slot = prv_index[ index ].index;
	priv_bit = -1;
	for ( ; slot < last_slot ; slot++)
	{	if (prv_len == priv_table[slot].len && !memcmp(prv_start,priv_table[ slot ].name,prv_len))
		{	priv_bit = priv_table[slot].bit_number;
			break;
		}
	}
	if (priv_bit == -1)
	{	rts_error(VARLSTCNT(4) ERR_ZPRIVARGBAD,2,prv_len,prv_start);
	}
	if ( !lib$bbssi(&priv_bit,out_value) )
	{	ret->m[1] = 0 ;
		return;
	}
	if (str_cur++ == str_end)
	{	break;
	}
	if (str_cur == str_end)
	{	rts_error(VARLSTCNT(1) ERR_ZPRIVSYNTAXERR);
	}
	prv_start = str_cur;
}
MV_FORCE_MVAL(ret,1);
}
