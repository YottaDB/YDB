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

#include "gtm_string.h"

#include "stringpool.h"
#include <ssdef.h>
#include <descrip.h>
#include <jpidef.h>
#include "gtm_caseconv.h"
#include "op.h"
#include "mvalconv.h"

#define	MAX_KEY_LEN	20	/* maximum length across all keywords in the jpi_param_table[] array as well as "ISPROCALIVE" */

typedef struct
{	char	len;
	char	name[MAX_KEY_LEN];
	short int item_code;
}jpi_tab;

typedef struct
{	char	index;
	char	len;
}jpi_ind;

typedef struct
{	int4 x;
	int4 y;
}out_struct;

static readonly jpi_tab jpi_param_table[] =
{
	{ 7, "ACCOUNT" , JPI$_ACCOUNT },	{ 6, "APTCNT" , JPI$_APTCNT },
	{ 6, "ASTACT" , JPI$_ASTACT},		{ 6, "ASTCNT" , JPI$_ASTCNT },
	{ 5, "ASTEN" , JPI$_ASTEN }, 		{ 5, "ASTLM" , JPI$_ASTLM },
	{ 7, "AUTHPRI" , JPI$_AUTHPRI },	{ 8, "AUTHPRIV" , JPI$_AUTHPRIV },
	{ 6, "BIOCNT" , JPI$_BIOCNT },		{ 5, "BIOLM" , JPI$_BIOLM },
	{ 5, "BUFIO" , JPI$_BUFIO },		{ 6, "BYTCNT" , JPI$_BYTCNT },
	{ 5,"BYTLM" , JPI$_BYTLM },		{ 7, "CLINAME" , JPI$_CLINAME },
	{ 6, "CPULIM" , JPI$_CPULIM },		{ 6, "CPUTIM" , JPI$_CPUTIM },
	{ 11, "CREPRCFLAGS" , JPI$_CREPRC_FLAGS },
	{ 7, "CURPRIV" , JPI$_CURPRIV },	{ 5, "DFPFC" , JPI$_DFPFC },
	{ 7, "DFWSCNT" , JPI$_DFWSCNT },	{ 6, "DIOCNT" , JPI$_DIOCNT },
	{ 5, "DIOLM" , JPI$_DIOLM },		{ 5, "DIRIO" , JPI$_DIRIO },
	{ 4, "EFCS" , JPI$_EFCS }, 		{ 4, "EFCU" , JPI$_EFCU },
	{ 4, "EFWM" , JPI$_EFWM },		{ 6, "ENQCNT" , JPI$_ENQCNT },
	{ 5, "ENQLM" , JPI$_ENQLM },		{ 6, "EXCVEC" , JPI$_EXCVEC },
	{ 6, "FILCNT" , JPI$_FILCNT },		{ 5, "FILLM" , JPI$_FILLM },
	{ 8, "FINALEXC" , JPI$_FINALEXC },	{ 7, "FREP0VA" , JPI$_FREP0VA },
	{ 7, "FREP1VA" , JPI$_FREP1VA },	{ 9, "FREPTECNT" , JPI$_FREPTECNT },
	{ 6, "GPGCNT" , JPI$_GPGCNT },		{ 3, "GRP" , JPI$_GRP },
	{ 10, "IMAGECOUNT" , JPI$_IMAGECOUNT },	{ 8, "IMAGNAME" , JPI$_IMAGNAME },
	{ 8, "IMAGPRIV" , JPI$_IMAGPRIV },	{ 9, "JOBPRCCNT" , JPI$_JOBPRCCNT },
	{ 7, "JOBTYPE" , JPI$_JOBTYPE },
	{ 8, "LOGINTIM" , JPI$_LOGINTIM },	{ 10, "MASTER_PID" , JPI$_MASTER_PID },
	{ 9, "MAXDETACH" , JPI$_MAXDETACH },	{ 7, "MAXJOBS" , JPI$_MAXJOBS },
	{ 3, "MEM" , JPI$_MEM },		{ 4, "MODE" ,JPI$_MODE },
	{ 7, "MSGMASK" , JPI$_MSGMASK },	{ 5, "OWNER" , JPI$_OWNER },
	{ 8, "PAGEFLTS" , JPI$_PAGEFLTS },	{ 9, "PAGFILCNT" , JPI$_PAGFILCNT },
	{ 9, "PAGFILLOC" , JPI$_PAGFILLOC },
	{ 9, "PGFLQUOTA" , JPI$_PGFLQUOTA },	{ 8, "PHDFLAGS" , JPI$_PHDFLAGS },
	{ 3, "PID" , JPI$_PID },		{ 6, "PPGCNT" , JPI$_PPGCNT },
	{ 6, "PRCCNT" , JPI$_PRCCNT },		{ 5, "PRCLM" ,JPI$_PRCLM },
	{ 6, "PRCNAM" , JPI$_PRCNAM },		{ 3, "PRI" , JPI$_PRI },
	{ 4, "PRIB" , JPI$_PRIB },		{ 9, "PROCINDEX" , JPI$_PROC_INDEX },
	{ 8, "PROCPRIV" , JPI$_PROCPRIV },	{ 8, "SHRFILLM" , JPI$_SHRFILLM },
	{ 8, "SITESPEC" , JPI$_SITESPEC },
	{ 5, "STATE" , JPI$_STATE },		{ 3, "STS" , JPI$_STS },
	{ 9, "SWPFILLOC" , JPI$_SWPFILLOC },	{ 9, "TABLENAME" , JPI$_TABLENAME },
	{ 8, "TERMINAL" ,JPI$_TERMINAL },
	{ 4, "TMBU" , JPI$_TMBU },		{ 5, "TQCNT" , JPI$_TQCNT },
	{ 4, "TQLM" , JPI$_TQLM },		{ 8, "UAFFLAGS" , JPI$_UAF_FLAGS },
	{ 3, "UIC" , JPI$_UIC },
	{ 8, "USERNAME" ,JPI$_USERNAME },	{ 8, "VIRTPEAK" , JPI$_VIRTPEAK },
	{ 7, "VOLUMES" , JPI$_VOLUMES },	{ 6, "WSAUTH" , JPI$_WSAUTH },
	{ 9, "WSAUTHEXT" , JPI$_WSAUTHEXT },	{ 8, "WSEXTENT" , JPI$_WSEXTENT },
	{ 6, "WSPEAK" , JPI$_WSPEAK },		{ 7, "WSQUOTA" , JPI$_WSQUOTA },
	{ 6, "WSSIZE" , JPI$_WSSIZE }
};

