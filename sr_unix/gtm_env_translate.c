/****************************************************************
 *								*
 * Copyright (c) 2006-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"

#include "gtm_limits.h"		/* needed for PATH_MAX */
#include "error.h"
#include "libyottadb.h"		/* needed for ydb_string_t */
#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"
#include "gtm_env_xlate_init.h"
#include "dlopen_handle_array.h"

GBLREF mstr	env_ydb_gbldir_xlate;
GBLREF mstr	env_gtm_env_xlate;
GBLREF mval	dollar_zdir;
LITREF mval	literal_null;

error_def(ERR_TEXT);
error_def(ERR_XTRNRETSTR);
error_def(ERR_XTRNRETVAL);
error_def(ERR_XTRNTRANSDLL);
error_def(ERR_XTRNTRANSERR);

mval *gtm_env_translate(mval *val1, mval *val2, mval *val_xlated)
{
	ydb_string_t	in1, in2, in3, out;
	int		ret_gtm_env_xlate;
	char		pakname[PATH_MAX + 1];
	void_ptr_t	pakhandle;
	MSTR_CONST(routine_name_gtm, GTM_ENV_XLATE_ROUTINE_NAME);
	MSTR_CONST(routine_name_ydb, YDB_ENV_XLATE_ROUTINE_NAME);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 != env_gtm_env_xlate.len)
	{
		MV_FORCE_STR(val2);
		if (NULL == RFPTR(gtm_env_xlate_entry))
		{
			memcpy(pakname, env_gtm_env_xlate.addr, env_gtm_env_xlate.len);
			pakname[env_gtm_env_xlate.len] = '\0';
			pakhandle = fgn_getpak(pakname, ERROR);
			dlopen_handle_array_add(pakhandle);
			/* Looking for ydb_env_xlate then gtm_env_xlate. If neither are found,
			 * we redo the search for ydb_env_xlate and let it throw an appropriate error.
			 */
			SFPTR(gtm_env_xlate_entry, (fgnfnc)fgn_getrtn(pakhandle, &routine_name_ydb, ERROR, FGN_OK_IF_NOT_FOUND));
			if (NULL == RFPTR(gtm_env_xlate_entry))
				SFPTR(gtm_env_xlate_entry, (fgnfnc)fgn_getrtn(pakhandle, &routine_name_gtm, ERROR, FGN_OK_IF_NOT_FOUND));
			if (NULL == RFPTR(gtm_env_xlate_entry))
				SFPTR(gtm_env_xlate_entry, (fgnfnc)fgn_getrtn(pakhandle, &routine_name_ydb, ERROR, FGN_ERROR_IF_NOT_FOUND));
			/* With UTF8 mstr changes, ydb_string_t is no longer compatible with mstr
			 * so explicit copy of len/addr fields required */
		}
		in1.length = val1->str.len;
		in1.address = val1->str.addr;
		in2.length = val2->str.len;
		in2.address = val2->str.addr;
		in3.length = dollar_zdir.str.len;
		in3.address = dollar_zdir.str.addr;
		out.address = NULL;
		ret_gtm_env_xlate = IVFPTR(gtm_env_xlate_entry)(&in1, &in2, &in3, &out);
		val_xlated->str.len = (mstr_len_t)out.length;
		val_xlated->str.addr = out.address;
		if (MAX_DBSTRLEN < val_xlated->str.len)
			RTS_ERROR_ABT(VARLSTCNT(4) ERR_XTRNRETVAL, 2, val_xlated->str.len, MAX_DBSTRLEN);
		if (0 != ret_gtm_env_xlate)
		{
			if ((val_xlated->str.len) && (val_xlated->str.addr))
				RTS_ERROR_ABT(VARLSTCNT(6) ERR_XTRNTRANSERR, 0, ERR_TEXT,  2,
					val_xlated->str.len, val_xlated->str.addr);
			else
				RTS_ERROR_ABT(VARLSTCNT(1) ERR_XTRNTRANSERR);
		}
		if ((NULL == val_xlated->str.addr) && (0 != val_xlated->str.len))
			RTS_ERROR_ABT(VARLSTCNT(1)ERR_XTRNRETSTR);
		val_xlated->mvtype = MV_STR;
		val1 = val_xlated;
	}
	return val1;
}

mval *ydb_gbldir_translate(mval *val1, mval *val_xlated)
{
	ydb_string_t	in1, in2, in3, out;
	int		ret_ydb_gbldir_xlate;
	char		pakname[PATH_MAX + 1];
	void_ptr_t	pakhandle;
	MSTR_CONST(routine_name_ydb, YDB_GBLDIR_XLATE_ROUTINE_NAME);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 != env_ydb_gbldir_xlate.len)
	{
		if (NULL == RFPTR(ydb_gbldir_xlate_entry))
		{
			memcpy(pakname, env_ydb_gbldir_xlate.addr, env_ydb_gbldir_xlate.len);
			pakname[env_ydb_gbldir_xlate.len] = '\0';
			pakhandle = fgn_getpak(pakname, ERROR);
			dlopen_handle_array_add(pakhandle);
			/* Look for ydb_gbldir_xlate. If not found, throw appropriate error. */
			SFPTR(ydb_gbldir_xlate_entry, (fgnfnc)fgn_getrtn(pakhandle, &routine_name_ydb, ERROR, FGN_ERROR_IF_NOT_FOUND));
			/* With UTF8 mstr changes, ydb_string_t is no longer compatible with mstr
			 * so explicit copy of len/addr fields required */
		}
		in1.length = val1->str.len;
		in1.address = val1->str.addr;
		in2.length = literal_null.str.len;
		in2.address = literal_null.str.addr;
		in3.length = dollar_zdir.str.len;
		in3.address = dollar_zdir.str.addr;
		out.address = NULL;
		ret_ydb_gbldir_xlate = IVFPTR(ydb_gbldir_xlate_entry)(&in1, &in2, &in3, &out);
		val_xlated->str.len = (mstr_len_t)out.length;
		val_xlated->str.addr = out.address;
		if (MAX_DBSTRLEN < val_xlated->str.len)
			rts_error(VARLSTCNT(4) ERR_XTRNRETVAL, 2, val_xlated->str.len, MAX_DBSTRLEN);
		if (0 != ret_ydb_gbldir_xlate)
		{
			if ((val_xlated->str.len) && (val_xlated->str.addr))
				rts_error(VARLSTCNT(6) ERR_XTRNTRANSERR, 0, ERR_TEXT,  2,
					  val_xlated->str.len, val_xlated->str.addr);
			else
				rts_error(VARLSTCNT(1) ERR_XTRNTRANSERR);
		}
		if ((NULL == val_xlated->str.addr) && (0 != val_xlated->str.len))
			rts_error(VARLSTCNT(1)ERR_XTRNRETSTR);
		val_xlated->mvtype = MV_STR;
		val1 = val_xlated;
	}
	return val1;
}
