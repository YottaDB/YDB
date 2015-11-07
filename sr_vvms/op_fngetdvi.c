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
#include <dvidef.h>
#include <devdef.h>
#include <ssdef.h>
#include <descrip.h>
#include "stringpool.h"
#include "op_fn.h"
#include "efn.h"
#include "gtm_caseconv.h"
#include "mvalconv.h"
#include "op.h"

#define MIN_INDEX 0
#define MAX_INDEX 25
#define MAX_DVI_STRLEN 128
#define MAX_DEV_LENGTH 63
#define MAX_KEY_LENGTH 14
#define VAL_LENGTH 4
#define SPL_CODE 83
#define HEX_LEN 8


static readonly dvi_struct dvi_table[] =
{
	{ 6, "ACPPID" , DVI$_ACPPID },		{ 7, "ACPTYPE", DVI$_ACPTYPE },
	{ 3, "ALL" ,DVI$_ALL },			{ 9, "ALLDEVNAM", DVI$_ALLDEVNAM },
	{ 9, "ALTYPEAHD", DVI$_TT_ALTYPEAHD },	{ 7, "ANSICRT", DVI$_TT_ANSICRT },
	{ 10, "APP_KEYPAD", DVI$_TT_APP_KEYPAD },{ 8, "AUTOBAUD", DVI$_TT_AUTOBAUD },
	{ 3,"AVL" , DVI$_AVL },			{ 3, "AVO", DVI$_TT_AVO },
	{ 5, "BLOCK", DVI$_TT_BLOCK },		{ 9, "BRDCSTMBX", DVI$_TT_BRDCSTMBX },
	{ 3,"CCL" , DVI$_CCL },			{ 7, "CLUSTER", DVI$_CLUSTER },
	{ 9, "CONCEALED", DVI$_CONCEALED },	{ 6, "CRFILL", DVI$_TT_CRFILL },
	{ 9, "CYLINDERS", DVI$_CYLINDERS },
	{ 6, "DECCRT", DVI$_TT_DECCRT },
	{ 9, "DEVBUFSIZ", DVI$_DEVBUFSIZ },	{ 7, "DEVCHAR", DVI$_DEVCHAR },
	{ 8, "DEVCHAR2", DVI$_DEVCHAR2 },	{ 8, "DEVCLASS", DVI$_DEVCLASS },
	{ 9, "DEVDEPEND", DVI$_DEVDEPEND },	{ 10, "DEVDEPEND2", DVI$_DEVDEPEND2 },
	{ 10, "DEVLOCKNAM", DVI$_DEVLOCKNAM },	{ 6, "DEVNAM", DVI$_DEVNAM },
	{ 6, "DEVSTS", DVI$_DEVSTS },		{ 7, "DEVTYPE", DVI$_DEVTYPE },
	{ 6, "DIALUP", DVI$_TT_DIALUP },
	{ 3,"DIR" , DVI$_DIR },			{ 10, "DISCONNECT", DVI$_TT_DISCONNECT },
	{ 3, "DMA", DVI$_TT_DMA },		{ 3, "DMT" , DVI$_DMT },
	{ 4, "DRCS", DVI$_TT_DRCS },		{ 3, "DUA" , DVI$_DUA },
	{ 4, "EDIT", DVI$_TT_EDIT },		{ 7, "EDITING", DVI$_TT_EDITING },
	{ 8, "EIGHTBIT", DVI$_TT_EIGHTBIT },	{ 3, "ELG" , DVI$_ELG },
	{ 6, "ERRCNT", DVI$_ERRCNT },		{ 6, "ESCAPE", DVI$_TT_ESCAPE },
	{ 8, "FALLBACK", DVI$_TT_FALLBACK },	{ 3, "FOD" , DVI$_FOD },
	{ 3, "FOR" , DVI$_FOR },			{ 10, "FREEBLOCKS", DVI$_FREEBLOCKS },
	{ 10, "FULLDEVNAM", DVI$_FULLDEVNAM },
	{ 3, "GEN" , DVI$_GEN },
	{ 7, "HALFDUP", DVI$_TT_HALFDUP },
	{ 6, "HANGUP", DVI$_TT_HANGUP },	{ 8, "HOSTSYNC", DVI$_TT_HOSTSYNC },
	{ 3, "IDV" , DVI$_IDV },			{ 6, "INSERT", DVI$_TT_INSERT },
	{ 6, "LFFILL", DVI$_TT_LFFILL },	{ 9, "LOCALECHO", DVI$_TT_LOCALECHO },
	{ 6, "LOCKID", DVI$_LOCKID },
	{ 9, "LOGVOLNAM", DVI$_LOGVOLNAM },	{ 5, "LOWER", DVI$_TT_LOWER },
	{ 8, "MAXBLOCK", DVI$_MAXBLOCK },	{ 8, "MAXFILES", DVI$_MAXFILES },
	{ 3, "MBX" , DVI$_MBX },			{ 8, "MBXDSABL", DVI$_TT_MBXDSABL },
	{ 8, "MECHFORM", DVI$_TT_MECHFORM },	{ 7, "MECHTAB", DVI$_TT_MECHTAB },
	{ 3, "MNT" , DVI$_MNT },			{ 5, "MODEM", DVI$_TT_MODEM },
	{ 9, "MODHANGUP", DVI$_TT_MODHANGUP },	{ 8, "MOUNTCNT", DVI$_MOUNTCNT },
	{ 3, "NET" , DVI$_NET },			{ 10, "NEXTDEVNAM", DVI$_NEXTDEVNAM },
	{ 8, "NOBRDCST", DVI$_TT_NOBRDCST },
	{ 6, "NOECHO", DVI$_TT_NOECHO },	{ 9, "NOTYPEAHD", DVI$_TT_NOTYPEAHD },
	{ 3, "ODV" , DVI$_ODV },			{ 5, "OPCNT", DVI$_OPCNT },
	{ 4, "OPER", DVI$_TT_OPER},		{ 3, "OPR" , DVI$_OPR },
	{ 6, "OWNUIC", DVI$_OWNUIC },
	{ 7, "PASTHRU", DVI$_TT_PASTHRU },
	{ 3, "PID", DVI$_PID },		{ 7, "PRINTER", DVI$_TT_PRINTER },
	{ 3, "RCK" , DVI$_RCK },			{ 8, "READSYNC", DVI$_TT_READSYNC },
	{ 3, "REC" , DVI$_REC },			{ 6, "RECSIZ", DVI$_RECSIZ },
	{ 6, "REFCNT", DVI$_REFCNT },		{ 5, "REGIS", DVI$_TT_REGIS },
	{ 3, "RND" , DVI$_RND },			{ 10, "ROOTDEVNAM", DVI$_ROOTDEVNAM },
	{ 3, "RTM" , DVI$_RTM },
	{ 5, "SCOPE", DVI$_TT_SCOPE },		{ 3, "SDI" , DVI$_SDI },
	{ 7, "SECTORS", DVI$_SECTORS },		{ 6, "SECURE", DVI$_TT_SECURE },
	{ 9, "SERIALNUM", DVI$_SERIALNUM },	{13, "SERVED_DEVICE", DVI$_SERVED_DEVICE},
	{ 8, "SETSPEED", DVI$_TT_SETSPEED },
	{ 3, "SHR" , DVI$_SHR },			{ 5, "SIXEL", DVI$_TT_SIXEL },
	{ 3, "SPL" , DVI$_SPL },			{ 9, "SPLDEVNAM", DVI$_DEVNAM },
	{ 3, "SQD" , DVI$_SQD },			{ 3, "STS", DVI$_STS },
	{ 3, "SWL" , DVI$_SWL },			{ 6, "SYSPSW", DVI$_TT_SYSPWD },
	{ 6, "TRACKS", DVI$_TRACKS },		{ 8, "TRANSCNT", DVI$_TRANSCNT },
	{ 3, "TRM" , DVI$_TRM },		{ 12, "TT_ACCPORNAM" , DVI$_TT_ACCPORNAM },
	{ 12, "TT_PHYDEVNAM" , DVI$_TT_PHYDEVNAM },	{ 6, "TTSYNC", DVI$_TT_TTSYNC },
	{ 4, "UNIT", DVI$_UNIT },		{ 8, "VOLCOUNT", DVI$_VOLCOUNT },
	{ 6, "VOLNAM", DVI$_VOLNAM },		{ 9, "VOLNUMBER", DVI$_VOLNUMBER },
	{ 9, "VOLSETMEM", DVI$_VOLSETMEM },	{ 5, "VPROT", DVI$_VPROT },
	{ 3, "WCK" , DVI$_WCK },			{ 4, "WRAP", DVI$_TT_WRAP }
};

