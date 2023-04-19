/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "util.h"

/* Prototypes */
#include "gtmmsg.h"		/* for gtm_putmsg prototype */
#include "send_msg.h"		/* for send_msg */
#include "verify_db_format_change_request.h"
#include "wcs_phase2_commit_wait.h"

#define INELIGIBLE_UPGRADE(REG, CMD, NEWFMT, CURFMT)										\
MBSTART {															\
	util_out_print("Region !AD : is ineligible for !AZ to !AZ. !AD is currently !AZ.", TRUE,				\
			REG_LEN_STR(REG), CMD, gtm_dbversion_table[NEWFMT], DB_LEN_STR(REG), gtm_dbversion_table[CURFMT]);	\
	return ERR_MUNOUPGRD;													\
} MBEND

LITREF	char			*gtm_dbversion_table[];

error_def(ERR_MUNOUPGRD);
error_def(ERR_NOGTCMDB);

/* input parameter "command_name" is a string that is either "MUPIP REORG UPGRADE/DOWNGRADE" or "MUPIP SET VERSION" */
int4	verify_db_format_change_request(gd_region *reg, enum db_ver new_db_format, char *command_name)
{
	char			*db_fmt_str;
	char			*wcblocked_ptr;
	enum db_ver		cur_db_format;
	int4			status;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;

	assert(reg->open);
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cur_db_format = csd->desired_db_format;

	assert(GDSV6 < new_db_format);	/* Switching to V6 and before has been disallowed since V7. Nothing should request it */
	if (reg_cmcheck(reg))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_NOGTCMDB, 2, LEN_AND_STR(command_name), DB_LEN_STR(reg));
		return ERR_NOGTCMDB;
	}
	if (reg->read_only)
	{	/* Cannot proceed for read-only data files */
		util_out_print("Region !AD : is read-only ineligible for !AD", TRUE, REG_LEN_STR(reg), LEN_AND_STR(command_name));
		return ERR_MUNOUPGRD;
	}
	switch (cur_db_format)
	{
		case GDSV4:
			INELIGIBLE_UPGRADE(reg, command_name, new_db_format, cur_db_format);
		case GDSV6:
			if (GDSV6p == new_db_format)
			{	/* Only allow MUPIP UPGRADE for this combination */
				assert(0 == MEMCMP_LIT(command_name, "MUPIP UPGRADE"));
				break;
			}
			INELIGIBLE_UPGRADE(reg, command_name, new_db_format, cur_db_format);
		case GDSV6p:
			if ((GDSV7m == new_db_format) && (GDSV7m == csd->certified_for_upgrade_to))
			{	/* REORG -UPGRADE to GDSV7m only if it is certified */
				assert(0 == MEMCMP_LIT(command_name, "MUPIP REORG -UPGRADE"));
				assert(FALSE == csd->fully_upgraded);
				break;
			} else if ((GDSV6p == new_db_format) && (GDSV7m != csd->certified_for_upgrade_to))
			{	/* US1479880 Repeat an incomplete MUPIP UPGRADE */
				assert(0 == MEMCMP_LIT(command_name, "MUPIP UPGRADE"));
				assert(FALSE == csd->fully_upgraded);
				break;
			}
			INELIGIBLE_UPGRADE(reg, command_name, new_db_format, cur_db_format);
		case GDSV7m:
			if (GDSV7m == new_db_format)
			{	/* REORG -UPGRADE either not run or incomplete */
				assert(0 == MEMCMP_LIT(command_name, "MUPIP REORG -UPGRADE"));
				break;
			}
			INELIGIBLE_UPGRADE(reg, command_name, new_db_format, cur_db_format);
		case GDSV7:
		default:
			INELIGIBLE_UPGRADE(reg, command_name, new_db_format, cur_db_format);
			assert(GDSV7 == cur_db_format);
			break;
	}
	return  SS_NORMAL;
}
