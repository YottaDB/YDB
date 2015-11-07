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

#include <descrip.h>
#include <fab.h>
#include <rab.h>

#include "gtmidef.h"

#ifdef __ALPHA
#include <builtins.h>
#else
#pragma builtins
#endif

#define memcpy(DST,SRC,LEN) _MOVC3((unsigned short) (LEN), (char *) (SRC), (char *) (DST))
#define memset(DST, VALUE, LEN) _MOVC5(0, (char *)0, VALUE, LEN, (char *)(DST))

typedef int4 condition_code;
#ifdef TRACE_API
/*******debug only*****/
#define TRACE(X) dsmapi_out_write(X, SIZEOF(X) - 1)
#define TRACE_NUM(X) dsmapi_out_hex(X)
#define TRACE_DESC(X) (TRACE("Descriptor Length/Value:"), \
	TRACE_NUM((X).dsc$w_length), \
	dsmapi_out_write((X).dsc$a_pointer, (X).dsc$w_length))
/*****debug above this ****/
#else
#define TRACE(X)
#define TRACE_NUM(X)
#define TRACE_DESC(X)
#endif
void dsmapi_out_write();

#define CHECK_ARGUMENT_COUNT(ARG_CNT, FIRST_ARG, MIN, MAX) \
	(ARG_CNT) = *(((int *) (&FIRST_ARG)) - 1); \
	if ((ARG_CNT) > (MAX) || (ARG_CNT) < (MIN)) \
		return DSM$_CI_ARGCNT
#define CHECK_SDB(SDB) if ((SDB)->sdb_size != DSM$K_SDB_SIZE) return DSM$_CI_SDB
#define CHECK_GDB(GDB) if ((GDB)->gdb_size != DSM$K_GDB_SIZE) return DSM$_CI_GDB
#define CHECK_STATUS(STAT) if (((STAT) & 1) == 0) return (STAT)
#define CHECK_GTM_STATUS(STAT) if (((STAT) & 1) == 0)\
	{\
		if (STAT == ERR_UNDEF || STAT == ERR_GVUNDEF)\
			STAT = DSM$_UNDEF;\
		else if (STAT == ERR_MAXNRSUBSCRIPTS || STAT == ERR_GVSUBOFLOW)\
			STAT = DSM$_INVSUBSCR;\
		else if (STAT == ERR_GVNAKED || STAT == ERR_GVNAKEDEXTNM)\
			STAT = DSM$_NAKEDERR;\
		else if (STAT == ERR_LVNULLSUBS || STAT == ERR_NULSUBSC)\
			STAT = DSM$_NULLSUBSCR;\
		else if (STAT == ERR_IFNOTINIT)\
			STAT = DSM$_CI_NOTINIT;\
		else if (STAT == ERR_VAREXPECTED)\
			STAT = DSM$_NAME;\
		return STAT;\
	}
#define GBLDIR_PREFIX "GTMAPI$"
#define DSM$DK_NAME_LENGTH 8
globalvalue DSM$K_NAME_LENGTH = DSM$DK_NAME_LENGTH;		/* Max length of global/local name*/
#define DSM$DK_NUMBER_SUBSCRIPT 25
globalvalue DSM$K_NUMBER_SUBSCRIPT = DSM$DK_NUMBER_SUBSCRIPT;	/* Max # of subscripts */
globalvalue DSM$K_STRING_LENGTH = 512;		/* Max string length */
globalvalue DSM$K_SUBSCRIPT_LENGTH = 245;	/* Max length of a subscript */
globalvalue DSM$K_UCI_NAME_LENGTH = 3;		/* UCI name length */
#define DSM$DK_VOLSET_NAME_LENGTH 3
globalvalue DSM$K_VOLSET_NAME_LENGTH = DSM$DK_VOLSET_NAME_LENGTH;	/* Volume set name length */

#define DSM$M_SIBLING 1
#define DSM$M_PREVIOUS 2