static readonly dvi_index_struct dvi_index[] = {
	/*	A	B		C	D		E		F	G		*/
	{ 0 , 10 }, { 10 , 12 }, { 12 , 17 }, { 17, 35 }, { 35 , 41 }, { 41 , 46 }, { 46, 47 },
	/*	H	I		J	K		L	M		N		*/
	{ 47, 50 }, { 50 , 52 }, { 0 , 0 }, { 0 , 0 }, { 52 , 57 }, { 57 , 67 }, { 67 , 72 },
	/*	O	P		Q	R		S	T		U		*/
	{ 72 , 77 }, { 77 , 80 }, { 0 , 0 }, { 80 , 89 }, { 89 , 104 }, { 104 , 110 }, {110 , 111},
	/*	V	W	X     Y	    Z								*/
	{ 111,116},{116,118},{0,0},{0,0},{0,0}
};
GBLREF spdesc stringpool;

void op_fngetdvi(mval *device, mval *keyword, mval *ret)
{
	itmlist_struct	item_list;
	short 		out_len, iosb[4];
	uint4 	status;
	char 		index, slot, last_slot;
	int4 		item_code, out_value;
	unsigned char 	buff[MAX_KEY_LENGTH], *upper_case;
	bool		want_secondary;
	$DESCRIPTOR(device_name,"");
	error_def(ERR_DVIKEYBAD);
	error_def(ERR_INVSTRLEN);

	MV_FORCE_STR(device);
	MV_FORCE_STR(keyword);

	if (MAX_DEV_LENGTH < device->str.len)
		rts_error(VARLSTCNT(1) SS$_IVLOGNAM);
	if (keyword->str.len > MAX_KEY_LENGTH)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, keyword->str.len, MAX_KEY_LENGTH);
	if (!keyword->str.len)
	{	rts_error(VARLSTCNT(6) ERR_DVIKEYBAD, 4, device->str.len, device->str.addr, 4, "NULL");
	}

	lower_to_upper(&buff[0], keyword->str.addr, keyword->str.len);
	upper_case = buff;
	if ( device->str.len == 0 || (device->str.len == 1 && *device->str.addr == '0'))
	{	device_name.dsc$a_pointer = "SYS$INPUT";
		device_name.dsc$w_length = SIZEOF("SYS$INPUT")-1;
	}
	else
	{	device_name.dsc$a_pointer = device->str.addr;
		device_name.dsc$w_length = device->str.len;
	}
	item_list.bufflen = VAL_LENGTH;
	item_list.itmcode = SPL_CODE;
	item_list.buffaddr = &out_value;
	item_list.retlen = &out_len;
	item_list.end = NULL;
	status = sys$getdvi( efn_immed_wait, 0, &device_name, &item_list, &iosb[0], 0, 0, 0 );
	if (status != SS$_NORMAL && status != SS$_NONLOCAL)
	{	rts_error(VARLSTCNT(1)  status ) ;
	}
	sys$synch(efn_immed_wait, &iosb[0]);
	if (iosb[0] != SS$_NORMAL && iosb[0] != SS$_NONLOCAL)
	{	rts_error(VARLSTCNT(1)  iosb[0] );
	}
	if (out_value != NULL)
	{	want_secondary = TRUE;
	}
	else
	{	want_secondary = FALSE;
	}

	if ((index = *upper_case - 'A') < MIN_INDEX || index > MAX_INDEX)
	{	rts_error(VARLSTCNT(6) ERR_DVIKEYBAD, 4, device->str.len, device->str.addr, keyword->str.len, keyword->str.addr);
	}
	item_code = 0;
	if ( dvi_index[ index ].len)
	{
		slot = dvi_index[ index ].index;
		last_slot = dvi_index[ index ].len;
		for ( ; slot < last_slot ; slot++ )
		{	if (keyword->str.len == dvi_table[ slot ].len &&
				!memcmp(dvi_table[ slot ].name, upper_case, keyword->str.len))
			{	item_code = dvi_table[ slot ].item_code;
				break;
			}
		}
	}
	if (!item_code)
	{	rts_error(VARLSTCNT(6) ERR_DVIKEYBAD, 4, device->str.len, device->str.addr, keyword->str.len, keyword->str.addr);
	}

	switch( item_code )
	{
	/* **** the following item codes require a string be returned **** */
	case DVI$_ALLDEVNAM:
	case DVI$_DEVLOCKNAM:
	case DVI$_DEVNAM:
	case DVI$_FULLDEVNAM:
	case DVI$_LOGVOLNAM:
	case DVI$_NEXTDEVNAM:
	case DVI$_ROOTDEVNAM:
	case DVI$_TT_ACCPORNAM:
	case DVI$_TT_PHYDEVNAM:
	case DVI$_VOLNAM:
		if (want_secondary)
		{
			if (!((item_code == DVI$_DEVNAM) && (keyword->str.len == 9)))
			{	item_code |= DVI$C_SECONDARY;
			}
		}
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
		ENSURE_STP_FREE_SPACE(MAX_DVI_STRLEN);
		item_list.bufflen = MAX_DVI_STRLEN;
		item_list.itmcode = item_code;
		item_list.buffaddr = stringpool.free;
		item_list.retlen = &out_len;
		item_list.end = NULL;
		status = sys$getdvi( efn_immed_wait, 0, &device_name, &item_list, &iosb[0], 0, 0, 0 );
		if (status != SS$_NORMAL && status != SS$_NONLOCAL)
		{		rts_error(VARLSTCNT(1)  status );
		}
		sys$synch(efn_immed_wait, &iosb[0]);
		if (iosb[0] != SS$_NORMAL && iosb[0] != SS$_NONLOCAL)
		{		rts_error(VARLSTCNT(1)  iosb[0] ) ;
		}
		ret->str.addr = stringpool.free;
		ret->str.len = out_len;
		ret->mvtype = MV_STR;
		stringpool.free += out_len;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.top >= stringpool.free);
		return;

	default:
		if (want_secondary)
			item_code |= DVI$C_SECONDARY;
		item_list.itmcode = item_code;
		item_list.bufflen = VAL_LENGTH;
		item_list.buffaddr = &out_value;
		item_list.retlen = &out_len;
		item_list.end = NULL;
		status = sys$getdvi( efn_immed_wait, 0, &device_name, &item_list, &iosb[0], 0, 0, 0 );
		if (status != SS$_NORMAL && status != SS$_NONLOCAL)
			rts_error(VARLSTCNT(1)  status );
		sys$synch(efn_immed_wait, &iosb[0]);
		if (iosb[0] != SS$_NORMAL && iosb[0] != SS$_NONLOCAL)
			rts_error(VARLSTCNT(1)  iosb[0] );
		if (want_secondary)
			item_code = item_code - 1;
		switch(item_code)
		{	case DVI$_LOCKID:
			case DVI$_ACPPID:
			case DVI$_OWNUIC:
			if (out_value)
			{	assert(stringpool.free >= stringpool.base);
				assert(stringpool.top >= stringpool.free);
				ENSURE_STP_FREE_SPACE(HEX_LEN);
				i2hex(out_value, stringpool.free, HEX_LEN);
				ret->str.addr = stringpool.free;
				ret->str.len = HEX_LEN;
				stringpool.free += HEX_LEN;
				assert(stringpool.free >= stringpool.base);
				assert(stringpool.top >= stringpool.free);
			}
			else
			{	ret->str.addr = "";
				ret->str.len = 0;
			}
			ret->mvtype = MV_STR;
			break;
		case DVI$_ACPTYPE:
			switch(out_value)
			{
			case 0: ret->str.addr = "ILLEGAL";
				ret->str.len = 7;
				break;
			case 1: ret->str.addr = "F11V1";
				ret->str.len = 5;
				break;
			case 2: ret->str.addr = "F11V2";
				ret->str.len = 5;
				break;
			case 3: ret->str.addr = "MTA";
				ret->str.len = 3;
				break;
			case 4: ret->str.addr = "NET";
				ret->str.len = 3;
				break;
			case 5: ret->str.addr = "REM";
				ret->str.len = 3;
			}
			ret->mvtype = MV_STR;
			break;
		default:
			i2mval(ret,out_value) ;
		}
		return;
	}
}
