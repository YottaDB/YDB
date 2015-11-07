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
#include "gtm_limits.h"

#include <ssdef.h>
#include "stringpool.h"
#include <descrip.h>
#include "op_fn.h"
#include <psldef.h>
#include <lnmdef.h>
#include "vmsdtype.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "mvalconv.h"
#include "min_max.h"

#define FULL_VALUE -2
#define NOVALUE -1
#define MAX_INDEX 25
#define MAX_RESULT_SIZE 256
#define MAX_LOGNAM_LENGTH 255
#define MIN_INDEX 0

GBLREF spdesc stringpool;
static readonly dvi_struct trnlnm_table[] =
{
	{ 11, "ACCESS_MODE", LNM$_ACMODE},	{ 9, "CONCEALED", LNM$_ATTRIBUTES },
	{ 7, "CONFINE", LNM$_ATTRIBUTES},	{ 4, "FULL", FULL_VALUE },
	{ 6, "LENGTH", LNM$_LENGTH},		{ 9, "MAX_INDEX", LNM$_MAX_INDEX},
	{ 8, "NO_ALIAS", LNM$_ATTRIBUTES},	{ 5, "TABLE", LNM$_ATTRIBUTES},
	{ 10, "TABLE_NAME", LNM$_TABLE},	{ 8, "TERMINAL", LNM$_ATTRIBUTES},
	{ 5, "VALUE", LNM$_STRING}
};

static readonly int attr_tab[] = {NOVALUE, LNM$M_CONCEALED, LNM$M_CONFINE,NOVALUE,NOVALUE,NOVALUE,LNM$M_NO_ALIAS,
		LNM$M_TABLE,NOVALUE,LNM$M_TERMINAL,NOVALUE};

static readonly dvi_index_struct trnlnm_index[] = {
	{ 0, 1 }, { 0, 0 }, { 1, 3}, { 0, 0}, { 0, 0}, { 3, 4}, { 0, 0}, { 0, 0}, { 0, 0},
	{ 0, 0}, { 0, 0}, { 4, 5}, { 5, 6}, { 6, 7}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0},
	{ 0, 0}, { 7, 10}, { 0, 0}, { 10, 11}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}
};

