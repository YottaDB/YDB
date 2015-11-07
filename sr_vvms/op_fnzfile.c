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

#include <ssdef.h>
#include <rms.h>

#include "stringpool.h"
#include "min_max.h"
#include "gtm_caseconv.h"
#include "mvalconv.h"
#include "op.h"

GBLREF spdesc stringpool;

#define ZF_ALQ	0
#define ZF_BDT	1
#define ZF_BKS	2
#define ZF_BLS 	3
#define ZF_CBT	4
#define ZF_CDT	5
#define ZF_CTG	6
#define ZF_DEQ	7
#define ZF_DID	8
#define ZF_DVI	9
#define ZF_EDT	10
#define ZF_EOF	11
#define ZF_FID	12
#define ZF_FSZ	13
#define ZF_GRP	14
#define ZF_KNO	15
#define ZF_MBM	16
#define ZF_MRN	17
#define ZF_MRS	18
#define ZF_NOA	19
#define ZF_NOK	20
#define ZF_ORG	21
#define ZF_PRO	22
#define ZF_PVN	23
#define ZF_RAT	24
#define ZF_RCK	25
#define ZF_RDT	26
#define ZF_RFM	27
#define ZF_RVN	28
#define ZF_UIC	29
#define ZF_WCK	30

#define KEY_LEN	3
#define MAX_ZF_LEN	128
#define PRO_GRP_LEN	3
#define PRO_MBM_LEN	5
#define ORG_LEN	3
#define RAT_LEN	3
#define RAT_CR_LEN	2
#define RFM_LEN	3
#define RFM_STM_LEN	5
#define MIN_ZF_INDEX	0
#define MAX_ZF_INDEX	25
#define PRO_FLDS	4
#define ACC_FLDS	4
#define FLD_SZ	4

#define MAX_KW_LEN    	8

typedef struct
{	char *name;
}zfile_key_struct;

static readonly zfile_key_struct zfile_key[] =
{
	"ALQ","BDT","BKS","BLS","CBT","CDT","CTG","DEQ","DID","DVI","EDT","EOF","FID",
	"FSZ","GRP","KNO","MBM","MRN","MRS","NOA","NOK","ORG","PRO","PVN","RAT","RCK",
	"RDT","RFM","RVN","UIC","WCK"
};

typedef struct
{	char index;
	char last_index;
}zfile_index_struct;

static readonly zfile_index_struct zfile_index[] =
{
	{0,1},{1,4},{4,7},{7,10},{10,12},{12,14},{14,15},{0,0},{0,0},{0,0},{15,16},{0,0},
	{16,19},{19,21},{21,22},{22,24},{0,0},{24,29},{0,0},{0,0},{29,30},{0,0},{30,31},
	{0,0},{0,0},{0,0}
};

typedef struct
{	unsigned x;
	unsigned y;
}quad_struct;