/* Return status indicators from the callable routines */
/* Theirs */
globalvalue DSM$_ALLOC		= 0x007F800A;
globalvalue DSM$_CI_GDB		= 0x007F8092;
globalvalue DSM$_ARGERR		= 0x007F8012;
globalvalue DSM$_CI_ITEMBUFLEN	= 0x007F809A;
globalvalue DSM$_CI_ITEMINSUF	= 0x007F80A2;
globalvalue DSM$_CI_ITEMTYPE	= 0x007F80AA;
globalvalue DSM$_CI_LOCKMODE	= 0x007F80B2;
globalvalue DSM$_CI_NOSIBLING	= 0x007F80B8;
globalvalue DSM$_CI_NOTINIT	= 0x007F80C2;
globalvalue DSM$_CI_SDB		= 0x007F80CA;
globalvalue DSM$_CI_SUBPOS	= 0x007F80D2;
globalvalue DSM$_CI_ACTIVE	= 0x007F8082;
globalvalue DSM$_CI_ARGCNT	= 0x007F808A;
globalvalue DSM$_INVSUBSCR	= 0x007F8542;
globalvalue DSM$_FORCEXIT	= 0x007F82A4;
globalvalue DSM$_NAKEDERR	= 0x007F862A;
globalvalue DSM$_NAME		= 0x007F863A;
globalvalue DSM$_NOSUCHUCI	= 0x007F86A2;
globalvalue DSM$_NOSUCHVOLSET	= 0x007F86AA;
globalvalue DSM$_NOTRUN		= 0x007F86C2;
globalvalue DSM$_STRLEN		= 0x007F885A;
globalvalue DSM$_SUCCESS	= 0x007F8861;
globalvalue DSM$_NULLSUBSCR	= 0x007F86DA;
globalvalue DSM$_SYNTAX		= 0x007F886A;
globalvalue DSM$_TIMEOUT	= 0x007F889A;
globalvalue DSM$_TPABORT	= 0x007F88A2;
globalvalue DSM$_TPNORU		= 0x007F88C2;
globalvalue DSM$_TPNOTCONF	= 0x007F88CA;
globalvalue DSM$_TPROTRAN	= 0x007F88FA;
globalvalue DSM$_PROT		= 0x007F8772;
globalvalue DSM$_UNDEF		= 0x007F890A;
globalvalue DSM$_RULEVEL	= 0x007F87C2;
globalvalue DSM$_RUNDOWN	= 0x007F87D0;

/* Ours */
#define error_def(x) globalvalue x
error_def(ERR_UNIMPLOP);
error_def(ERR_GVUNDEF);
error_def(ERR_UNDEF);
error_def(ERR_IFNOTINIT);
error_def(ERR_VAREXPECTED);
error_def(ERR_LVNULLSUBS);
error_def(ERR_NULSUBSC);
error_def(ERR_GVNAKED);
error_def(ERR_GVNAKEDEXTNM);
error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_GVSUBOFLOW);

/* Global Data Block definition */
struct gdb
{
	int4 gdb_size;				/* length of gdb in longwords */
	int4 name_length;			/* length of global name */
						/* If zero, this is a global reference */
	char global_name[DSM$DK_NAME_LENGTH];	/* global name */
	char uci_nlen;
	char uci_name[DSM$DK_NAME_LENGTH];	/* uci name */
	char volset_nlen;
	char volset_name[DSM$DK_VOLSET_NAME_LENGTH]; /* volume set name */
};

globalvalue DSM$K_GDB_SIZE = 11;	/* Number of longwords in a GDB */

/* SDB data block definition */
struct sdb
{
	int4 sdb_size;		/* length of sdb in longwords */
	int4 sdb_count;		/* subscript count */
	struct dsc$descriptor_s subscripts[DSM$DK_NUMBER_SUBSCRIPT]; /* array of subscripts */
};

globalvalue DSM$K_SDB_SIZE = 90;	/* number of longwords in a SDB */

static struct dsc$descriptor_s return_string = {0, DSC$K_DTYPE_T, DSC$K_CLASS_D, 0 };
static struct dsc$descriptor_s global_name_desc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
static char global_directory_buffer[24];
static struct dsc$descriptor_s global_directory_desc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, global_directory_buffer };

extern condition_code gtm$gblget(), gtm$get(), gtm$gblorder(), gtm$gbldata(), gtm$init(),
	gtm$kill(), gtm$put(), gtm$xecute();

struct dsc$descriptor_s *setup_gname(global_data_block)
struct gdb *global_data_block;
{
	if (global_data_block->name_length == 0)
		return 0;
	global_name_desc.dsc$w_length = global_data_block->name_length;
	global_name_desc.dsc$a_pointer = global_data_block->global_name;
	return &global_name_desc;
}

struct dsc$descriptor_s *setup_gdir(global_data_block)
struct gdb *global_data_block;
{
	char *cp;