static readonly jpi_ind jpi_index_table[] =
{
	{ 0 , 8 }, { 8 , 13 }, { 13 , 18 }, { 18 , 23 }, { 23 , 29 }, { 29 , 35 },
	{ 35 , 37 }, { 0 , 0 }, { 37 , 40 }, { 40 , 42 }, { 0, 0 }, { 42 , 43 },
	{ 43, 49 }, { 0 , 0 }, { 49 , 50 }, { 50 , 64 }, { 0 , 0 }, { 0 , 0 },
	{ 64 , 69 }, { 69 , 74 }, { 74 , 77 }, { 77 , 79 }, { 79 , 85 },
	{ 0 , 0 }, { 0 , 0 }, { 0 , 0 },
};

#define MAX_JPI_STRLEN 512
#define MIN_INDEX 0
#define MAX_INDEX 25

GBLREF spdesc stringpool;

void op_fngetjpi(mint jpid, mval *keyword, mval *ret)
{
	out_struct	out_quad;
	int4		out_long, jpi_code, pid;
	short		index, length, slot, last_slot, out_len;
	uint4		status;
	char		upcase[MAX_KEY_LEN];

	$DESCRIPTOR(out_string, "");
	error_def(ERR_BADJPIPARAM);

	assert (stringpool.free >= stringpool.base);
	assert (stringpool.top >= stringpool.free);
	ENSURE_STP_FREE_SPACE(MAX_JPI_STRLEN);
	MV_FORCE_STR(keyword);
	if (keyword->str.len == 0)
		rts_error(VARLSTCNT(4) ERR_BADJPIPARAM, 2, 4, "Null");
	if (keyword->str.len > MAX_KEY_LEN)
		rts_error(VARLSTCNT(4)  ERR_BADJPIPARAM, 2, keyword->str.len, keyword->str.addr );
	lower_to_upper((uchar_ptr_t)upcase, (uchar_ptr_t)keyword->str.addr, keyword->str.len);
	if ((index = upcase[0] - 'A') < MIN_INDEX || index > MAX_INDEX)
		rts_error(VARLSTCNT(4)  ERR_BADJPIPARAM, 2, keyword->str.len, keyword->str.addr );
	/* Before checking if it is a VMS JPI attribute, check if it is GT.M specific "ISPROCALIVE" attribute */
	if ((keyword->str.len == STR_LIT_LEN("ISPROCALIVE")) && !memcmp(upcase, "ISPROCALIVE", keyword->str.len))
	{
		out_long = (0 != jpid) ? is_proc_alive(jpid, 0) : 1;
		i2mval(ret, out_long);
		return;
	}
	/* Check if it is a VMS JPI attribute */
	slot = jpi_index_table[ index ].index;
	last_slot = jpi_index_table[ index ].len;
	jpi_code = 0;
	/* future enhancement:
	 * 	(i) since keywords are sorted, we can exit the for loop if 0 < memcmp.
	 * 	(ii) also, the current comparison relies on kwd->str.len which means a C would imply CPUTIM instead of CSTIME
	 * 		or CUTIME this ambiguity should probably be removed by asking for an exact match of the full keyword
	 */
	for ( ; slot < last_slot ; slot++ )
	{
		if (jpi_param_table[ slot ].len == keyword->str.len
			&& !(memcmp(jpi_param_table[ slot ].name, upcase, keyword->str.len)))
		{
			jpi_code = jpi_param_table[ slot ].item_code;
			break;
		}
	}
	if (!jpi_code)
		rts_error(VARLSTCNT(4)  ERR_BADJPIPARAM, 2, keyword->str.len, keyword->str.addr);
	assert (jpid >= 0);
	switch( jpi_code )
	{
	/* **** This is a fall through for all codes that require a string returned **** */
	case JPI$_ACCOUNT:
	case JPI$_AUTHPRIV:
	case JPI$_CLINAME:
	case JPI$_CURPRIV:
	case JPI$_IMAGNAME:
	case JPI$_IMAGPRIV:
	case JPI$_PRCNAM:
	case JPI$_PROCPRIV:
	case JPI$_TABLENAME:
	case JPI$_TERMINAL:
	case JPI$_USERNAME:
		out_string.dsc$a_pointer = stringpool.free;
		out_string.dsc$w_length = MAX_JPI_STRLEN;
		if ((status = lib$getjpi(	 &jpi_code
						,&jpid
						,0
						,0
						,&out_string
						,&out_len	)) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status ); /* need a more specific GTM error message here and below */
		}
		ret->str.addr = stringpool.free;
		ret->str.len = out_len;
		ret->mvtype = MV_STR;
		stringpool.free += out_len;
		assert (stringpool.top >= stringpool.free);
		assert (stringpool.free >= stringpool.base);
		return;
	case JPI$_LOGINTIM:
	{	int4 days;
		int4 seconds;

		if ((status = lib$getjpi(	 &jpi_code
					,&jpid
					,0
					,&out_quad
					,0
					,0	)) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status );
		}
		if ((status = lib$day(	 &days
					,&out_quad
					,&seconds)) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status );
		}
		days += DAYS;
		seconds /= CENTISECONDS;
		ret->str.addr = stringpool.free;
		stringpool.free = i2s(&days);
		*stringpool.free++ = ',';
		stringpool.free = i2s(&seconds);
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		ret->mvtype = MV_STR;
		return;
	}
	default:
		if ((status = lib$getjpi(	 &jpi_code
						,&jpid
						,0
						,&out_long
						,0
						,0	)) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status );
		}
		i2mval(ret, out_long) ;
		return;
	}
}