void op_fnzfile(mval *name,mval *key,mval *ret)
{
	struct NAM	nm;
	struct XABDAT	dat;
	struct XABSUM	sum;
	struct XABPRO	pro;
	struct XABFHC	fhc;
	quad_struct 	*bdt;
	quad_struct 	*cdt;
	quad_struct 	*edt;
	quad_struct 	*rdt;
	int4 days;
	int4 seconds;
	error_def(ERR_ZFILNMBAD);
	error_def(ERR_ZFILKEYBAD);
	struct FAB	f;
	int4 	status;
	char 	index,slot,last_slot;
	char	buf[MAX_KW_LEN] ;

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.top >= stringpool.free);
	MV_FORCE_STR(name);
	if (name->str.len == 0)
	{	rts_error(VARLSTCNT(4) ERR_ZFILNMBAD,2,4,"NULL");
	}
	MV_FORCE_STR(key);
	if (key->str.len == 0 || key->str.len != KEY_LEN)
	{	rts_error(VARLSTCNT(4) ERR_ZFILKEYBAD,2,key->str.len,key->str.addr);
	}
	lower_to_upper(buf,key->str.addr,MIN(key->str.len,MAX_KW_LEN)) ;
	nm = cc$rms_nam;
	dat = cc$rms_xabdat;
	sum = cc$rms_xabsum;
	pro = cc$rms_xabpro;
	fhc = cc$rms_xabfhc;
	bdt = &(dat.xab$q_bdt);
	cdt = &(dat.xab$q_cdt);
	edt = &(dat.xab$q_edt);
	rdt = &(dat.xab$q_rdt);
	dat.xab$l_nxt = &sum;
	sum.xab$l_nxt = &pro;
	pro.xab$l_nxt = &fhc;
	if ((index = buf[0] - 'A') < MIN_ZF_INDEX || index > MAX_ZF_INDEX)
		rts_error(VARLSTCNT(4)  ERR_ZFILKEYBAD,2,key->str.len,key->str.addr);
	if (!(last_slot = zfile_index[ index ].last_index))
		rts_error(VARLSTCNT(4)  ERR_ZFILKEYBAD,2,key->str.len,key->str.addr);
	slot = zfile_index[ index ].index;
	for ( ; slot < last_slot ; slot++ )
	{
		if (!memcmp(zfile_key[slot].name,buf,3))
		{	break;
		}
	}
	if (slot == last_slot)
		rts_error(VARLSTCNT(4)  ERR_ZFILKEYBAD,2,key->str.len,key->str.addr);
	f  = cc$rms_fab;
	f.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_SHRDEL | FAB$M_SHRUPD;
	f.fab$l_nam = &nm;
	f.fab$l_xab = &dat;
	f.fab$l_fna = name->str.addr;
	f.fab$b_fns = name->str.len;
	if ((status = sys$open(&f)) != RMS$_NORMAL)
		rts_error(VARLSTCNT(1) status);
	ENSURE_STP_FREE_SPACE(MAX_ZF_LEN);
	switch( slot )
	{
	case ZF_ALQ:
		i2mval(ret,f.fab$l_alq) ;
		break;
	case ZF_BDT:
	{	ret->mvtype = MV_STR;
		if (!bdt->x && !bdt->y)
		{	ret->str.len = 0;
			break;
		}
		if ((status= lib$day( &days, bdt, &seconds )) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status );
		}
		days += DAYS;
		seconds /= CENTISECONDS;
		ret->str.addr = stringpool.free;
		stringpool.free = i2s(&days);
		*stringpool.free++ = ',';
		stringpool.free = i2s(&seconds);
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);

		break;
	}
	case ZF_BKS:
		i2mval(ret,f.fab$b_bks) ;
		break;
	case ZF_BLS:
		i2mval(ret,f.fab$w_bls) ;
		break;
	case ZF_CBT:
		i2mval (ret,(f.fab$l_fop & FAB$M_CBT ? 1 : 0 )) ;
		break;
	case ZF_CDT:
	{
		if ((status= lib$day( &days, cdt, &seconds )) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status );
		}
		days += DAYS;
		seconds /= CENTISECONDS;
		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		stringpool.free = i2s(&days);
		*stringpool.free++ = ',';
		stringpool.free = i2s(&seconds);
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
		break;
	}
	case ZF_CTG:
		i2mval (ret,(f.fab$l_fop & FAB$M_CTG ? 1 : 0 )) ;
		break;
	case ZF_DEQ:
		i2mval(ret,f.fab$w_deq) ;
		break;
	case ZF_DID:
	{	int4 did;

		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		did = nm.nam$w_did[0];
		stringpool.free = i2s(&did);
		*stringpool.free++ = ',';
		did = nm.nam$w_did[1];
		stringpool.free = i2s(&did);
		*stringpool.free++ = ',';
		did = nm.nam$w_did[2];
		stringpool.free = i2s(&did);
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
		break;
	}
	case ZF_DVI:
		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		memcpy(stringpool.free,&(nm.nam$t_dvi[1]),nm.nam$t_dvi[0]);
		ret->str.len = nm.nam$t_dvi[0];
		stringpool.free += ret->str.len;
		break;
	case ZF_EDT:
	{	ret->mvtype = MV_STR;
		if (!edt->x && !edt->y)
		{	ret->str.len = 0;
			break;
		}
		if ((status= lib$day(	 &days
					,edt
					,&seconds	)) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status );
		}
		days += DAYS;
		seconds /= CENTISECONDS;
		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		stringpool.free = i2s(&days);
		*stringpool.free++ = ',';
		stringpool.free = i2s(&seconds);
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
		break;
	}
	case ZF_EOF:
		i2mval(ret,fhc.xab$l_ebk - 1) ;
		break;
	case ZF_FID:
	{	int4 fid;

		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		fid = nm.nam$w_fid[0];
		stringpool.free = i2s(&fid);
		*stringpool.free++ = ',';
		fid = nm.nam$w_fid[1];
		stringpool.free = i2s(&fid);
		*stringpool.free++ = ',';
		fid = nm.nam$w_fid[2];
		stringpool.free = i2s(&fid);
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
		break;
	}
	case ZF_FSZ:
		i2mval(ret,f.fab$b_fsz) ;
		break;
	case ZF_GRP:
	{	char *pro_ptr;

		pro_ptr = ((char *)&pro + XAB$C_PROLEN - PRO_GRP_LEN);
		i2mval(ret,*(short *)pro_ptr) ;
		break;
	}
	case ZF_KNO:
		ret->mvtype = MV_STR;
		ret->str.len = 0;
		break;
	case ZF_MBM:
	{	char *pro_ptr;
		pro_ptr = ((char *)&pro + XAB$C_PROLEN - PRO_MBM_LEN);
		i2mval(ret,*(short *)pro_ptr) ;
		break;
	}
	case ZF_MRN:
		i2mval(ret,f.fab$l_mrn) ;
		break;
	case ZF_MRS:
		i2mval(ret,f.fab$w_mrs) ;
		break;
	case ZF_NOA:
		i2mval(ret,sum.xab$b_noa) ;
		break;
	case ZF_NOK:
		i2mval(ret,sum.xab$b_nok) ;
		break;
	case ZF_ORG:
		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		if (f.fab$b_org == FAB$C_SEQ)
		{	memcpy(stringpool.free,"SEQ",ORG_LEN);
		}
		else if (f.fab$b_org == FAB$C_REL)
		{	memcpy(stringpool.free,"REL",ORG_LEN);
		}
		else if (f.fab$b_org == FAB$C_IDX)
		{	memcpy(stringpool.free,"IDX",ORG_LEN);
		}
		else if (f.fab$b_org == FAB$C_HSH)
		{	memcpy(stringpool.free,"HSH",ORG_LEN);
		}
		ret->str.len = ORG_LEN;
		stringpool.free += ORG_LEN;
		break;
	case ZF_PRO:
	{	unsigned short mask;
		char x,i;

		x = XAB$V_SYS;
		mask = pro.xab$w_pro;
		ret->str.addr = stringpool.free;
		ret->mvtype = MV_STR;
		for ( x = 0 ; x < PRO_FLDS ;x++ )
		{	if (x)
			{	*stringpool.free++ = ',';
			}
			for (i = 0 ; i < ACC_FLDS ; i++ )
			{	switch(i)
				{
				case XAB$V_NOREAD:
					if ((mask & XAB$M_NOREAD) == 0)
					{	*stringpool.free++ = 'R';
					}
					break;
				case XAB$V_NOWRITE:
					if ((mask & XAB$M_NOWRITE) == 0)
					{	*stringpool.free++ = 'W';
					}
					break;
				case XAB$V_NOEXE:
					if ((mask & XAB$M_NOEXE) == 0)
					{	*stringpool.free++ = 'E';
					}
					break;
				case XAB$V_NODEL:
					if ((mask & XAB$M_NODEL) == 0)
					{	*stringpool.free++ = 'D';
					}
					break;
				}
			}
			mask = (mask >> FLD_SZ);
		}
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		break;
	}
	case ZF_PVN:
		i2mval(ret,sum.xab$w_pvn) ;
		break;
	case ZF_RAT:
		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		if (f.fab$b_rat & FAB$M_BLK)
		{	memcpy(stringpool.free,"BLK",RAT_LEN);
			stringpool.free += RAT_LEN;
			*stringpool.free++ = ',';
		}
		if (f.fab$b_rat & FAB$M_CR)
		{	memcpy(stringpool.free,"CR",RAT_CR_LEN);
			stringpool.free += RAT_CR_LEN;
			*stringpool.free++ = ',';
		}
		if (f.fab$b_rat & FAB$M_FTN)
		{	memcpy(stringpool.free,"FTN",RAT_LEN);
			stringpool.free += RAT_LEN;
			*stringpool.free++ = ',';
		}
		if (f.fab$b_rat & FAB$M_PRN)
		{	memcpy(stringpool.free,"PRN",RAT_LEN);
			stringpool.free += RAT_LEN;
			*stringpool.free++ = ',';
		}
		if (stringpool.free != ret->str.addr)
		{	stringpool.free--;
		}
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		break;
	case ZF_RCK:
		i2mval(ret, (f.fab$l_fop & FAB$M_RCK ? 1 : 0 )) ;
		break;
	case ZF_RDT:
	{	ret->mvtype = MV_STR;
		if (!rdt->x && !rdt->y)
		{	ret->str.len = 0;
			break;
		}
		if ((status= lib$day(	 &days
					,rdt
					,&seconds	)) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1)  status );
		}
		days += DAYS;
		seconds /= CENTISECONDS;
		ret->str.addr = stringpool.free;
		stringpool.free = i2s(&days);
		*stringpool.free++ = ',';
		stringpool.free = i2s(&seconds);
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
		break;
	}
	case ZF_RFM:
		ret->mvtype = MV_STR;
		ret->str.addr = stringpool.free;
		switch( f.fab$b_rfm )
		{
		case FAB$C_FIX:
			memcpy(stringpool.free,"FIX",RFM_LEN);
			stringpool.free += RFM_LEN;
			break;
		case FAB$C_STM:
			memcpy(stringpool.free,"STM",RFM_LEN);
			stringpool.free += RFM_LEN;
			break;
		case FAB$C_STMCR:
			memcpy(stringpool.free,"STMCR",RFM_STM_LEN);
			stringpool.free += RFM_STM_LEN;
			break;
		case FAB$C_STMLF:
			memcpy(stringpool.free,"STMLF",RFM_STM_LEN);
			stringpool.free += RFM_STM_LEN;
			break;
		case FAB$C_UDF:
			memcpy(stringpool.free,"UDF",RFM_LEN);
			stringpool.free += RFM_LEN;
			break;
		case FAB$C_VAR:
			memcpy(stringpool.free,"VAR",RFM_LEN);
			stringpool.free += RFM_LEN;
			break;
		case FAB$C_VFC:
			memcpy(stringpool.free,"VFC",RFM_LEN);
			stringpool.free += RFM_LEN;
			break;
		default:
			assert(0);
		}
		ret->str.len = (char *) stringpool.free - ret->str.addr;
		break;
	case ZF_RVN:
		i2mval(ret,dat.xab$w_rvn) ;
		break;
	case ZF_UIC:
		i2mval(ret,pro.xab$l_uic) ;
		break;
	case ZF_WCK:
		i2mval(ret,(f.fab$l_fop & FAB$M_WCK ? 1 : 0 )) ;
		break;
	default:
		assert(0);
	}
	if ((status = sys$close(&f)) != RMS$_NORMAL)
	{	rts_error(VARLSTCNT(1)  status );
	}
	return;
}