	if (global_data_block->uci_nlen != 3 || global_data_block->volset_nlen != 3)
		return 0;
	memcpy(global_directory_buffer, GBLDIR_PREFIX, SIZEOF(GBLDIR_PREFIX) - 1);
	cp = global_directory_buffer + SIZEOF(GBLDIR_PREFIX) - 1;
	memcpy(cp, global_data_block->uci_name, SIZEOF(global_data_block->uci_name));
	cp += SIZEOF(global_data_block->uci_name);
	*cp++ = '$';
	memcpy(cp, global_data_block->volset_name, SIZEOF(global_data_block->volset_name));
	cp += SIZEOF(global_data_block->volset_name);
	global_directory_desc.dsc$w_length = cp - global_directory_buffer;
	return &global_directory_desc;
}

condition_code check_3_alphas(cp)
char *cp;
{
	int i, ch;

	for (i = 0 ; i < 3 ; i++)
	{
		ch = *cp++;
		if (ch < 'A' || ch > 'Z')
			return DSM$_NOSUCHUCI;
	}
	return DSM$_SUCCESS;
}

condition_code dsm$dsm(command_line)
struct dsc$descriptor_s *command_line;
{
	int arg_cnt;

	CHECK_ARGUMENT_COUNT(arg_cnt, command_line, 1, 1);
TRACE("at DSM$DSM");
	return ERR_UNIMPLOP;
}

condition_code dsm$gdb_clear(global_data_block)
struct gdb *global_data_block;
{
	int arg_cnt;

TRACE("condition_code dsm$gdb_clear(global_data_block)");
	CHECK_ARGUMENT_COUNT(arg_cnt, global_data_block, 1, 1);
	CHECK_GDB(global_data_block);
	global_data_block->name_length = 0;
	global_data_block->uci_nlen = 0;
	global_data_block->volset_nlen = 0;
	return DSM$_SUCCESS;
}

condition_code dsm$gdb_create(gdb_address)
struct gdb **gdb_address;
{
	struct gdb *global_data_block;
	condition_code status;
	int arg_cnt;

TRACE("condition_code dsm$gdb_create(gdb_address)");
	CHECK_ARGUMENT_COUNT(arg_cnt, gdb_address, 1, 1);
	status = lib$get_vm(&SIZEOF(*global_data_block), &global_data_block, 0);
	CHECK_STATUS(status);
	*gdb_address = global_data_block;
	memset(global_data_block, 0, SIZEOF(*global_data_block));
	global_data_block->gdb_size = DSM$K_GDB_SIZE;
	return DSM$_SUCCESS;
}

condition_code dsm$gdb_initialize(gdbsiz, global_data_block)
uint4 *gdbsiz;
struct gdb *global_data_block;
{
	int arg_cnt;

TRACE("condition_code dsm$gdb_initialize(gdbsiz, global_data_block)");
	CHECK_ARGUMENT_COUNT(arg_cnt, gdbsiz, 2, 2);
	if (*gdbsiz != DSM$K_GDB_SIZE)
		lib$signal(DSM$_CI_GDB);
	memset(global_data_block, 0, SIZEOF(*global_data_block));
	global_data_block->gdb_size = DSM$K_GDB_SIZE;
	return DSM$_SUCCESS;
}

condition_code dsm$gdb_extract(global_data_block, global_name, global_name_length, uci_name, volume_set_name)
struct gdb *global_data_block;
struct dsc$descriptor_s *global_name;
unsigned short *global_name_length;
struct dsc$descriptor_s *uci_name;
struct dsc$descriptor_s *volume_set_name;
{
	int arg_cnt;
	struct dsc$descriptor_s	auto_string;
	condition_code status;

TRACE("condition_code dsm$gdb_extract(global_data_block, ...");
	CHECK_ARGUMENT_COUNT(arg_cnt, global_data_block, 2, 5);
	CHECK_GDB(global_data_block);
	auto_string.dsc$w_length = global_data_block->name_length;
	auto_string.dsc$b_dtype = DSC$K_DTYPE_T;
	auto_string.dsc$b_class = DSC$K_CLASS_S;
	auto_string.dsc$a_pointer = global_data_block->global_name;
	status = lib$scopy_dxdx(&auto_string, global_name);
	CHECK_STATUS(status);
	if (arg_cnt > 2)
	{
		if (global_name_length)
			*global_name_length = global_data_block->name_length;
		if (arg_cnt > 3)
		{
			if (uci_name)
			{
				auto_string.dsc$w_length = global_data_block->uci_nlen;
				auto_string.dsc$a_pointer = global_data_block->uci_name;
				status = lib$scopy_dxdx(&auto_string, uci_name);
				CHECK_STATUS(status);
			}
			if (arg_cnt > 4)
			{
				if (volume_set_name)
				{
					auto_string.dsc$w_length = global_data_block->volset_nlen;
					auto_string.dsc$a_pointer = global_data_block->volset_name;
					status = lib$scopy_dxdx(&auto_string, volume_set_name);
					CHECK_STATUS(status);
				}
			}
		}
	}
	return DSM$_SUCCESS;
}

