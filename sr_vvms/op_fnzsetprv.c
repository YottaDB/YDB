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

#include "prvdef.h"
#include <jpidef.h>
#include <ssdef.h>

#include "stringpool.h"
#include "min_max.h"
#include "gtm_caseconv.h"
#include "op.h"

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
	{ 4, "ACNT", PRV$V_NOACNT },		{ 8, "ALLSPOOL", PRV$V_ALLSPOOL },
	{ 6, "BUGCHK", PRV$V_BUGCHK },
	{ 6, "BYPASS", PRV$V_BYPASS },		{ 6, "CMEXEC", PRV$V_CMEXEC },
	{ 6, "CMKRNL", PRV$V_CMKRNL },		{ 6, "DETACH", PRV$V_DETACH },
	{ 8, "DIAGNOSE", PRV$V_DIAGNOSE },	{ 9, "DOWNGRADE", PRV$V_DOWNGRADE },
	{ 7, "EXQUOTA", PRV$V_EXQUOTA },	{ 5, "GROUP", PRV$V_GROUP },
	{ 6, "GRPNAM", PRV$V_GRPNAM },		{ 6, "GRPPRV", PRV$V_GRPPRV },
	{ 6, "LOG_IO", PRV$V_LOG_IO },		{ 5, "MOUNT", PRV$V_MOUNT },
	{ 6, "NETMBX", PRV$V_NETMBX },
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
	{ 0 , 2 }, { 2 , 4 }, { 4 , 6 }, { 6 , 9 }, { 9 , 10 }, { 0 , 0 }, { 10 , 13 }, { 0 , 0 },
	{ 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 13 , 14 }, { 14 , 15 }, { 15 , 16 }, { 16 , 17 },
	{ 17, 24 }, { 0 , 0 }, { 24 , 25 }, { 25 , 34 }, { 34 , 36 }, { 36 , 37 }, { 37 , 38 },
	{ 38 , 39 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }
};

#define MIN_INDEX 0
#define MAX_INDEX 25
#define MAX_STPRV_LEN 512
#define QUAD_STR 8
#define is_priv_set(a,b) ((b[a/QUAD_STR] & (1 << (a % QUAD_STR))) ? TRUE : FALSE)
#define set_bit(a,b) (b[a/QUAD_STR] |= (1 << (a % QUAD_STR)))

static unsigned char priv_disable = 0;
static unsigned char priv_enable = 1;
static int4 cur_priv_code = JPI$_CURPRIV;
static unsigned char permanent = TRUE;

GBLREF spdesc stringpool;

void op_fnzsetprv(mval *prv,mval *ret)
{
int4 		priv_bit, status ;
short 		index, slot, last_slot, prv_len, out_len;
unsigned char	cur_priv[QUAD_STR], auth_priv[QUAD_STR], enable[QUAD_STR], disable[QUAD_STR], set_priv, priv_set;
char		*str_end, *str_cur, *prv_start;
char		buf[MAX_STPRV_LEN] ;
error_def	(ERR_ZSETPRVARGBAD);
error_def	(ERR_ZSETPRVSYNTAX);

assert(stringpool.free >= stringpool.base);
assert(stringpool.top >= stringpool.free);
MV_FORCE_STR(prv);
if (prv->str.len == 0)
	rts_error(VARLSTCNT(4) ERR_ZSETPRVARGBAD,2,4,"Null");
ENSURE_STP_FREE_SPACE(MAX_STPRV_LEN);
if ((status = lib$getjpi(	 &cur_priv_code
				,0 ,0
				,cur_priv
				,0  ,0	)) != SS$_NORMAL)
{	rts_error(VARLSTCNT(1)  status );
}
prv_len = MIN(prv->str.len,MAX_STPRV_LEN) ;
lower_to_upper(buf,prv->str.addr,prv_len) ;
str_cur = prv_start = str_end = buf ;
str_end += prv_len;
ret->str.addr = stringpool.free;
ret->mvtype = MV_STR;
out_len = 0;
memset(enable,'\0',QUAD_STR);
memset(disable,'\0',QUAD_STR);
while (prv_start < str_end)
{	if (!memcmp(str_cur,"NO",2))
	{	str_cur += 2;
		set_priv = FALSE;
		prv_start = str_cur;
	}
	else
	{	set_priv = TRUE;
	}
	while (*str_cur != ',' && str_cur != str_end)
	{	str_cur++;
	}
	prv_len = str_cur - prv_start;
	if ((index = *prv_start - 'A') < MIN_INDEX || index > MAX_INDEX
		|| !(last_slot = prv_index[ index ].last_index) )
	{	rts_error(VARLSTCNT(4) ERR_ZSETPRVARGBAD,2,prv_len,prv_start);
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
	{	rts_error(VARLSTCNT(4) ERR_ZSETPRVARGBAD,2,prv_len,prv_start);
	}
	assert(stringpool.top >= stringpool.free);
	if( !is_priv_set(priv_bit,cur_priv) )
	{	memcpy(stringpool.free,"NO",2);
		stringpool.free += 2;
	}
	memcpy(stringpool.free,prv_start,prv_len);
	stringpool.free += prv_len;
	if ( set_priv )
	{	set_bit(priv_bit,enable);
	}
	else
	{	set_bit(priv_bit,disable);
	}
	assert(stringpool.top >= stringpool.free);
	if (str_cur++ == str_end)
	{	break;
	}
	if (str_cur == str_end)
	{	rts_error(VARLSTCNT(1) ERR_ZSETPRVSYNTAX);
	}
	prv_start = str_cur;
	*stringpool.free++ = ',';
}
ret->str.len = (char *) stringpool.free - ret->str.addr;
if ((status = sys$setprv(	 priv_disable
				,disable
				,permanent
				,0	)) != SS$_NORMAL)
{	rts_error(VARLSTCNT(1)  status );
}
if ((status = sys$setprv(	 priv_enable
				,enable
				,permanent
				,0	)) != SS$_NORMAL)
{
	if ((status & 1)==0)
	{
		rts_error(VARLSTCNT(1)  status );
	}
}
return ;
}

