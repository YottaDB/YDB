/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_CRE_FILE_INCLUDED
#define MU_CRE_FILE_INCLUDED

#include "gdsfhead.h"
#include "gtm_common_defs.h"
#include "filestruct.h"
#include "gtmimagename.h"
#include "mdef.h"
#include <mdefsp.h>
#include "gtmio.h"
#include "io.h"
#include "db_header_conversion.h"
#include "get_fs_block_size.h"

error_def(ERR_DBFILERR);
error_def(ERR_DBCREINCOMP);
error_def(ERR_DBNOTGDS);
error_def(ERR_BADDBVER);
error_def(ERR_STATSDBNOTSUPP);
error_def(ERR_INVSTATSDB);
error_def(ERR_STATSDBINUSE);
error_def(ERR_DBBLKSIZEALIGN);
GBLREF uint4 process_id;

/* Macros to send warning or error messages to the correct destination:
 *  - If MUPIP image, message goes to stderr of the process.
 *  - Else MUMPS image captures the error message and wraps it with MUCREFILERR and sends it to the system log.
 * In addition, some messages require cleanup if they are emitted past a certain point in the processing (said point
 * setting the 'cleanup_needed' flag to TRUE.
 */

#define PUTMSG_MSG_ROUTER_CSA(CSAARG, REG, VARCNT, ERRORID, ...)							\
MBSTART {														\
	mval		zpos;												\
															\
	if (IS_MUPIP_IMAGE)												\
		gtm_putmsg_csa(CSA_ARG(CSAARG) VARLSTCNT(VARCNT) ERRORID, __VA_ARGS__);					\
	else														\
	{														\
		/* Need to reflect the current error to the syslog - find entry ref to add to error. The VARLSTCNT	\
		 * computation is 8 for the prefix message, plus the VARLSTCNT() that would apply to the actual error	\
		 * message that got us here.										\
		 */													\
		getzposition(&zpos);											\
		send_msg_csa(CSA_ARG(CSAARG) VARLSTCNT((8 + VARCNT)) ERR_MUCREFILERR, 6, zpos.str.len, zpos.str.addr,	\
				DB_LEN_STR(REG), REG_LEN_STR(REG), ERRORID, __VA_ARGS__);				\
	}														\
} MBEND


unsigned char mu_cre_file(gd_region *reg);
unsigned char mu_init_file(gd_region *reg, boolean_t has_ftok);

enum db_validity
{
	DB_VALID,
	DB_VALID_DBGLDMISMATCH,
	DB_INVALID,
	DB_UNKNOWN_SEMERR,
	DB_INVALID_SHORT,
	DB_INVALID_CREATEINPROGRESS,
	DB_INVALID_NOMATCH,
	DB_POTENTIALLY_VALID,
	/* Anything < preceding enum is potentially valid stage of creation and initialization, even if currently invalid */
	DB_READERR,
	DB_OPNERR,
	DB_INVALID_NOTGDS,
	DB_INVALID_BADDBVER,
	DB_INVALID_STATSDBNOTSUPP,
	DB_INVALID_INVSTATSDB,
	DB_INVALID_NOTOURSTATSDB,
	DB_INVALID_BLKSIZEALIGN,
	DB_INVALID_ENDIAN,
	COUNT_DB_VALIDITY_ENUMS
};
#ifdef DEBUG_RESERVEDDB
static const char *const db_validity_names[COUNT_DB_VALIDITY_ENUMS] = {
	[DB_VALID] = "DB_VALID",
	[DB_VALID_DBGLDMISMATCH] = "DB_VALID_DBGLDMISMATCH",
	[DB_INVALID] = "DB_INVALID",
	[DB_UNKNOWN_SEMERR] = "DB_UNKNOWN_SEMERR",
	[DB_INVALID_SHORT] = "DB_INVALID_SHORT",
	[DB_INVALID_CREATEINPROGRESS] = "DB_INVALID_CREATEINPROGRESS",
	[DB_POTENTIALLY_VALID] = "DB_POTENTIALLY_VALID",
	[DB_READERR] = "DB_READERR",
	[DB_OPNERR] = "DB_OPNERR",
	[DB_INVALID_NOTGDS] = "DB_INVALID_NOTGDS",
	[DB_INVALID_BADDBVER] = "DB_INVALID_BADDBVER",
	[DB_INVALID_STATSDBNOTSUPP] = "DB_INVALID_STATSDBNOTSUPP",
	[DB_INVALID_INVSTATSDB] = "DB_INVALID_INVSTATSDB",
	[DB_INVALID_NOTOURSTATSDB] = "DB_INVALID_NOTOURSTATSDB",
	[DB_INVALID_BLKSIZEALIGN] = "DB_INVALID_BLKSIZEALIGN",
	[DB_INVALID_ENDIAN] = "DB_INVALID_ENDIAN"
};
#endif

/* A standard error handler for results of read_db_file_header which RTS_ERRORs with appropriate values.
 * Caller should call only if value is appropriate to RTS_ERROR. This function also disables stats on a given parent region if
 * the error occurs on a statsdb region as a standard statsdb error handling response.
 */