condition_code dsm$gdb_insert(global_name, global_data_block, uci_name, volume_set_name)
struct dsc$descriptor_s *global_name;
struct gdb *global_data_block;
struct dsc$descriptor_s *uci_name;
struct dsc$descriptor_s *volume_set_name;
{
	int arg_cnt;
	condition_code status;
	int n;
	char *cin, *cout;

TRACE("condition_code dsm$gdb_insert(global_name, ...");
	CHECK_ARGUMENT_COUNT(arg_cnt, global_name, 2, 4);
	CHECK_GDB(global_data_block);
	n = global_name->dsc$w_length;
	if (n < 1 || n > DSM$K_NAME_LENGTH)
		return DSM$_NAME;
	global_data_block->name_length = n;
	for (cin = global_name->dsc$a_pointer, cout = global_data_block->global_name ; n-- > 0 ; )
		*cout++ = *cin++;
	if (arg_cnt > 2 && uci_name)
	{
		status = check_3_alphas(uci_name);
		if ((status & 1) == 0)
			return DSM$_NOSUCHUCI;
		for (cin = uci_name->dsc$a_pointer, cout = global_data_block->uci_name , n = SIZEOF(global_data_block->uci_name);
			 n-- > 0 ; )
			*cout++ = *cin++;
	}
	if (arg_cnt > 3 && volume_set_name)
	{
		status = check_3_alphas(volume_set_name);
		if ((status & 1) == 0)
			return DSM$_NOSUCHVOLSET;
		for (cin = volume_set_name->dsc$a_pointer,
			 cout = global_data_block->volset_name , n = SIZEOF(global_data_block->volset_name);
			 n-- > 0 ; )
			*cout++ = *cin++;
	}
	return DSM$_SUCCESS;
}

condition_code dsm$gdb_free(global_data_block)
struct gdb *global_data_block;
{
	int arg_cnt;
	condition_code status;

TRACE("condition_code dsm$gdb_free(global_data_block)");
	CHECK_ARGUMENT_COUNT(arg_cnt, global_data_block, 1, 1);
	CHECK_GDB(global_data_block);
	status = lib$free_vm(&SIZEOF(*global_data_block), &global_data_block, 0);
	CHECK_STATUS(status);
	return DSM$_SUCCESS;
}

condition_code dsm$global_$data(global_data_block, subscript_data_block, dvalue)
struct gdb *global_data_block;
struct sdb *subscript_data_block;
uint4 *dvalue;
{
	int arg_cnt;
	condition_code status;
	struct dsc$descriptor *gname_desc;
	struct dsc$descriptor *gdir_desc;
	struct dsc$descriptor return_data_value;
/***************DEBUG **************/
#ifdef TRACE_API
int4 *ip, nnn;
ip = subscript_data_block;
TRACE("SDB HEADER VALUE: ");
TRACE_NUM(ip);
TRACE_NUM(*ip);
for (nnn = 0 ; nnn < subscript_data_block->sdb_count ; nnn++)
	TRACE_DESC(subscript_data_block->subscripts[nnn]);
/************END DEBUG **********/
TRACE("condition_code dsm$global_$data(global_data_block, ...");
#endif
	CHECK_ARGUMENT_COUNT(arg_cnt, global_data_block, 3, 3);
	CHECK_GDB(global_data_block);
	CHECK_SDB(subscript_data_block);
	gname_desc = setup_gname(global_data_block);
	gdir_desc = setup_gdir(global_data_block);
	return_data_value.dsc$w_length = SIZEOF(int4);
	return_data_value.dsc$b_dtype = DSC$K_DTYPE_L;
	return_data_value.dsc$b_class =  DSC$K_CLASS_S;
	return_data_value.dsc$a_pointer = dvalue;
	status = dsm_api_dispatch(gtm$gbldata, &return_data_value, gdir_desc, gname_desc, subscript_data_block->sdb_count,
		subscript_data_block->subscripts);
	CHECK_GTM_STATUS(status)
#ifdef TRACE_GLOBALS
display_global_ref("$D", global_data_block, subscript_data_block);
display_int(*dvalue);
#endif
TRACE_DESC(return_data_value);
	return DSM$_SUCCESS;
}

