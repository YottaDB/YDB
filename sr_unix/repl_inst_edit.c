/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "cli.h"
#include "util.h"
#include "repl_instance.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"		/* for JNL_WHOLE_FROM_SHORT_TIME */
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_inst_dump.h"

GBLREF	boolean_t	in_repl_inst_edit;	/* used by an assert in repl_inst_read/repl_inst_write */
GBLREF	boolean_t	detail_specified;	/* set to TRUE if -DETAIL is specified */
GBLREF	uint4		section_offset;		/* Used by PRINT_OFFSET_PREFIX macro in repl_inst_dump.c */

error_def(ERR_MUPCLIERR);
error_def(ERR_SIZENOTVALID8);

void	mupcli_get_offset_size_value(uint4 *offset, uint4 *size, gtm_uint64_t *value, boolean_t *value_present)
{
	if (!cli_get_hex("OFFSET", offset))
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	if (!cli_get_hex("SIZE", size))
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	if (!((SIZEOF(char) == *size) || (SIZEOF(short) == *size) || (SIZEOF(int4) == *size) || (SIZEOF(gtm_int64_t) == *size)))
                rts_error(VARLSTCNT(1) ERR_SIZENOTVALID8);
	if (0 > (int4)*size)
	{
		util_out_print("Error: SIZE specified cannot be negative", TRUE);
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	}
	if (0 != (*offset % *size))
	{
		util_out_print("Error: OFFSET [0x!XL] should be a multiple of Size [!UL]", TRUE, *offset, *size);
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	}
	if (CLI_PRESENT == cli_present("VALUE"))
	{
		*value_present = TRUE;
		if (!cli_get_hex64("VALUE", value))
			rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	} else
		*value_present = FALSE;
}

void	mupcli_edit_offset_size_value(sm_uc_ptr_t buff, uint4 offset, uint4 size, gtm_uint64_t value, boolean_t value_present)
{
	char		temp_str[256], temp_str1[256];
	gtm_uint64_t	old_value;

	memset(temp_str, 0, 256);
	memset(temp_str1, 0, 256);
	if (SIZEOF(char) == size)
	{
		SPRINTF(temp_str, "!UB [0x!XB]");
		old_value = *(sm_uc_ptr_t)buff;
	}
	else if (SIZEOF(short) == size)
	{
		SPRINTF(temp_str, "!UW [0x!XW]");
		old_value = *(sm_ushort_ptr_t)buff;
	}
	else if (SIZEOF(int4) == size)
	{
		SPRINTF(temp_str, "!UL [0x!XL]");
		old_value = *(sm_uint_ptr_t)buff;
	}
	else if (SIZEOF(gtm_int64_t) == size)
	{
		SPRINTF(temp_str, "!@UQ [0x!@XQ]");
		old_value = *(qw_num_ptr_t)buff;
	}
	if (value_present)
	{
		if (SIZEOF(char) == size)
			*(sm_uc_ptr_t)buff = (unsigned char)value;
		else if (SIZEOF(short) == size)
			*(sm_ushort_ptr_t)buff = (unsigned short)value;
		else if (SIZEOF(int4) == size)
			*(sm_uint_ptr_t)buff = (unsigned int)value;
		else if (SIZEOF(gtm_int64_t) == size)
			*(qw_num_ptr_t)buff = value;
	} else
		value = old_value;
	SPRINTF(temp_str1, "Offset !UL [0x!XL] : Old Value = %s : New Value = %s : Size = !UB [0x!XB]",
		temp_str, temp_str);
	if (SIZEOF(int4) >= size)
		util_out_print(temp_str1, TRUE, offset, offset, (uint4)old_value, (uint4)old_value,
			(uint4)value, (uint4)value, size, size);
	else
		util_out_print(temp_str1, TRUE, offset, offset, &old_value, &old_value,
			&value, &value, size, size);
}

/* Description:
 *	Edits or displays the contents of a replication instance file.
 * Parameters:
 	None
 * Return Value:
 	None
 */
void	repl_inst_edit(void)
{
	unsigned short		inst_fn_len, instname_len;
	char			inst_fn[MAX_FN_LEN + 1], buff_unaligned[REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE + 8];
	char			instname[MAX_INSTNAME_LEN];
	char			*buff;
	repl_inst_hdr_ptr_t	repl_instance;
	gtmsrc_lcl_ptr_t	gtmsrclcl_ptr;
	uint4			offset, size;
	gtm_uint64_t		value;
	boolean_t		value_present;

	in_repl_inst_edit = TRUE;
	inst_fn_len = MAX_FN_LEN;
	if (!cli_get_str("INSTFILE", inst_fn, &inst_fn_len) || (0 == inst_fn_len))
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	inst_fn[inst_fn_len] = '\0';
	buff = &buff_unaligned[0];
	buff = (char *)ROUND_UP2((INTPTR_T)buff, 8);
	if (CLI_PRESENT == cli_present("SHOW"))
	{
		detail_specified = (CLI_PRESENT == cli_present("DETAIL"));
		repl_inst_read(inst_fn, (off_t)0, (sm_uc_ptr_t)buff, REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
		util_out_print("GTM-I-MUREPLSHOW, SHOW output for replication instance file !AD", TRUE, inst_fn_len, inst_fn);
		repl_instance = (repl_inst_hdr_ptr_t)&buff[0];
		section_offset = 0;
		repl_inst_dump_filehdr(repl_instance);
		section_offset = REPL_INST_HDR_SIZE;
		repl_inst_dump_gtmsrclcl((gtmsrc_lcl_ptr_t)&buff[REPL_INST_HDR_SIZE]);
		section_offset = REPL_INST_HISTINFO_START;
		repl_inst_dump_history_records(inst_fn, repl_instance->num_histinfo);
	}
	if (CLI_PRESENT == cli_present("CHANGE"))
	{
		mupcli_get_offset_size_value(&offset, &size, &value, &value_present);
		assert(size <= REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
		repl_inst_read(inst_fn, (off_t)offset, (sm_uc_ptr_t)buff, size);
		mupcli_edit_offset_size_value((sm_uc_ptr_t)buff, offset, size, value, value_present);
		repl_inst_write(inst_fn, (off_t)offset, (sm_uc_ptr_t)buff, size);
	}
	if (CLI_PRESENT == cli_present("NAME"))
	{	/* Edit the instance name */
		instname_len = MAX_INSTNAME_LEN;
		assert(MAX_INSTNAME_LEN == SIZEOF(instname));
		if (!cli_get_str("NAME", instname, &instname_len))
			rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
		assert(MAX_INSTNAME_LEN >= instname_len);
		if (MAX_INSTNAME_LEN == instname_len)
		{
			util_out_print("Error: Instance name length can be at most 15", TRUE);
			rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
		}
		repl_inst_read(inst_fn, (off_t)0, (sm_uc_ptr_t)buff, REPL_INST_HDR_SIZE);
		repl_instance = (repl_inst_hdr_ptr_t)&buff[0];
		util_out_print("HDR Instance Name changing from !AZ to !AZ",
			TRUE, repl_instance->inst_info.this_instname, instname);
		assert('\0' == instname[instname_len]);
		memcpy(repl_instance->inst_info.this_instname, instname, instname_len + 1);
		repl_inst_write(inst_fn, (off_t)0, (sm_uc_ptr_t)buff, REPL_INST_HDR_SIZE);
	}
	in_repl_inst_edit = FALSE;
}