void op_fnztrnlnm(mval *name,mval *table,int4 ind,mval *mode,mval *case_blind,mval *item,mval *ret)
{
	struct dsc$descriptor	lname, ltable;
	uint4		attribute, status, retlen, mask, full_mask;
	char			acmode;
	short int		*item_code, pass, index;
	bool			full;
	char			buff[256], result[MAX_RESULT_SIZE];
	char			i, slot, last_slot;
	char			def_table[] = "LNM$DCL_LOGICAL";

	struct
	{
		item_list_3	item[3];
		int4		terminator;
	} item_list;
	error_def(ERR_BADTRNPARAM);

	if(!name->str.len || MAX_LOGNAM_LENGTH < name->str.len || MAX_LOGNAM_LENGTH < table->str.len)
		rts_error(VARLSTCNT(1) SS$_IVLOGNAM);
	memset(&item_list,0,SIZEOF(item_list));
	item_list.item[0].item_code = 0;
	item_list.item[0].buffer_address = result;
	item_list.item[0].buffer_length = MAX_RESULT_SIZE;
	item_list.item[0].return_length_address = &retlen;
	item_code = &item_list.item[0].item_code;

	lname.dsc$w_length = name->str.len;
	lname.dsc$a_pointer = name->str.addr;
	lname.dsc$b_dtype = DSC$K_DTYPE_T;
	lname.dsc$b_class = DSC$K_CLASS_S;

	if (table->str.len)
	{	ltable.dsc$w_length = table->str.len;
		ltable.dsc$a_pointer = table->str.addr;
	}else
	{	ltable.dsc$a_pointer = def_table;
		ltable.dsc$w_length = strlen(def_table);
	}
	ltable.dsc$b_dtype = DSC$K_DTYPE_T;
	ltable.dsc$b_class = DSC$K_CLASS_S;

	if(ind)
	{	item_list.item[0].item_code = LNM$_INDEX;
		item_list.item[0].buffer_address = &ind;
		item_list.item[1].item_code = 0;
		item_list.item[1].buffer_address = result;
		item_list.item[1].buffer_length = MAX_RESULT_SIZE;
		item_list.item[1].return_length_address = &retlen;
		item_code = &item_list.item[1].item_code;
	}

	attribute = LNM$M_CASE_BLIND;
	if (case_blind->str.len)
	{
		if (case_blind->str.len > 14)
			rts_error(VARLSTCNT(4) ERR_BADTRNPARAM, 2,
				MIN(SHRT_MAX, case_blind->str.len), case_blind->str.addr);
		lower_to_upper(buff,case_blind->str.addr,case_blind->str.len);
		if (case_blind->str.len == 14 && !memcmp(buff,"CASE_SENSITIVE",14))
			attribute = 0;
		else if (case_blind->str.len != 10 || memcmp(buff,"CASE_BLIND",10))
			rts_error(VARLSTCNT(4) ERR_BADTRNPARAM,2,case_blind->str.len,case_blind->str.addr);
	}

	acmode = NOVALUE;
	if (mode->str.len)
	{
		if (mode->str.len > 14)
			rts_error(VARLSTCNT(4) ERR_BADTRNPARAM, 2,
				MIN(SHRT_MAX, mode->str.len), mode->str.addr);
		lower_to_upper(buff,mode->str.addr,mode->str.len);
		switch (buff[0])
		{
		case 'U':
			if ( mode->str.len = 4 && !memcmp(buff, "USER", 4))
				acmode = PSL$C_USER;
		case 'S':
			if ( mode->str.len = 10 && !memcmp(buff, "SUPERVISOR", 10))
				acmode = PSL$C_SUPER;
		case 'K':
			if ( mode->str.len = 6 && !memcmp(buff, "KERNEL", 6))
				acmode = PSL$C_KERNEL;
		case 'E':
			if ( mode->str.len = 9 && !memcmp(buff, "EXECUTIVE", 9))
				acmode = PSL$C_EXEC;
		}
		if (acmode == NOVALUE)
			rts_error(VARLSTCNT(4) ERR_BADTRNPARAM,2,mode->str.len,mode->str.addr);
	}

	full = FALSE;
	*item_code = NOVALUE;
	if (item->str.len)
	{	if (item->str.len > 12)
			rts_error(VARLSTCNT(4) ERR_BADTRNPARAM, 2,
				MIN(SHRT_MAX, item->str.len), item->str.addr);
		lower_to_upper(buff,item->str.addr,item->str.len);
		if ((i = buff[0] - 'A') < MIN_INDEX || i > MAX_INDEX)
		{	rts_error(VARLSTCNT(4) ERR_BADTRNPARAM, 2, item->str.len, item->str.addr);
		}
		if ( trnlnm_index[i].len)
		{	slot = trnlnm_index[i].index;
			last_slot = trnlnm_index[i].len;
			for (; slot < last_slot; slot++)
			{	if (item->str.len == trnlnm_table[slot].len &&
					!memcmp(trnlnm_table[slot].name, buff, item->str.len))
				{	if (trnlnm_table[slot].item_code == FULL_VALUE)
					{	if (ind)
							index = 2;
						else
							index = 1;
						*item_code = LNM$_STRING;
						item_list.item[index].buffer_address = &full_mask;
						item_list.item[index].buffer_length = SIZEOF(full_mask);
						item_list.item[index].item_code = LNM$_ATTRIBUTES;
						item_code = &item_list.item[index].item_code;
						full = TRUE;
					}else
					{	*item_code = trnlnm_table[slot].item_code;
					}
					break;
				}
			}
		}
		if (*item_code == NOVALUE)
		{	rts_error(VARLSTCNT(4) ERR_BADTRNPARAM,2,item->str.len,item->str.addr);
		}
	}else
	{	*item_code = LNM$_STRING;
	}
	for ( pass = 0 ; ; pass++ )
	{	retlen = 0;
		status = sys$trnlnm(&attribute, &ltable, &lname, acmode == NOVALUE ? 0 : &acmode, &item_list);
		if (status & 1)
		{	if (*item_code == LNM$_STRING || *item_code == LNM$_TABLE)
			{	ret->mvtype = MV_STR;
				ENSURE_STP_FREE_SPACE(retlen);
				ret->str.addr = stringpool.free;
				ret->str.len = retlen;
				memcpy(ret->str.addr, result, retlen);
				stringpool.free += retlen;
				return;
			}else if (*item_code == LNM$_LENGTH || *item_code == LNM$_MAX_INDEX)
			{
				MV_FORCE_MVAL(ret,*(int4 *)result) ;
				n2s(ret);
				return;
			}else if (*item_code == LNM$_ACMODE)
			{	ret->mvtype = MV_STR;
				switch(*result)
				{	case PSL$C_USER:
						ENSURE_STP_FREE_SPACE(4);
						ret->str.addr = stringpool.free;
						ret->str.len = 4;
						memcpy(ret->str.addr, "USER", 4);
						stringpool.free += 4;
						return;
					case PSL$C_SUPER:
						ENSURE_STP_FREE_SPACE(5);
						ret->str.addr = stringpool.free;
						ret->str.len = 5;
						memcpy(ret->str.addr, "SUPER", 5);
						stringpool.free += 5;
						return;
					case PSL$C_EXEC:
						ENSURE_STP_FREE_SPACE(9);
						ret->str.addr = stringpool.free;
						ret->str.len = 9;
						memcpy(ret->str.addr, "EXECUTIVE", 9);
						stringpool.free += 9;
						return;
					case PSL$C_KERNEL:
						ENSURE_STP_FREE_SPACE(6);
						ret->str.addr = stringpool.free;
						ret->str.len = 6;
						memcpy(ret->str.addr, "KERNEL", 6);
						stringpool.free += 6;
						return;
					default:
						GTMASSERT;
				}
			}else
			{	assert(*item_code == LNM$_ATTRIBUTES);
				if (full)
				{	if (!retlen)	/* If the logical name exists, but has no entry for the specified index, */
					{		/* then the return status will be normal as the TERMINAL attribute will  */
							/* be filled in, but there will be no equivalence name, thus retlen == 0 */
						ret->mvtype = MV_STR;
						if (!pass)
						{	ret->str.len = 0;
							return;
						}
						ENSURE_STP_FREE_SPACE(lname.dsc$w_length);
						ret->str.addr = stringpool.free;
						ret->str.len = lname.dsc$w_length;
						memcpy(ret->str.addr, lname.dsc$a_pointer, lname.dsc$w_length);
						stringpool.free += lname.dsc$w_length;
						return;
					}
					if(full_mask & LNM$M_TERMINAL)
					{	ret->mvtype = MV_STR;
						ENSURE_STP_FREE_SPACE(retlen);
						ret->str.addr = stringpool.free;
						ret->str.len = retlen;
						memcpy(ret->str.addr, result, retlen);
						stringpool.free += retlen;
						return;
					}
					memcpy(buff,result,retlen);
					lname.dsc$w_length = retlen;
					lname.dsc$a_pointer = buff;
				}else
				{	mask = attr_tab[slot];
					if (mask == NOVALUE)
						GTMASSERT;
					MV_FORCE_MVAL(ret,( *((int4*)result) & mask ? 1 : 0 )) ;
					n2s(ret);
					return;
				}
			}
		}else if (status == SS$_NOLOGNAM)
		{	ret->mvtype = MV_STR;
			if (full && pass > 0)
			{
				ENSURE_STP_FREE_SPACE(lname.dsc$w_length);
				ret->str.addr = stringpool.free;
				ret->str.len = lname.dsc$w_length;
				memcpy(ret->str.addr, lname.dsc$a_pointer, lname.dsc$w_length);
				stringpool.free += lname.dsc$w_length;
			}else
			{	ret->str.len = 0;
			}
			return;
		}else
		{	rts_error(VARLSTCNT(1) status);
		}
	}
	MV_FORCE_MVAL(ret, 0) ;
	return;
}