condition_code dsm$global_$order(global_data_block, subscript_data_block,
	option_mask, sibling, sibling_length)
struct gdb *global_data_block;
struct sdb *subscript_data_block;
uint4 *option_mask;
struct dsc$descriptor_s *sibling;
unsigned short *sibling_length;
{
	int arg_cnt;
	condition_code status;
	struct dsc$descriptor *gname_desc;
	struct dsc$descriptor *gdir_desc;

TRACE("condition_code dsm$global_$order(global_data_block, ");
	CHECK_ARGUMENT_COUNT(arg_cnt, global_data_block, 3, 5);
	CHECK_GDB(global_data_block);
	CHECK_SDB(subscript_data_block);
	if (subscript_data_block->sdb_count < 1)
		return DSM$_NAKEDERR;
	if (*option_mask & 2)	/* Should be $ZPREVIOUS */
	{
TRACE("At $ZPREVIOUS");
		lib$signal(ERR_UNIMPLOP);
	}
	gname_desc = setup_gname(global_data_block);
	gdir_desc = setup_gdir(global_data_block);
	status = dsm_api_dispatch(gtm$gblorder, &return_string, gdir_desc, gname_desc, subscript_data_block->sdb_count,
		subscript_data_block->subscripts);
	CHECK_GTM_STATUS(status)
	if (return_string.dsc$w_length == 0)
	{
		status = DSM$_CI_NOSIBLING;
TRACE("Found NOSIBLING");
		if (*option_mask & 1)
			subscript_data_block->sdb_count--;
#ifdef TRACE_GLOBALS
display_global_ref("$O", global_data_block, subscript_data_block);
display_str("=<null>");
#endif
	}
	else
	{
		status = DSM$_SUCCESS;
		if (*option_mask & 1)
TRACE_DESC(return_string);
		{
TRACE("option_mask is odd, performining scopy_dxdxto subscript_data_block");
			lib$scopy_dxdx(&return_string, &subscript_data_block->subscripts[subscript_data_block->sdb_count - 1]);
		}
		if (arg_cnt > 3 && sibling)
		{
			lib$scopy_dxdx(&return_string, sibling);
			if (arg_cnt > 4 && sibling_length)
			{
				*sibling_length = (return_string.dsc$w_length < sibling->dsc$w_length)
				? return_string.dsc$w_length : sibling->dsc$w_length;
			}
		}
#ifdef TRACE_GLOBALS
display_global_ref("$O", global_data_block, subscript_data_block);
display_descript(&return_string, return_string.dsc$w_length);
#endif
	}
	lib$sfree1_dd(&return_string);
	return status;
}

condition_code dsm$global_get_t(global_data_block, subscript_data_block, get_value, get_value_length)
struct gdb *global_data_block;
struct sdb *subscript_data_block;
struct dsc$descriptor_s *get_value;
unsigned short *get_value_length;
{
	int arg_cnt;
	condition_code status;
	struct dsc$descriptor *gname_desc;
	struct dsc$descriptor *gdir_desc;

TRACE("condition_code dsm$global_get_t(global_data_block, ...");
	CHECK_ARGUMENT_COUNT(arg_cnt, global_data_block, 3, 4);
	CHECK_GDB(global_data_block);
	CHECK_SDB(subscript_data_block);
	gname_desc = setup_gname(global_data_block);
	gdir_desc = setup_gdir(global_data_block);
	status = dsm_api_dispatch(gtm$gblget, &return_string, gdir_desc, gname_desc, subscript_data_block->sdb_count,
		subscript_data_block->subscripts);
	CHECK_GTM_STATUS(status)
#ifdef TRACE_GLOBALS
display_global_ref("G ", global_data_block, subscript_data_block);
display_descript(&return_string, return_string.dsc$w_length);
#endif
	TRACE_DESC(return_string);
	if (arg_cnt > 3 && get_value_length)
	{
		*get_value_length = (return_string.dsc$w_length < get_value->dsc$w_length)
		    ? return_string.dsc$w_length : get_value->dsc$w_length;
	}
	lib$scopy_dxdx(&return_string, get_value);
	lib$sfree1_dd(&return_string);
	return DSM$_SUCCESS;
}

