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

#include "gtm_string.h"

#include <rms.h>
#include <ssdef.h>
#include "gtm_caseconv.h"
#include "stringpool.h"
#include "op.h"

#define ZP_DEVICE	1
#define ZP_DIRECTORY	2
#define ZP_NAME		3
#define ZP_NODE		4
#define ZP_TYPE		5
#define ZP_VERSION	6
#define ZP_FULL		7

#define DEV_LEN		6
#define DIR_LEN		9
#define VER_LEN		7
#define ZP_LEN		4
#define ZP_STR_LEN	255
#define NCON_LEN	10
#define SYN_LEN		11

void op_fnzparse( mval *file, mval *field, mval *def1, mval *def2, mval *type, mval *ret)
{
char 		field_type;
uint4 	status;
unsigned char 	field_buf[DIR_LEN],type_buf[SYN_LEN], def_buf[ZP_STR_LEN], esa[ZP_STR_LEN];
struct FAB fab, fab_def;
struct NAM nam, nam_def;
error_def(ERR_ZPARSETYPE);
error_def(ERR_ZPARSFLDBAD);
error_def(ERR_INVSTRLEN);

MV_FORCE_STR(field);
MV_FORCE_STR(def1);
MV_FORCE_STR(def2);
MV_FORCE_STR(file);
MV_FORCE_STR(type);

if (def1->str.len > ZP_STR_LEN)
	rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, def1->str.len, ZP_STR_LEN);
if (def2->str.len > ZP_STR_LEN)
	rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, def2->str.len, ZP_STR_LEN);

if (field->str.len == 0)
{	field_type = ZP_FULL;
}
else
{	field_type = 0;
	if (field->str.len <= DIR_LEN)
	{
		lower_to_upper(&field_buf[0], field->str.addr, field->str.len);
		switch( field_buf[0] )
		{
		case 'D':
			if (field->str.len <= DEV_LEN &&
				!memcmp(&field_buf[0],"DEVICE",field->str.len))
			{	field_type = ZP_DEVICE;
			}
			else if (field->str.len <= DIR_LEN &&
				!memcmp(&field_buf[0],"DIRECTORY",field->str.len))
			{	field_type = ZP_DIRECTORY;
			}
			break;
		case 'N':
			if (field->str.len <= ZP_LEN)
			{	if(!memcmp(&field_buf[0],"NAME",field->str.len))
				{	field_type = ZP_NAME;
				}
				else if (!memcmp(&field_buf[0],"NODE",field->str.len))
				{	field_type = ZP_NODE;
				}
			}
			break;
		case 'T':
			if (field->str.len <= ZP_LEN &&
				!memcmp(&field_buf[0],"TYPE",field->str.len))
			{	field_type = ZP_TYPE;
			}
			break;
		case 'V':
			if (field->str.len <= VER_LEN &&
				!memcmp(&field_buf[0],"VERSION",field->str.len))
			{	field_type = ZP_VERSION;
			}
			break;
		default:
			break;
		}
	}
	if(!field_type)
	{	rts_error(VARLSTCNT(4) ERR_ZPARSFLDBAD,2,field->str.len,field->str.addr);
	}
}
fab = cc$rms_fab;
nam = cc$rms_nam;
if (type->str.len == 0)
{	nam.nam$b_nop = 0;
}
else
{
	if (type->str.len <= SYN_LEN)
		lower_to_upper(&type_buf[0], type->str.addr, type->str.len);
	if (type->str.len <= SYN_LEN && !memcmp(&type_buf[0],"SYNTAX_ONLY",type->str.len))
	{	nam.nam$b_nop = NAM$M_SYNCHK;
	}
	else if (type->str.len <= NCON_LEN &&
			!memcmp(&type_buf[0],"NO_CONCEAL",type->str.len))
	{	nam.nam$b_nop = NAM$M_NOCONCEAL;
	}
	else
	{	rts_error(VARLSTCNT(4) ERR_ZPARSETYPE,2,type->str.len,type->str.addr);
	}
}

fab.fab$l_nam = &nam;
fab.fab$l_fop = FAB$M_NAM;
fab.fab$l_fna = file->str.addr;
fab.fab$b_fns = file->str.len;
nam.nam$l_esa = esa;
nam.nam$b_ess = sizeof (esa);

if (def2->str.len)
{
	fab_def = cc$rms_fab;
	nam_def = cc$rms_nam;
	fab_def.fab$l_nam = &nam_def;
	fab_def.fab$l_fop = FAB$M_NAM;
	fab_def.fab$l_fna = def2->str.addr;
	fab_def.fab$b_fns = def2->str.len;
	nam_def.nam$b_nop = NAM$M_SYNCHK;
	nam_def.nam$l_esa = def_buf;
	nam_def.nam$b_ess = sizeof (def_buf);
	if ((status = sys$parse(&fab_def,0,0)) != RMS$_NORMAL)
	{	ret->mvtype = MV_STR;
		ret->str.len = 0;
		return;
	}
	nam_def.nam$l_rsa = nam_def.nam$l_esa;
	nam_def.nam$b_rsl = nam_def.nam$b_esl;
	nam.nam$l_rlf = &nam_def;
}
if (def1->str.len)
{	fab.fab$l_dna = def1->str.addr;
	fab.fab$b_dns = def1->str.len;
}

if ((status = sys$parse(&fab, 0, 0)) != RMS$_NORMAL)
{	ret->mvtype = MV_STR;
	ret->str.len = 0;
	return;
}

ret->mvtype = MV_STR;
switch( field_type )
{
case ZP_DEVICE:
	ret->str.addr = nam.nam$l_dev;
	ret->str.len = nam.nam$b_dev;
	break;
case ZP_DIRECTORY:
	ret->str.addr = nam.nam$l_dir;
	ret->str.len = nam.nam$b_dir;
	break;
case ZP_NAME:
	ret->str.addr = nam.nam$l_name;
	ret->str.len = nam.nam$b_name;
	break;
case ZP_NODE:
	ret->str.addr = nam.nam$l_node;
	ret->str.len = nam.nam$b_node;
	break;
case ZP_TYPE:
	ret->str.addr = nam.nam$l_type;
	ret->str.len = nam.nam$b_type;
	break;
case ZP_VERSION:
	ret->str.addr = nam.nam$l_ver;
	ret->str.len = nam.nam$b_ver;
	break;
default:
	ret->str.addr = nam.nam$l_esa;
	ret->str.len = nam.nam$b_esl;
	break;
}
s2pool(&ret->str);

if (nam.nam$b_nop != NAM$M_SYNCHK)
{	/* release channel from parse */
	fab.fab$b_dns = fab.fab$b_fns = 0;
	nam.nam$b_nop = NAM$M_SYNCHK;
	sys$parse(&fab, 0, 0);
}

return;
}