static inline void error_on_db_invalidity(sgmnt_addrs *csa, gd_region *reg, sgmnt_data_ptr_t tsd, enum db_validity validity,
		int save_errno)
{
	gd_region *baseDBreg;
	sgmnt_addrs *baseDBcsa;
	uint4 fsb_size;
	unix_db_info *udi = NULL;

	if (IS_STATSDB_REG(reg))
	{
		STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
		baseDBcsa = REG2CSA(baseDBreg);
		baseDBreg->reservedDBFlags |= RDBF_NOSTATS;
		baseDBcsa->reservedDBFlags |= RDBF_NOSTATS;
	}
	switch (validity)
	{
		case DB_INVALID_SHORT:
		case DB_INVALID_CREATEINPROGRESS:
			assert(!IS_AUTODB_REG(reg)); /* Technically possible with enough loops and a race, but surpassingly rare */
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(4) ERR_DBCREINCOMP, 2, DB_LEN_STR(reg));
		case DB_OPNERR:
		case DB_READERR:
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
		case DB_INVALID_NOTGDS:
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(4) ERR_DBNOTGDS, 2, DB_LEN_STR(reg));
		case DB_INVALID_BADDBVER:
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(4) ERR_BADDBVER, 2, DB_LEN_STR(reg));
		case DB_INVALID_STATSDBNOTSUPP:
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(4) ERR_STATSDBNOTSUPP, 2, DB_LEN_STR(reg));
		case DB_INVALID_INVSTATSDB:
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(6) ERR_INVSTATSDB, 4, DB_LEN_STR(reg), REG_LEN_STR(reg));
		case DB_INVALID_NOTOURSTATSDB:
			STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(8) ERR_STATSDBINUSE, 6, DB_LEN_STR(reg), tsd->basedb_fname_len,
					tsd->basedb_fname, DB_LEN_STR(baseDBreg));
		case DB_INVALID_BLKSIZEALIGN:
			if (reg->dyn.addr->file_cntl)
				udi = FILE_INFO(reg);
			assert(udi);
			fsb_size = get_fs_block_size(udi->fd);
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(6) ERR_DBBLKSIZEALIGN, 4, DB_LEN_STR(reg), tsd->blk_size,
					fsb_size);
		case DB_INVALID_ENDIAN:
			RTS_ERROR_CSA_ABT(CSA_ARG(csa) VARLSTCNT(6) ERR_DBENDIAN, 4, DB_LEN_STR(reg), ENDIANOTHER, ENDIANTHIS);
		default:
			assert(FALSE);
	}
	assert(FALSE);
}

/* Reads the db file header and returns an enum indicating its validity state. Caller MUST HANDLE every possible return
 * value. Caller may choose to RTS_ERROR on some errors or otherwise handle depending on place in code; see standard
 * error handler above.
 */
static inline enum db_validity read_db_file_header(unix_db_info *udi, gd_region *reg, sgmnt_data_ptr_t tsd)
{
	size_t read_len = 0;
	int status = 0;
	enum db_validity rc = DB_VALID;
	uint4 fsb_size;
	endian32_struct check_endian;

	memset(tsd, 0, sizeof(*tsd));
	DBG_CHECK_DIO_ALIGNMENT(udi, 0, tsd, SIZEOF(sgmnt_data));
	LSEEKREAD_AVAILABLE(udi->fd, 0, tsd, SIZEOF(sgmnt_data), read_len, status);
	if (read_len != SIZEOF(sgmnt_data))
		rc = (-1 == status) ? DB_INVALID_SHORT : DB_READERR;
	if ((read_len != 0) && (rc != DB_READERR))
	{
		/* File has size > 0. Do de minimis validation that it is a valid db file at some stage of the creation and
		 * initialization process (potentially interrupted by a kill -9).
		 */
		if (0 != memcmp(tsd->label, GDS_LABEL, MIN(GDS_LABEL_SZ - 3, read_len)))
			rc = DB_INVALID_NOTGDS;
		else if ((0 != memcmp(tsd->label, GDS_LABEL, MIN(strlen(GDS_LABEL), read_len)))
				&& (0 != memcmp(tsd->label, V6_GDS_LABEL, MIN(strlen(V6_GDS_LABEL), read_len))))
			rc = DB_INVALID_BADDBVER;
		else
		{
			if (0 == memcmp(tsd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
				db_header_upconv(tsd);
			check_endian.word32 = tsd->minor_dbver;
			if (((OFFSETOF(sgmnt_data, minor_dbver) + SIZEOF(tsd->minor_dbver)) <= read_len)
					&& !check_endian.shorts.ENDIANCHECKTHIS)
				rc = DB_INVALID_ENDIAN;
			else if (CREATE_IN_PROGRESS(tsd))
				rc = DB_INVALID_CREATEINPROGRESS;
			else if (((OFFSETOF(sgmnt_data, reservedDBFlags) + SIZEOF(tsd->reservedDBFlags)) <= read_len))
			{
				if (IS_RDBF_STATSDB(tsd) && !IS_STATSDB_REGNAME(reg))
					rc = DB_INVALID_STATSDBNOTSUPP;
				else if (!IS_RDBF_STATSDB(tsd) && IS_STATSDB_REGNAME(reg))
					rc = DB_INVALID_INVSTATSDB;
			}
		}
	}
	assert(!status || (rc != DB_VALID));
	if (DB_READERR == rc)
		errno = status;
	else if (DB_VALID == rc)
	{
		if (IS_AIO_DBGLDMISMATCH((reg->dyn.addr), tsd))
		{
			COPY_AIO_SETTINGS(reg->dyn.addr, tsd);
			rc = DB_VALID_DBGLDMISMATCH;
		}
		if (tsd->asyncio)
		{
			fsb_size = get_fs_block_size(udi->fd);
			if (0 != (tsd->blk_size % fsb_size))
				rc = DB_INVALID_BLKSIZEALIGN;
		}
	}
	reg->file_initialized = !rc;

	DBGRDB((stderr, "%s:%d:%s: process id %d returning %s after read_db_file_header of file %s for region %s\n",
				__FILE__, __LINE__, __func__, process_id, db_validity_names[rc], reg->dyn.addr->fname, reg->rname));
	return rc;
}
#endif /* MU_CRE_FILE_INCLUDED */