condition_code dsm$initialize(command_line)
struct dsc$descriptor_s *command_line;
{
	int arg_cnt;
	condition_code status;

TRACE("condition_code dsm$initialize(command_line)");
	CHECK_ARGUMENT_COUNT(arg_cnt, command_line, 0, 1);
	if (arg_cnt > 0 && command_line != 0 && command_line->dsc$w_length != 0)
	{
TRACE("AT DSM$INITIALIZE");
 	dsmapi_out_write(command_line->dsc$a_pointer, command_line->dsc$w_length);
		return ERR_UNIMPLOP;
	}
	status = gtm$init();
	CHECK_GTM_STATUS(status)
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_clear(subscript_data_block)
struct sdb *subscript_data_block;
{
	int arg_cnt;

TRACE("condition_code dsm$sdb_clear(subscript_data_block)");
	CHECK_ARGUMENT_COUNT(arg_cnt, subscript_data_block, 1, 1);
	CHECK_SDB(subscript_data_block);
	subscript_data_block->sdb_count = 0;
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_count(subscript_data_block, count)
struct sdb *subscript_data_block;
uint4 *count;
{
	int arg_cnt;

TRACE("condition_code dsm$sdb_count(subscript_data_block, count)");
	CHECK_ARGUMENT_COUNT(arg_cnt, subscript_data_block, 2, 2);
	CHECK_SDB(subscript_data_block);
	*count = subscript_data_block->sdb_count;
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_create(sdb_address)
struct sdb **sdb_address;
{
	struct sdb *subscript_data_block;
	condition_code status;
	int arg_cnt, dsm_sdb_size;

TRACE("condition_code dsm$sdb_create(sdb_address)");
	CHECK_ARGUMENT_COUNT(arg_cnt, sdb_address, 1, 1);
	status = lib$get_vm(&SIZEOF(*subscript_data_block), &subscript_data_block, 0);
	CHECK_STATUS(status);
	*sdb_address = subscript_data_block;
	memset(subscript_data_block, 0, SIZEOF(*subscript_data_block));
	dsm_sdb_size = DSM$K_SDB_SIZE;
	status = dsm$sdb_initialize(&dsm_sdb_size, subscript_data_block);
	CHECK_STATUS(status);
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_extract_one_t(subscript_data_block, position, sub_value, sub_value_length)
struct sdb *subscript_data_block;
uint4 *position;
struct dsc$descriptor_s *sub_value;
unsigned short *sub_value_length;
{
	int arg_cnt;
	condition_code status;
	int pos;

TRACE("condition_code dsm$sdb_extract_one_t(subscript_data_block, ...");
	CHECK_ARGUMENT_COUNT(arg_cnt, subscript_data_block, 3, 4);
	CHECK_SDB(subscript_data_block);
	pos = *position - 1;
	if (pos < 0 || pos > subscript_data_block->sdb_count + 1)
		return DSM$_CI_SUBPOS;
	status = lib$scopy_dxdx(&subscript_data_block->subscripts[pos], sub_value);
	CHECK_STATUS(status);
	if (arg_cnt > 3 && sub_value_length)
		*sub_value_length = subscript_data_block->subscripts[pos].dsc$w_length;
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_free(subscript_data_block)
struct sdb *subscript_data_block;
{
	int arg_cnt, dsm_number_subscript;
	condition_code status;

TRACE("condition_code dsm$sdb_free(subscript_data_block)");
	CHECK_ARGUMENT_COUNT(arg_cnt, subscript_data_block, 1, 1);
	CHECK_SDB(subscript_data_block);
	dsm_number_subscript = DSM$K_NUMBER_SUBSCRIPT;
	status = lib$sfreen_dd(&dsm_number_subscript, subscript_data_block->subscripts);
	CHECK_STATUS(status);
	status = lib$free_vm(&SIZEOF(*subscript_data_block), &subscript_data_block, 0);
	CHECK_STATUS(status);
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_initialize(sdbsize, subscript_data_block)
int4 *sdbsize;
struct sdb *subscript_data_block;
{
	int arg_cnt;
	condition_code status;
	int n;

TRACE("condition_code dsm$sdb_initialize(sdbsize, subscript_data_block)");
	CHECK_ARGUMENT_COUNT(arg_cnt, sdbsize, 2, 2);
	if (*sdbsize != DSM$K_SDB_SIZE)
		return DSM$_CI_SDB;
	subscript_data_block->sdb_size = DSM$K_SDB_SIZE;
	subscript_data_block->sdb_count = 0;
	for (n = 0 ; n < DSM$K_NUMBER_SUBSCRIPT ; n++)
	{
		subscript_data_block->subscripts[n].dsc$w_length = 0;
		subscript_data_block->subscripts[n].dsc$b_dtype = DSC$K_DTYPE_T;
		subscript_data_block->subscripts[n].dsc$b_class = DSC$K_CLASS_D;
		subscript_data_block->subscripts[n].dsc$a_pointer = 0;
	}
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_insert_null(position, subscript_data_block)
uint4 *position;
struct sdb *subscript_data_block;
{
	int arg_cnt;
	condition_code status;
	int pos;

TRACE("condition_code dsm$sdb_insert_null(position, subscript_data_block)");
TRACE("Null inserted at position");
TRACE_NUM(*position);
	CHECK_ARGUMENT_COUNT(arg_cnt, position, 2, 2);
	CHECK_SDB(subscript_data_block);
	pos = *position - 1;
	if (pos < 0 || pos > subscript_data_block->sdb_count + 1)
		return DSM$_CI_SUBPOS;
	subscript_data_block->subscripts[pos].dsc$w_length = 0;
	subscript_data_block->sdb_count = pos + 1;
	return DSM$_SUCCESS;
}

condition_code dsm$sdb_insert_one_t(position, sub_value, subscript_data_block)
uint4 *position;
struct dsc$descriptor_s *sub_value;
struct sdb *subscript_data_block;
{
	int arg_cnt;
	condition_code status;
	int pos;

TRACE("condition_code dsm$sdb_insert_one_t(position, ...");
TRACE("Position/Value");
TRACE_NUM(*position);
TRACE_DESC(*sub_value);
	CHECK_ARGUMENT_COUNT(arg_cnt, position, 3, 3);
	CHECK_SDB(subscript_data_block);
	pos = *position - 1;
	if (pos < 0 || pos > subscript_data_block->sdb_count + 1)
		return DSM$_CI_SUBPOS;
	status = lib$scopy_dxdx(sub_value, &subscript_data_block->subscripts[pos]);
	CHECK_STATUS(status);
	if (pos >= subscript_data_block->sdb_count)
	subscript_data_block->sdb_count = pos + 1;
	/* Note check here for maximum depth! */
	return DSM$_SUCCESS;
}

condition_code dsm$xecute(mumps_code)
struct dsc$descriptor_s *mumps_code;
{
	int arg_cnt;
	condition_code status;

	CHECK_ARGUMENT_COUNT(arg_cnt, mumps_code, 1, 1);
	status = gtm$xecute(mumps_code);
	CHECK_GTM_STATUS(status)
	return DSM$_SUCCESS;
}

condition_code dsm$rundown()
{
TRACE("condition_code dsm$rundown()");
	return DSM$_SUCCESS;
}

condition_code dsm$local_get(local_name, get_value, get_value_length)
struct dsc$descriptor_s *local_name;
struct dsc$descriptor_s *get_value;
unsigned short *get_value_length;
{
	int arg_cnt;
	condition_code status;

TRACE("condition_code dsm$local_get(local_name, ...");
	CHECK_ARGUMENT_COUNT(arg_cnt, local_name, 2, 3);
	status = gtm$get(GTMI$_LOCAL, &return_string, local_name);
	CHECK_GTM_STATUS(status)
#ifdef TRACE_GLOBALS
display_descript(&local_name, local_name.dsc$w_length);
display_descript(&return_string, return_string.dsc$w_length);
#endif
	TRACE_DESC(return_string);
	if (arg_cnt > 2 && get_value_length)
	{
		*get_value_length = (return_string.dsc$w_length < get_value->dsc$w_length)
		    ? return_string.dsc$w_length : get_value->dsc$w_length;
	}
	lib$scopy_dxdx(&return_string, get_value);
	lib$sfree1_dd(&return_string);
	return DSM$_SUCCESS;
}

condition_code dsm$local_set(local_name, set_value)
struct dsc$descriptor_s *local_name;
struct dsc$descriptor_s *set_value;
{
	int arg_cnt;
	condition_code status;

TRACE("condition_code dsm$local_set(local_name, ...");
	CHECK_ARGUMENT_COUNT(arg_cnt, local_name, 2, 2);
	status = gtm$put(GTMI$_LOCAL, set_value, local_name);
	CHECK_GTM_STATUS(status)
	return DSM$_SUCCESS;
}

condition_code dsm$local_kill(local_name)
struct dsc$descriptor_s *local_name;
{
	int arg_cnt;
	condition_code status;

TRACE("condition_code dsm$local_set(local_name, ...");
	CHECK_ARGUMENT_COUNT(arg_cnt, local_name, 1, 1);
	status = gtm$kill(GTMI$_LOCAL, local_name);
	CHECK_GTM_STATUS(status)
	return DSM$_SUCCESS;
}

#ifdef TRACE_GLOBALS
static unsigned char tg_buffer[132];
static unsigned char *tgb;

display_global_ref(typ, g, s)
unsigned char *typ;
struct gdb *g;
struct sdb *s;
{
	int i, j;
	unsigned char *cp;

	tgb = tg_buffer;
	while (*typ)
		*tgb++ = *typ++;
	*tgb++ = ':';
	*tgb++ = '^';
	cp = g->global_name;
	for (i = 0 ; i < g->name_length ; i++)
		*tgb++ = *cp++;
	if (s->sdb_count == 0)
		return;
	*tgb++ = '(';
	for (i = 0 ; i < s->sdb_count ; i++)
	{
		*tgb++ = '"';
		for (cp = s->subscripts[i].dsc$a_pointer, j = s->subscripts[i].dsc$w_length ; j-- > 0 ; )
			*tgb++ = *cp++;
		*tgb++ = '"';
		*tgb++ = ',';
	}
	*(tgb - 1) = ')';
	return;
}

display_int(n)
int n;
{
	*tgb++ = '=';
	if (n > 9)
		*tgb++ = '0' + ((n / 10) % 10);
	*tgb++ = '0' + (n % 10);
	display_dump();
}

display_descript(d, len)
struct dsc$descriptor_s *d;
int len;
{
	char *cp;
	int n;

	*tgb++ = '=';
	for (n = 0 , cp = d->dsc$a_pointer ; n++ < len; )
		*tgb++ = *cp++;
	display_dump();
}
display_dump()
{
	dsmapi_out_write(tg_buffer, tgb - tg_buffer);
}

display_str(s)
char *s;
{
	while (*tgb++ = *s++)
		;
	dsmapi_out_write(tg_buffer, tgb - tg_buffer);
}

#endif

static struct RAB *dsmapi_output_rab = 0;
static struct FAB *dsmapi_output_fab = 0;

static void dsmapi_out_open()
{
	uint4			status;

	dsmapi_output_fab = malloc(SIZEOF(*dsmapi_output_fab));
	dsmapi_output_rab = malloc(SIZEOF(*dsmapi_output_rab));
	*dsmapi_output_fab  = cc$rms_fab;
	*dsmapi_output_rab  = cc$rms_rab;
	dsmapi_output_rab->rab$l_fab = dsmapi_output_fab;
	dsmapi_output_rab->rab$w_usz = 255;
	dsmapi_output_fab->fab$w_mrs = 255;
	dsmapi_output_fab->fab$b_fac = FAB$M_GET | FAB$M_PUT;
	dsmapi_output_fab->fab$b_rat = FAB$M_CR;
	dsmapi_output_fab->fab$l_fna = "SYS$OUTPUT";
	dsmapi_output_fab->fab$b_fns = SIZEOF("SYS$OUTPUT") - 1;
	status = sys$create(dsmapi_output_fab, 0, 0);
	if ((status & 1) == 0)
		lib$signal(status);
	status = sys$connect(dsmapi_output_rab, 0, 0);
	if ((status & 1) == 0)
		lib$signal(status);
}

static void dsmapi_out_write(addr, len)
unsigned char *addr;
unsigned int len;
{
	if (!dsmapi_output_fab)
		dsmapi_out_open();
	dsmapi_output_rab->rab$l_rbf = addr;
	dsmapi_output_rab->rab$w_rsz = len;
	sys$put(dsmapi_output_rab,0 ,0);
	return;
}

static void dsmapi_out_hex(val)
int4 val;
{
	register char *cp;
	register unsigned int n;
	char x;
	char dest[8];

	n = val;
	cp = &dest[8];
	while (cp > dest)
	{
		x = n & 0xF;
		n >>= 4;
		*--cp = x + ((x > 9) ? 'A' - 10 : '0');
	}
	dsmapi_out_write(dest, SIZEOF(dest));
	return;
}

static void dsmapi_out_close()
{
	sys$close(dsmapi_output_fab, 0, 0);
	free(dsmapi_output_fab);
	free(dsmapi_output_rab);
	dsmapi_output_fab = 0;
	dsmapi_output_rab = 0;
	return;
}
